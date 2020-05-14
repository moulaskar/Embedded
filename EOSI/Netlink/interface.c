/* -----------------------------------------------  hcsr misc driver --------------------------------------------------
 netlink driver registartion and provides netlink interaction to userspace configuration. 
 ----------------------------------------------------------------------------------------------------------------*/

#include <linux/fs.h>
#include <linux/slab.h>
#include<linux/miscdevice.h>
#include<linux/gpio.h>
#include<linux/irq.h>
#include<linux/interrupt.h>
#include<linux/spinlock_types.h>
#include<linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/cdev.h>
 #include <net/genetlink.h>
//#include "../hcsr_ioctl.h"



#define NLSUPPORT 1


#ifdef NLSUPPORT

#include "genl_ex.h"

#endif


#define GPIO_INTERRUPT "gpio_interrupt"

typedef struct {
   int triggerPin;
   int echoPin;
   int ledPin;
}gpioPinSetting;

typedef struct {
   int frequency; /* number of samples per measurement */
   int samplingPeriod; /* period for each sample or the sampling period */
}gpioParSetting;


#define MAX_DEVICES  1                      // Maximum device instances of this module,overrides module parameter to keep boundary check.

#define MAX_ENTRIES 10
#define MAX_MUX
//#define DEBUG 1  
#if defined(DEBUG)
    //#define DPRINTK(fmt,args...) printk("DEBUG: %s %s() %d: \n"fmt,__FILE__,__func__,__LINE__,##args);
    #define DPRINTK(fmt,args...) printk("DEBUG: %s() %d: \n"fmt,__func__,__LINE__,##args);
#else
   #define DPRINTK(fmt,args...)
#endif

/* Enum for Shield IO PIN */
enum SHIELD_PIN {
   IO00 = 0,
   IO01 = 1,
   IO02 = 2,
   IO03 = 3,
   IO04 = 4,
   IO05 = 5,
   IO06 = 6,
   IO07 = 7,
   IO08 = 8,
   IO09 = 9,
   IO10 = 10,
   IO11 = 11,
   IO12 = 12,
   IO13 = 13,
   /*
   //Using till 13
   IO14 = 14,
   IO15 = 15,
   IO16 = 16,
   IO17 = 17,
   IO18 = 18,
   IO19 = 19
   */
   
};

/*****************************************************/
/*********SPI DETAILS*********************************/
#define SPI_SPEED 10000000
#define BITS_PER_WORD 16
uint16_t pattern[6][8] =      {
                                 {0x0100,0x0200,0x0300,0x0420,0x0570,0x0620,0x0700,0x0800},
                                 {0x0100,0x0200,0x0300,0x0408,0x051C,0x0608,0x0700,0x0800},
                                 {0x0100,0x0200,0x0300,0x0402,0x0507,0x0602,0x0700,0x0800},
                                 {0x0100,0x0200,0x0300,0x0408,0x051C,0x0608,0x0700,0x0800},
                                 {0x0100,0x0200,0x0300,0x0420,0x0570,0x0620,0x0700,0x0800},
                                 {0x0142,0x0225,0x031A,0x04FC,0x053C,0x065A,0x07A5,0x0842}
};

int sequence[14] = {0,500,1,500,2,500,3,500,4,500,5,500,0,0};
#ifdef NLSUPPORT

static void greet_group(unsigned int group, int distance);

#endif

uint16_t buff[1];
uint16_t configData[5]={0x0C01,0x0900,0x0A0F,0x0B07,0x0F00};
uint16_t noDisplay[8] = {0x0100,0x0200,0x0300,0x0400,0x0500,0x0600,0x0700,0x0800};
static struct spi_message mess;
struct spi_master *master=NULL;
struct spi_device *spi_dev = NULL;
struct device *pdev;
struct spi_transfer tx = {

	.tx_buf = buff,         // Transmission Buffer
	.rx_buf = 0,                // setting receiving buffer as zero to have Half-Duplex Operation
	.len = 2,                   // No of Bytes
	.bits_per_word = BITS_PER_WORD,        // Bits per Word
	.speed_hz = SPI_SPEED,       // Speed of Transfer
	.delay_usecs = 1,           // Delay between Transfers
};

