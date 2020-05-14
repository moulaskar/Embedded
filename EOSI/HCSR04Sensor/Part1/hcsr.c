/* -----------------------------------------------  hcsr misc driver --------------------------------------------------
 
 performs misc driver registration and exposes device nodes only for userspace configuration. 
 Module takes max_device as parameter however the boundary is set to 10 maximum devices. 
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
#include "hcsr_ioctl.h"

#define MAX_DEVICES  10                      // Maximum device instances of this module,overrides module parameter to keep boundary check.

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
	spinlock_t m_Lock;
	struct task_struct *task;
        uint64_t tsc1;
        uint64_t tsc2;
        short irq_status;
        int mux1;
        int mux2;
        int levelShifter;
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

/*
*
* Open hcsr driver: setting up default values
*/
int hcsr_driver_open(struct inode *inode, struct file *file)
{
	struct hcsr_dev *hcsr_devp = file->private_data;
        hcsr_devp->status = 0;
        hcsr_devp->delta = 200;  //default value
        hcsr_devp->m = 10; //default value
        hcsr_devp->writeIndex = 0;
        hcsr_devp->maxEntries = 0;
        hcsr_devp->trigger_pin = 0;
        hcsr_devp->echo_pin = 0;
        hcsr_devp->mux1 = -1;
        hcsr_devp->mux2 = -1;
        hcsr_devp->levelShifter = -1;
        hcsr_devp->tsc1 = 0;
        hcsr_devp->tsc2 = 0;
        sema_init(&hcsr_devp->readBlock,0);
        spin_lock_init(&hcsr_devp->m_Lock); 
	DPRINTK("\n%s is opened successfully \n", hcsr_devp->miscDev.name);
	return 0;
}


/*
 * Release hcsr driver, free the pins, stops the running thread and frees the irq
 */
int hcsr_driver_release(struct inode *inode, struct file *file)
{
        struct hcsr_dev *phcsrdev = file->private_data;
        /* stop thread function if any */
        if(phcsrdev->thread_state == 1)
            kthread_stop(phcsrdev->task);

        resetirq(phcsrdev);
        gpio_free(phcsrdev->echo_pin);
        gpio_free(phcsrdev->trigger_pin);
        if(phcsrdev->mux1 > -1)
            gpio_free(phcsrdev->mux1);
        if(phcsrdev->mux2 > -1)
            gpio_free(phcsrdev->mux2);
        if(phcsrdev->levelShifter > -1)
            gpio_free(phcsrdev->levelShifter);
	DPRINTK("%s is closed successfully\n", phcsrdev->miscDev.name);
	return 0;
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
    
    if(phcsrdev->status == 1) 
    {
       phcsrdev->status = 0;
       up(&phcsrdev->readBlock);
    }
    return 0;
}
    

/* ssize_t hcsr_driver_write(struct file *file, const char *buf,size_t count, loff_t *ppos)
 * @param file: device file structure
 * @param buffer: user's input
 * Func: Does two task a) if thread is not running, start the thread 
 * b) Depending on user's input, performs action. 0=do nothing, 1=clear the datas
 */