int set_max7219_pins(void);
/***********************************************************/

/* used to store information */
struct user_hcsr_data{
  int distance;
  uint64_t tsc;
};

static int max_device =  MAX_DEVICES;

//int muxping[MAX_MUX]


struct hcsr_dev {
	struct miscdevice miscDev;
	int status;
	struct semaphore readBlock; /* to block a read operation if device is empty */
        int delta;       // frequency in milisecond to calculate frequency
        int m;           // (m + 2)distance samples in every delta milisecond
	int dev_num;
        int writeIndex;                  //writeIndex for the hcsr04_object_info
        int maxEntries;                  //notes down the current entries in user_data
	struct user_hcsr_data user_data[MAX_ENTRIES];
	int thread_state;
	int trigger_pin;
	int echo_pin;
	int led_pin;
	spinlock_t m_Lock;
	struct task_struct *task;
        uint64_t tsc1;
        uint64_t tsc2;
        short irq_status;
};


static struct hcsr_dev *pchsrChipdev=NULL; /* should use MAX_DEVICE */

/* function prototypes */
static void start_measurement(struct hcsr_dev *phcsr);
static void resetirq(struct hcsr_dev *phcsrdev);
static int setirq(int pin, struct hcsr_dev *phcsrdev);
int calculate_distance(struct user_hcsr_data *arr,  int writeCurrIndex, int maxEntry, int m);
void set_mux(int pin, int mux1, int mux2, int high);

#if defined(__i386__)
static __inline__ unsigned long long get_tsc(void)

{
	unsigned long long int x;
     	__asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     	return x;
}

#elif defined(__x86_64__)
static __inline__ unsigned long long get_tsc(void)

{
	unsigned hi, lo;
  	__asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  	return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 ;
}
#endif


/* send pattern to led matrx*/

int pass_pattern(uint16_t passPattern[], int length){

    int i;
    for(i = 0;i < length; i++){
        //printk("pass pattern = %d\n",passPattern[i]);
        buff[0]=passPattern[i];
        gpio_set_value(15,0);
        
        spi_message_init(&mess);
        spi_message_add_tail((void *)&tx, &mess);
        gpio_set_value(15,0);
        if(spi_sync(spi_dev, &mess) < 0){
            printk("\nError in sending SPI message.\n");
            return -1;
        }
        gpio_set_value(15,1);
	}
	return 0;
}

/*light_up_matrix(void)
  This function chooses the pattern to display
*/
void light_up_matrix(void)
{ 
    int i;
    for(i = 0; i < ARRAY_SIZE(sequence); i+=2){
        // Sending NO DISPLAY data at the end of the sequence
        if(sequence[i] == 0 && sequence[i+1] == 0){
            pass_pattern(noDisplay,ARRAY_SIZE(noDisplay));
            break;
        }
        pass_pattern(pattern[sequence[i]],ARRAY_SIZE(pattern[sequence[i]]));

         // Sending the required SPI pattern
         msleep(sequence[i+1]);
    }
    return;

    
}

/*
* static int measurement_thread_function(void *handle) 
* Func: Threads executes for m+2 times with a frequency of delta ms 
* @param: device structure - struct hcsr_dev
* 
*/
static int measurement_thread_function(void *handle)
{
    int counter = 0;
    struct hcsr_dev *phcsrdev = NULL;
    int distance=0;
    phcsrdev = (struct hcsr_dev *)handle;
    DPRINTK("start measurement %s Line = %d\n",phcsrdev->miscDev.name,__LINE__);

    while(counter <  (phcsrdev->m+2))
    { 
        gpio_set_value_cansleep(phcsrdev->trigger_pin, 0);
        mdelay(10);
        gpio_set_value_cansleep(phcsrdev->trigger_pin, 1);
        /* As per HC-SR04 datasheet, the trigger pulse is high for 10usec*/
        udelay(10);
        gpio_set_value_cansleep(phcsrdev->trigger_pin, 0);  
        mdelay(phcsrdev->delta); 
        counter++;
    }                                            
    phcsrdev->thread_state = 0;

    distance =  calculate_distance(phcsrdev->user_data, phcsrdev->writeIndex-1, phcsrdev->maxEntries, phcsrdev->m);
    memset(phcsrdev->user_data, 0, MAX_ENTRIES*sizeof(struct user_hcsr_data));
    phcsrdev->writeIndex = 0;
    phcsrdev->maxEntries = 0;
    DPRINTK("Avg Distance =%d\n",distance);
    printk("Avg Distance =%d\n",distance);
    /* send async nl message to user-space */
#ifdef NLSUPPORT
    greet_group(GENL_TEST_MCGRP0, distance);
#endif    
    if(phcsrdev->status == 1) 
    {
       phcsrdev->status = 0;
       up(&phcsrdev->readBlock);
    }
    return 0;
}
    


/* static void start_measurement(struct hcsr_dev *phcsr)
 * Starts the measurement
*/
static void start_measurement(struct hcsr_dev *phcsr)
{
   
    phcsr->task = kthread_run(&measurement_thread_function,(void *)phcsr, "sampling");
    phcsr->thread_state = 1;

}


/*
* int calculate_distance(struct user_hcsr_data *arr,  int writeCurrIndex, int maxEntry)
* Calculate the average distance for m readings
*/
int calculate_distance(struct user_hcsr_data *arr,  int writeCurrIndex, int maxEntry, int m)
{
     unsigned int sum = 0;
     unsigned int average = 0;
     int i = 0;


     if(maxEntry == 0)
     {
          DPRINTK("No entry in buffer, returning 0\n");
          return 0;
     }
     // we have less number of data than user asked for, incase of missing interrupts,or when interrupt down is not followed by interrupt up
     // calculate average of all we have
     if(maxEntry < (m + 2))   //maxEntry is restricted to MAX_ENTRIES 
     {
         for(i = 0;i < maxEntry; i++)
         {
             DPRINTK("1: arr[%d].distrance = %d\t",i, arr[i].distance);
             sum = sum + arr[i].distance;
         }
         average = sum/maxEntry;
     }
     else
     {
         //for(i = 1 ;i < MAX_ENTRIES - 1; i++) /* skipping the fist and last entry*/
         for(i = 1 ;i < maxEntry - 1; i++) /* skipping the fist and last entry*/
         {
             DPRINTK("2: arr[%d].distrance = %d\t",i, arr[i].distance);
             sum = sum + arr[i].distance;
         }
         average = sum/(maxEntry - 2);
     }
     printk("\n");
     
     return average;

}

/*
* static irqreturn_t irq_handler(int irq, void *dev_id)
* IRQ Handler
* Called in rising and falling edge of Echo Pin
*
*/

static irqreturn_t irq_handler(int irq, void *dev_id)
{
    
    struct hcsr_dev *phcsrdev = (struct hcsr_dev *)dev_id;
    unsigned long flags;
    unsigned int temp;
    unsigned int distance;
    int gpioVal;

    DPRINTK("FUNC %s  LINE = %d name = %s\n",__func__, __LINE__,phcsrdev->miscDev.name);

    gpioVal = gpio_get_value(phcsrdev->echo_pin);
    if(irq == gpio_to_irq(phcsrdev->echo_pin))
    {
  
        if(gpioVal == 0)
        {
            DPRINTK("low \n");
            if(phcsrdev->tsc1 == 0) /* spurious interrupt, haven't seen the high but only low */
            {
                printk("Sometimes we get spurious interrupt\n");
                irq_set_irq_type(irq, IRQF_TRIGGER_RISING);
                return IRQ_NONE; 
            }
            phcsrdev->tsc2 = get_tsc();
            temp = phcsrdev->tsc2 - phcsrdev->tsc1;
            temp = (temp * 34)/2; /* to avoid overflow took away 10^3 from 34000 and 399076(cpu clock) */
            distance = temp/399076;

	    spin_lock_irqsave(&(phcsrdev->m_Lock), flags);
            phcsrdev->user_data[phcsrdev->writeIndex % MAX_ENTRIES].tsc = phcsrdev->tsc2;
            phcsrdev->user_data[phcsrdev->writeIndex % MAX_ENTRIES].distance = distance;
            phcsrdev->writeIndex++;
            if(phcsrdev->maxEntries < MAX_ENTRIES)
               phcsrdev->maxEntries++;
             
            irq_set_irq_type(irq, IRQF_TRIGGER_RISING);
            phcsrdev->tsc2 = phcsrdev->tsc1 = 0; 
            spin_unlock_irqrestore(&(phcsrdev->m_Lock), flags);
        }
        else if(gpioVal == 1) 
        {
            DPRINTK("hi\n");
            phcsrdev->tsc1 = get_tsc();
            irq_set_irq_type(irq, IRQF_TRIGGER_FALLING);
        }
        else
        {
            return IRQ_NONE;
        }
    }
    return IRQ_HANDLED;
}