ssize_t hcsr_driver_write(struct file *file, const char *buf,
           size_t count, loff_t *ppos)
{
    struct hcsr_dev *phcsrdev = file->private_data;
    int temp_user_input=0;
    int i=0;
    unsigned long flags;
    DPRINTK("Inside hscr %s write operation\n",phcsrdev->miscDev.name);
    if (copy_from_user((void *)&temp_user_input, buf, count))
        return -EFAULT;
    
    if (phcsrdev->thread_state == 1) //thread is currently executing
    {   
        DPRINTK("Measurement is going on; can't delete existing data. Line = %d\n",__LINE__);
        return -EINVAL;
    } 
    else /* if thread not running */
    {
        if (temp_user_input != 0)   //clearing the data  
        {   
	    spin_lock_irqsave(&(phcsrdev->m_Lock), flags );
            for(i = 0;i < MAX_ENTRIES; i++)
            {   
                phcsrdev->user_data[i].distance = 0;
                phcsrdev->user_data[i].tsc = 0;
            }   
            phcsrdev->writeIndex = 0;
            phcsrdev->maxEntries = 0;
            phcsrdev->tsc1= 0;
            phcsrdev->tsc2= 0;
            spin_unlock_irqrestore(&(phcsrdev->m_Lock), flags);
        }
        start_measurement(phcsrdev);
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
 * Read to hcsr driver
 * Read the RBTree instances either ascending or descending
 */

ssize_t hcsr_driver_read(struct file *file, char *buf,
           size_t count, loff_t *ppos)
{
    int byte_read=0;
    struct hcsr_dev *phcsrdev = file->private_data;
    struct user_hcsr_data data;
    unsigned long flags;
    int i = 0;
    int ret = 1;

    DPRINTK("Inside hscr %s read operation Line = %d\n",phcsrdev->miscDev.name,__LINE__);

    if(phcsrdev->maxEntries == 0) //the buffer is empty
    {
       start_measurement(phcsrdev);
       /* make the process sleep */
       phcsrdev->status = 1;
       DPRINTK("Acquiring the semaphore Line = %d\n",__LINE__);
       ret =  down_interruptible(&phcsrdev->readBlock);
       if(ret != 0)
       {
           DPRINTK("Couldnt lock the interrupt\n");
           return 0; // No data Read
       }   
    }
    
    DPRINTK("maxEntries = %d  writeIndex = %d LINE = %d\n",phcsrdev->maxEntries,phcsrdev->writeIndex,__LINE__);
    spin_lock_irqsave(&(phcsrdev->m_Lock), flags);
    data.distance =  calculate_distance(phcsrdev->user_data, phcsrdev->writeIndex-1, phcsrdev->maxEntries, phcsrdev->m);
    data.tsc = phcsrdev->user_data[(phcsrdev->writeIndex % MAX_ENTRIES) -1].tsc;
    spin_unlock_irqrestore(&(phcsrdev->m_Lock), flags);
    if (copy_to_user((struct user_hcsr_data *)buf,&data, sizeof(struct user_hcsr_data)))
    {
        return -EFAULT;
    }

    //Data to user sent, resetting all entries
    for(i = 0;i < MAX_ENTRIES; i++)
    {   
	phcsrdev->user_data[i].distance = 0;
	phcsrdev->user_data[i].tsc = 0;
    }   
    phcsrdev->writeIndex = 0;
    phcsrdev->maxEntries = 0;
    phcsrdev->tsc1= 0;
    phcsrdev->tsc2= 0;
    
    return byte_read;
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

    // DPRINTK("maxEntries = %d  writeIndex = %d LINE = %d\n",maxEntry,writeCurrIndex,__LINE__);

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
             printk("1: arr[%d].distrance = %d\t",i, arr[i].distance);
             sum = sum + arr[i].distance;
         }
         average = sum/maxEntry;
     }
     else
     {
         //for(i = 1 ;i < MAX_ENTRIES - 1; i++) /* skipping the fist and last entry*/
         for(i = 1 ;i < maxEntry - 1; i++) /* skipping the fist and last entry*/
         {
             printk("2: arr[%d].distrance = %d\t",i, arr[i].distance);
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

            if(distance > 400)    //Distance can never exceed 4m for HC-SC04
                distance = 50;   //setting arbitary value within range

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

        case IO11: gpioNo = 5;
                   levelShifter = 24;
                   mux1 = 44;
                   mux2 = 72;
                   break;

        case IO12: gpioNo = 15;
                   levelShifter = 42;
                   break;

        case IO13: gpioNo = 7;
                   levelShifter = 30;
                   mux1 = 46;
                   break;
        /*
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
        default:  printk("Not a valid shield Pin\n");
                  return -EINVAL;
    }
    
    pin = gpioNo;
    gpio_request(pin, "sysfs");
    if(direction == 0)   //Trigger Pin
    {
        ret = gpio_direction_output(pin, value);  
        if( ret < 0)
            return ret;
        set_mux(pin, mux1, mux2, high);
        if(levelShifter > -1)
        {
            ret = gpio_direction_output(levelShifter, value);  
            if( ret < 0)
                return ret;
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



/*
 * static long hcsr_driver_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param)
 * Handles IOCTL_CONFIG_PINS and IOCTL_SET_PARAMETERS
 * IOCTL_SET_PARAMETERS : sets the valueof m(number of samples) and delta (time in ms between each trigger)   
 * IOCTL_CONFIG_PINS : sets the gpio pins

*/
static long hcsr_driver_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) 
{

    gpioPinSetting gpioPin;
    gpioParSetting gpioSet;
    char *buffer;
    struct hcsr_dev *phcsrdev = file->private_data;
    int ret = 0;
    unsigned long flags;
    int gpioTrigger = -1;
    int gpioEcho = -1;
    int mux_1 = -1;
    int mux_2 = -1;
    int level_S = -1;
    DPRINTK("Func=%s  Line=%d\n",__func__,__LINE__);
    switch (ioctl_num) {

        case IOCTL_CONFIG_PINS:                    //this sets the flag
           buffer = kmalloc(sizeof(gpioPinSetting), GFP_KERNEL); 
           copy_from_user((void *)buffer, (unsigned long *)ioctl_param, sizeof(gpioPinSetting));

           /*We got shield Pins */
           gpioPin.triggerPin = ((gpioPinSetting *)buffer)->triggerPin;
           gpioPin.echoPin = ((gpioPinSetting *)buffer)->echoPin;
           kfree(buffer);

           if(gpioPin.triggerPin == gpioPin.echoPin)
           {
               printk("Can't have Trigger Pin and Echo Pin with same value\n");
               return -EINVAL;
           }
            
           //DPRINTK("ioctl trigger=%d    echo=%d device_name = %s\n",gpioPin.triggerPin, gpioPin.echoPin,phcsrdev->miscDev.name);
           printk("Shield Pin: ioctl trigger=%d    echo=%d device_name = %s\n",gpioPin.triggerPin, gpioPin.echoPin,phcsrdev->miscDev.name);

           spin_lock_irqsave(&(phcsrdev->m_Lock), flags );

           /* Validate and map the Shield Pins to valid gpios*/
           gpioTrigger =  setpin(gpioPin.triggerPin, 0, 0, &mux_1, &mux_2, &level_S);  
           if(gpioTrigger < 0) {
               printk("Trigger Pin has problem\n");
               return -EINVAL;
           }

           gpioEcho = setpin(gpioPin.echoPin, 1, 0, &mux_1, &mux_2, &level_S);  
           if(gpioEcho < 0) {
               printk("ECHO Pin has problem\n");
               return -EINVAL;
           }

           //DPRINTK("ioctl Echo=%d    Trigger=%d mux1 = %d levelshifter = %d device_name = %s\n",gpioTrigger,gpioEcho, mux_1, level_S, phcsrdev->miscDev.name);
           printk("ioctl Echo=%d    Trigger=%d mux1 = %d levelshifter = %d device_name = %s\n",gpioTrigger,gpioEcho, mux_1, level_S, phcsrdev->miscDev.name);

           phcsrdev->trigger_pin = gpioTrigger;
           phcsrdev->echo_pin = gpioEcho;
           phcsrdev->mux1 = mux_1;
           phcsrdev->mux2 = mux_2;
           phcsrdev->levelShifter = level_S;
           spin_unlock_irqrestore(&(phcsrdev->m_Lock), flags);
           ret = setirq(gpioEcho, phcsrdev);
           if(ret < 0)
           {
               return -EINVAL;
           }
          break;

        case IOCTL_SET_PARAMETERS:
           buffer = kmalloc(sizeof(gpioParSetting), GFP_KERNEL); 
           copy_from_user((void *)buffer, (unsigned long *)ioctl_param, sizeof(gpioParSetting));
           gpioSet.frequency = ((gpioParSetting *)buffer)->frequency;
           gpioSet.samplingPeriod = ((gpioParSetting *)buffer)->samplingPeriod;
           kfree(buffer);
           DPRINTK("ioctl freq=%d    period=%d\n",gpioSet.frequency, gpioSet.samplingPeriod);
           spin_lock_irqsave(&(phcsrdev->m_Lock), flags );
           phcsrdev->m = gpioSet.frequency;
           phcsrdev->delta = gpioSet.samplingPeriod;
           spin_unlock_irqrestore(&(phcsrdev->m_Lock), flags);
           break;
         default:
             return -EINVAL;
    }
    return 0;
}


/* File operations structure. Defined in linux/fs.h */
static struct file_operations hcsr_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= hcsr_driver_open,        /* Open method */
    .release	        = hcsr_driver_release,     /* Release method */
    .write		= hcsr_driver_write,       /* Write method */
    .read		= hcsr_driver_read,        /* Read method */
    .unlocked_ioctl     = hcsr_driver_ioctl,
};



/*
* A table of dynamically configurable HCSR devices
*/
static const char *hcsr_id_table[MAX_DEVICES] = { 
         "HCSR_1",
         "HCSR_2",
         "HCSR_3",
         "HCSR_4",
         "HCSR_5",
         "HCSR_6",
         "HCSR_7",
         "HCSR_8",
         "HCSR_9",
         "HCSR_10"
};


/*
 * Driver Initialization
 */


int hcsr_init(struct hcsr_dev *pchsrdev, int device_instance)
{
	int ret;

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
               printk("Inti/Reg failure for Misc device retister \n");
           }
       }
      
       printk("Module initalized successfully...\n");
       return 0;
};




/*
 * void __exit hcsr_driver_exit(void)
 * Driver Exit Function
 */
void __exit hcsr_driver_exit(void)
{
    int i;

    for (i = 0; i < max_device; i++)
    {
        misc_deregister(&pchsrChipdev[i].miscDev);
    }
    kfree(pchsrChipdev);
    printk("hcsr driver removed.\n");
};


module_param(max_device, int, 0660);
module_init(hcsr_driver_init);
module_exit(hcsr_driver_exit);
MODULE_LICENSE("GPL v2");