/*
* int setpin(int shieldPin, int direction, int value, int *mux_1, int *mux_2, int *level_s)
* Validates and maps the Shield Pin with gpio signals
* mux1, mux2 and levelshifter is pass by reference to be set in IOCTL
* Return Linux Gpio Signal
*/

int setpin(int shieldPin, int direction, int value, int *mux_1, int *mux_2, int *level_s)
{
    int gpioNo = -1;
    int mux1 = -1;
    int mux2 = -1;
    int levelShifter = -1;
    int high = 0;   //default is low
    int ret = 0;
    int pin = -1;
   
    
    switch(shieldPin)
    {
        case IO00: gpioNo = 11;
                   levelShifter = 32;
                   break;

        case IO01: gpioNo = 12;
                   levelShifter = 28;
                   mux1 = 45;
                   break;

        case IO02: gpioNo = 13;
                   levelShifter = 34;
                   mux1 = 77;
                   break;

        case IO03: gpioNo = 14; //ignoring gpio61 as only one connection can be made on HCSR04 at a time
                   levelShifter = 16;
                   mux1 = 76;
                   mux2 = 64;
                   break;

        case IO04: gpioNo = 6;
                   levelShifter = 36;
                   break;

        case IO05: gpioNo = 0;
                   levelShifter = 18;
                   mux1 = 66;
                   break;

        case IO06: gpioNo = 1;
                   levelShifter = 20;
                   mux1 = 68;
                   break;

        case IO07: gpioNo = 38;
                   break;

        case IO08: gpioNo = 40;
                   break;

        case IO09: gpioNo = 4;
                   levelShifter = 22;
                   mux1 = 70;
                   break;

        case IO10: gpioNo = 10;
                   levelShifter = 26;
                   mux1 = 74;
                   break;

        /*case IO11: gpioNo = 5;
                   levelShifter = 24;
                   mux1 = 44;
                   mux2 = 72;
                   break;*/

        case IO12: gpioNo = 15;
                   levelShifter = 42;
                   break;

       /* case IO13: gpioNo = 7;
                   levelShifter = 30;
                   mux1 = 46;
                   break;
        
        case IO14: gpioNo = 48;
                   break;

        case IO15: gpioNo = 50;
                   break;

        case IO16: gpioNo = 52;
                   break;

        case IO17: gpioNo = 54;
                   break;

        case IO18: gpioNo = 56;
                   mux1 = 60;
                   mux2 = 78;
                   high = 1;
                   break;

        case IO19: gpioNo = 58;
                   mux1 = 60;
                   mux2 = 79;
                   high = 1;
                   break;
        */ 
        default:  if(shieldPin == 11 || shieldPin == 13)
                      printk("reserved for led: try other pin\n");
                  else
                      printk("Not a valid shield Pin\n");
                  return -EINVAL;
    }
    pin = gpioNo;
    gpio_request(pin, "sysfs");
    if(direction == 0)   //Trigger Pin
    {
        ret = gpio_direction_output(pin, value);  
        if( ret < 0)
            return ret;
        gpio_set_value_cansleep(pin,0);
        //printk("GPIO LED  %d\n",pin);
        set_mux(pin, mux1, mux2, high);
        if(levelShifter > -1)
        {
            ret = gpio_direction_output(levelShifter, value);  
            if( ret < 0)
                return ret;
            gpio_set_value_cansleep(levelShifter,0);
        } 
       
    }
    else if(direction == 1)   //Echo Pin
    {
        if((pin == 38) || (pin == 40))
        {
            printk("Not a valid Echo Pin\n");
            return -EINVAL;
        }
        gpio_direction_input(pin);
        set_mux(pin, mux1, mux2, high);

        if(levelShifter > -1)
        {
            gpio_direction_input(levelShifter);
        }
    } 

    *mux_1 = mux1;
    *mux_2 = mux2;
    *level_s = levelShifter;

    DPRINTK("SPin = %d gpio = %d mux1 = %d mux2 = %d  levelShifter = %d high = %d\n",shieldPin ,gpioNo,  mux1,mux2,levelShifter,high);
    return gpioNo;
}

/*
* void set_mux(int pin, int mux1, int mux2, int high)
* sets the mux values associated with the gpio trigger andecho pins 
*/
void set_mux(int pin, int mux1, int mux2, int high)
{
    
    gpio_request(pin, "sysfs");
    DPRINTK("Mux1 = %d  mux2 = %d high = %d line = %d\n",mux1, mux2,high,__LINE__);
    if(mux1 > -1)
    {
        gpio_set_value_cansleep(mux1, 0);
    } 
    if(mux2 > -1)
    {
        gpio_set_value_cansleep(mux2, high);
    } 
}

/*int set_max7219_pins(void)
 * sets the MOSI and CLK pins for MAX7219*/

int set_max7219_pins(void)
{
    //Configuring pin 13 CLK
    gpio_direction_output(30,1);
    gpio_set_value_cansleep(30,0);
    gpio_direction_output(46,1);
    gpio_set_value_cansleep(46,1);

    // Configuring IO 11 as SPI_MOSI
    gpio_direction_output(24,1);
    gpio_set_value_cansleep(24,0);

    gpio_direction_output(44,1);
    gpio_set_value_cansleep(44,1);
    gpio_set_value_cansleep(72,0);

    // Sending the configuration data for Normal operation of the LED Display
    pass_pattern(configData, ARRAY_SIZE(configData));

   
   return 0;
}


/*
 * static int setirq(int pin, struct hcsr_dev *phcsrdev)
 * Requests IRQ for the rising edge for Echo Pin
*/
static int setirq(int pin, struct hcsr_dev *phcsrdev)
{
   int gpio_irq = 0;
   if ((gpio_irq = gpio_to_irq(pin)) < 0)
   {
       DPRINTK("gpio_to_irq mapping failure \n");
       return -1;
   }
   if (request_irq(gpio_irq, (irq_handler_t ) irq_handler, IRQF_TRIGGER_RISING , GPIO_INTERRUPT, (void*)phcsrdev))
   {
       return -1;
   }
   phcsrdev->irq_status = 1;

   DPRINTK("IRQ handler registered \n");
   return 0;
}


/*
 * void resetirq(struct hcsr_dev *phcsrdev)
 * Frees the IRQ
*/
void resetirq(struct hcsr_dev *phcsrdev)
{
   DPRINTK("IRQ handler freed \n");
   if(phcsrdev->irq_status)
       free_irq(gpio_to_irq(phcsrdev->echo_pin), (void*)phcsrdev);
   phcsrdev->irq_status = 0;
}



#ifdef NLSUPPORT
/*static int genl_test_rx_pin(struct sk_buff* skb, struct genl_info* info)
 * The function is interface where Configuration pins are set which the user sends*/
 
static int genl_test_rx_pin(struct sk_buff* skb, struct genl_info* info)
{

        int gpioTrigger = -1;
        int gpioEcho = -1;
        int gpioLed = -1;
        int mux_1 = -1;
        int mux_2 = -1;
        int level_S = -1;
        int temp1, temp2, temp3;
        int ret = 0;
        int devid = 0;
        

        DPRINTK("%s   %d\n",__func__,__LINE__);
	if (!info->attrs[GENL_TEST_ATTR_MSG]) 
        {
		printk(KERN_ERR "empty message from %d!!\n",
			info->snd_portid);
		printk(KERN_ERR "%p\n", info->attrs[GENL_TEST_ATTR_MSG]);
		return -EINVAL;
	}

        DPRINTK("%s   Received Pin Configuration\n",__func__);
        /* need to call the gpio init function called also from ioctl */
           devid = *(int *)nla_data(info->attrs[GENL_TEST_ATTR_DEVID]); 
           if(devid > max_device || devid < 0)
               devid = max_device - 1;
           
           temp1 = *(int *)nla_data(info->attrs[GENL_TEST_ATTR_GPIO_TRIGGER]);
           temp2 = *(int *)nla_data(info->attrs[GENL_TEST_ATTR_GPIO_ECHO]);
           temp3 = *(int *)nla_data(info->attrs[GENL_TEST_ATTR_GPIO_LED]);
           /*set the led pins*/
           gpioTrigger =  setpin(temp1, 0, 0, &mux_1, &mux_2, &level_S);  
           if(gpioTrigger < 0) {
               printk("Trigger Pin has problem\n");
               return -EINVAL;
           }
           gpioLed =  setpin(temp3, 0, 1, &mux_1, &mux_2, &level_S);  
           if(gpioLed < 0) {
               printk("Led Pin has problem\n");
               return -EINVAL;
           }

           gpioEcho = setpin(temp2, 1, 0, &mux_1, &mux_2, &level_S);  
           if(gpioEcho < 0) {
               printk("ECHO Pin has problem\n");
               return -EINVAL;
           }

           pchsrChipdev[devid].trigger_pin = gpioTrigger;
           pchsrChipdev[devid].echo_pin = gpioEcho;
           pchsrChipdev[devid].led_pin = gpioLed;
           ret = setirq(gpioEcho, &pchsrChipdev[devid]);
           if(ret < 0)
           {
               return -EINVAL;
           }
           set_max7219_pins();
           pchsrChipdev[devid].m = *(int *)nla_data(info->attrs[GENL_TEST_ATTR_FREQ]); 
           pchsrChipdev[devid].delta = *(int *)nla_data(info->attrs[GENL_TEST_ATTR_SAMPLE]);
           if(pchsrChipdev[devid].delta < 60)
                 pchsrChipdev[devid].delta = 100;
           return 0;
}


/* function to set speed */
static int genl_test_rx_pattern(struct sk_buff* skb, struct genl_info* info)
{
        int i;
        int temp;
        DPRINTK("%s   %d\n",__func__,__LINE__);
	if (!info->attrs[GENL_TEST_ATTR_MSG]) {
		printk(KERN_ERR "empty message from %d!!\n",
			info->snd_portid);
		printk(KERN_ERR "%p\n", info->attrs[GENL_TEST_ATTR_MSG]);
		return -EINVAL;
	}
           
       
        temp = sizeof(sequence)/sizeof(int);
        for(i=0; i<temp; i++)
            sequence[i] = *(int *)nla_data(info->attrs[GENL_TEST_ATTR_ANI1 + i]);

        light_up_matrix();
        /* need to call the SPI function to send speed to LED device */
	return 0;
}



/*static int genl_test_rx_speed(struct sk_buff* skb, struct genl_info* info)
this function is to start the distance measurement */

static int genl_test_rx_speed(struct sk_buff* skb, struct genl_info* info)
{
        int devid;
        DPRINTK("%s   Received Pin Configuration\n",__func__);
	if (!info->attrs[GENL_TEST_ATTR_DEVID]) {
		printk(KERN_ERR "empty message from %d!!\n",
			info->snd_portid);
		printk(KERN_ERR "%p\n", info->attrs[GENL_TEST_ATTR_DEVID]);
		return -EINVAL;
	}
        devid = *(int *)nla_data(info->attrs[GENL_TEST_ATTR_DEVID]); 
        if(devid > max_device || devid < 0)
            devid = max_device;
        /* need to call the SPI function to send speed to LED device */
        start_measurement(&pchsrChipdev[devid]);
	return 0;
}


/* static int genl_test_cleanup(struct sk_buff* skb, struct genl_info* info)
 This function is to clean up gpios*/

static int genl_test_cleanup(struct sk_buff* skb, struct genl_info* info)
{

        int devid = 0;

        DPRINTK("%s   %d\n",__func__,__LINE__);
	if (!info->attrs[GENL_TEST_ATTR_MSG]) 
        {
		printk(KERN_ERR "empty message from %d!!\n",
			info->snd_portid);
		printk(KERN_ERR "%p\n", info->attrs[GENL_TEST_ATTR_MSG]);
		return -EINVAL;
	}

        /* need to call the gpio init function called also from ioctl */
           devid = *(int *)nla_data(info->attrs[GENL_TEST_ATTR_DEVID]); 
           if(devid > max_device || devid < 0)
               devid = max_device;

        if(pchsrChipdev[devid].thread_state == 1)
            kthread_stop(pchsrChipdev[devid].task);
        resetirq(&pchsrChipdev[devid]);
           
        gpio_free(pchsrChipdev[devid].echo_pin);
        gpio_free(pchsrChipdev[devid].trigger_pin);
        gpio_free(pchsrChipdev[devid].led_pin);
        pass_pattern(noDisplay,ARRAY_SIZE(noDisplay));
	DPRINTK("%s is closed successfully\n", pchsrChipdev[devid].miscDev.name);
        return 0;
}


static const struct genl_ops genl_test_ops[] = {
	{
		.cmd = GENL_TEST_C_MSG,
		.policy = genl_test_policy,
		.doit = genl_test_rx_pin,
		.dumpit = NULL,
	},
	{
		.cmd = HCSR_SET_PATTERN_1,
		.policy = genl_test_policy,
		.doit = genl_test_rx_pattern,
		.dumpit = NULL,
	},
	{
		.cmd = HCSR_GET_SPEED,
		.policy = genl_test_policy,
		.doit = genl_test_rx_speed,
		.dumpit = NULL,
	},
	{
		.cmd = HCSR_CLEANUP,
		.policy = genl_test_policy,
		.doit = genl_test_cleanup,
		.dumpit = NULL,
        },
};

static const struct genl_multicast_group genl_test_mcgrps[] = {
	[GENL_TEST_MCGRP0] = { .name = GENL_TEST_MCGRP0_NAME, },
};



static struct genl_family genl_test_family = {
	.name = GENL_TEST_FAMILY_NAME,
	.version = 1,
	.maxattr = GENL_TEST_ATTR_MAX,
	.netnsok = false,
	.module = THIS_MODULE,
	.ops = genl_test_ops,
	.n_ops = ARRAY_SIZE(genl_test_ops),
	.mcgrps = genl_test_mcgrps,
	.n_mcgrps = ARRAY_SIZE(genl_test_mcgrps),
};


/*static void greet_group(unsigned int group, int distance)
  This function is the interface to user space*/

static void greet_group(unsigned int group, int distance)
{	
	void *hdr;
	int res, flags = GFP_ATOMIC;
	struct sk_buff* skb = genlmsg_new(NLMSG_DEFAULT_SIZE, flags);

	if (!skb) {
		printk(KERN_ERR "%d: sk_buff failed!!", __LINE__);
		return;
	}
	hdr = genlmsg_put(skb, 0, 0, &genl_test_family, flags, GENL_TEST_C_MSG);
	if (!hdr) {
		printk(KERN_ERR "%d: Unknown err !", __LINE__);
		goto nlmsg_fail;
	}
        res = nla_put_u32(skb, HCSR_GET_SPEED, distance);
	if (res) {
		printk(KERN_ERR "%d: err %d ", __LINE__, res);
		goto nlmsg_fail;
	}
	genlmsg_end(skb, hdr);
	genlmsg_multicast(&genl_test_family, skb, 0, group, flags);
	return;

nlmsg_fail:
	genlmsg_cancel(skb, hdr);
	nlmsg_free(skb);
	return;
}

#endif

/*
* A table of dynamically configurable HCSR devices
*/
static const char *hcsr_id_table[MAX_DEVICES] = { 
         "HCSR_1"
        /* "HCSR_2",
         "HCSR_3",
         "HCSR_4",
         "HCSR_5",
         "HCSR_6",
         "HCSR_7",
         "HCSR_8",
         "HCSR_9",
         "HCSR_10"*/
};


/*
 * Driver Initialization
 */
int init_led_device(int busnumber)
{

        int status = 0;
        pdev = bus_find_device_by_name(&spi_bus_type, NULL, "spi1.0");
        if(pdev)
        {
            printk("name: %s \n",pdev->driver->name);
        }
        else
        {
            printk("Couldnt find the device line =%d\n",__LINE__);
        }
        spi_dev = to_spi_device(pdev);
        if(!spi_dev)
        {
            printk("Spidev is null %d\n",__LINE__);
            return -1;
        }
        printk("got spi_device successfully\n");
        strlcpy(spi_dev->modalias,"spidev", SPI_NAME_SIZE);
        printk("LED INIT SUCCESS : %d\n ",status);
        return 1;

}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations hcsr_fops = {
    .owner              = THIS_MODULE,           /* Owner */
    .read               = NULL,
    .write              = NULL,
    .release            = NULL,
};

int hcsr_init(struct hcsr_dev *pchsrdev, int device_instance)
{
	int ret;
        printk("Device Instance: %d line = %d\n",device_instance,__LINE__);
        pchsrdev->miscDev.name = hcsr_id_table[device_instance];
        pchsrdev->miscDev.minor = device_instance + 1; 
        pchsrdev->miscDev.fops = &hcsr_fops;
        ret = misc_register(&pchsrdev->miscDev);
        if(ret)
        {
            printk("Couldn't register for device %d with error %d\n",device_instance,ret);
            return ret;
	}
        pchsrdev->writeIndex = 0; 
        pchsrdev->maxEntries = 0; 
	return 0;
}


/*
 * int __init hcsr_driver_init(void)
 * Driver Initialization
 */
int __init hcsr_driver_init(void) 
{
       int retval,i;
       int tempsize;
       DPRINTK("Func=%s  Line=%d  \n",__func__,__LINE__);

       if(max_device < 0 || max_device > MAX_DEVICES)
           max_device = MAX_DEVICES;
       /* allocate and initialize the platform device structure*/
       tempsize = max_device * sizeof(struct hcsr_dev);
       pchsrChipdev = kmalloc(tempsize, GFP_KERNEL);
       if (!pchsrChipdev)
       {
          printk("Not able to allocate memory for chip\n");
          return -ENOMEM; //try free all the memory before terurning
       }
       else
       {
           memset(pchsrChipdev, 0, tempsize);
       }
       for (i = 0; i < max_device; i++)
       {
           retval = hcsr_init(&pchsrChipdev[i], i);
           if(retval)
           {
               printk("Inti/Reg failure for Misc device retister %d \n",__LINE__);
               return -1;
           }

       }
       if(init_led_device(1) < 0)
       {
            printk("SPI init for LED failed\n");
            return -1;
       }
      
#ifdef NLSUPPORT
	retval   = genl_register_family(&genl_test_family);
        if(retval)
        {
            printk("Netlink Reg failure %d\n",__LINE__);
        }
#endif 
       printk("Module initalized successfully...\n");
       return 0;
}




/*
 * void __exit hcsr_driver_exit(void)
 * Driver Exit Function
 */
void __exit hcsr_driver_exit(void)
{
    int i;
    int retval;

    for (i = 0; i < max_device; i++)
    {
        misc_deregister(&pchsrChipdev[i].miscDev);
    }
    //spi_unregister_device(spi_dev);
    //spi_dev = NULL;
    kfree(pchsrChipdev);
#ifdef NLSUPPORT
	retval   = genl_unregister_family(&genl_test_family);
        if(retval)
        {
            printk("Netlink unReg failure %d\n",__LINE__);
        }
#endif 
    printk("hcsr driver removed.\n");
};


module_param(max_device, int, 0660);
module_init(hcsr_driver_init);
module_exit(hcsr_driver_exit);
MODULE_LICENSE("GPL v2");

