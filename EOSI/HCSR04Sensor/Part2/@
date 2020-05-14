/* ----------------------------------------------- platform hcsr driver --------------------------------------------------
 
						platform driver module
 
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
#include "hcsr_platdriver.h"
#include "hcsr_ioctl.h"

#define MAX_ENTRIES 10
#define BUFF_SIZE 256
#define CLASS_NAME "HCSR"
#define MAX_MUX
//#define DEBUG  
#if defined(DEBUG)
    #define DPRINTK(fmt,args...) printk("DEBUG: %s %s() %d: \n"fmt,__FILE__,__func__,__LINE__,##args);
#else
   #define DPRINTK(fmt,args...)
#endif

struct user_hcsr_data{
  int distance;
  uint64_t tsc;
};
int calculate_distance(struct user_hcsr_data *arr,  int writeCurrIndex, int maxEntry);

//int muxping[MAX_MUX]

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
   /*Using only till 13,
   IO14 = 14,
   IO15 = 15,
   IO16 = 16,
   IO17 = 17,
   IO18 = 18,
   IO19 = 19
   */
};

/*per device structure */

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
        int mux1;
        int mux2;
        int level_shifter;
        int avg_distance;
        int last_time;
	spinlock_t m_Lock;
	struct task_struct *task;
        uint64_t tsc1;
        uint64_t tsc2;
        short irq_status;
        struct device *dev;
};

static struct hcsr_dev *pchsrChipdev[10]; /* should use MAX_DEVICE */
static struct class *hcsrclass;
static int device_instance=0;
static int inuse;

/* function prototypes */
//int validate_triggerpin(int tpin);
int validate_echopin(int epin);
static void start_measurement(struct hcsr_dev *phcsr);
void resetirq(struct hcsr_dev *phcsrdev);

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
        hcsr_devp->trigger_pin = 0;
        hcsr_devp->echo_pin = 0;
        hcsr_devp->mux1 = -1;
        hcsr_devp->mux2 = -1;
        hcsr_devp->level_shifter = -1;
        hcsr_devp->tsc1 = 0;
        hcsr_devp->tsc2 = 0;
        hcsr_devp->avg_distance = 0;
        hcsr_devp->last_time = 0;
        sema_init(&hcsr_devp->readBlock,0);
        spin_lock_init(&hcsr_devp->m_Lock); 
        inuse++;
	printk("\n%s is opened successfully \n", hcsr_devp->miscDev.name);
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
        if(phcsrdev->level_shifter > -1)
            gpio_free(phcsrdev->level_shifter);
        inuse--;
	printk("%s is closed successfully\n", phcsrdev->miscDev.name);
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
    unsigned int average = 0;
    int i = 0;

    phcsrdev = (struct hcsr_dev *)handle;
    DPRINTK("Inside measuremet thread %s \n",phcsrdev->miscDev.name);

    while(counter <  (phcsrdev->m+2))
    { 
        gpio_set_value_cansleep(phcsrdev->trigger_pin, 0);
        mdelay(10);
        gpio_set_value_cansleep(phcsrdev->trigger_pin, 1);
        /* As per HC-SR04 datasheet, the trigger pulse is high for 10usec*/
        udelay(10);
        gpio_set_value_cansleep(phcsrdev->trigger_pin, 0);  
        mdelay(phcsrdev->delta); 
        DPRINTK("cou%d dist = %d EchoPin = %d name = %s\n",counter, phcsrdev->user_data[counter].distance,phcsrdev->echo_pin,phcsrdev->miscDev.name);
        counter++;
    }                                            
    phcsrdev->thread_state = 0;
    average = calculate_distance(phcsrdev->user_data, phcsrdev->writeIndex-1, phcsrdev->maxEntries); 
    DPRINTK("Average distance = %d\n",average); 
    phcsrdev->avg_distance = average;    
    phcsrdev->last_time = phcsrdev->tsc2;    
    
    if(phcsrdev->status == 1) 
    {
       phcsrdev->status = 0;
       up(&phcsrdev->readBlock);
    }
    //resetting the data here
    for(i = 0;i < MAX_ENTRIES; i++)
    {   
        phcsrdev->user_data[i].distance = 0;
	phcsrdev->user_data[i].tsc = 0;
    }   
    phcsrdev->writeIndex = 0;
    phcsrdev->maxEntries = 0;
    phcsrdev->tsc1= 0;
    phcsrdev->tsc2= 0;
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
    {   
        printk("Copy to user space failed\n");
        return -EFAULT;
    }
    
    if (phcsrdev->thread_state == 1) //thread is currently executing
    {   
        printk("Measurement is going on; can't delete existing data\n");
        return -EINVAL;
    }   
    else /* if thread not running */
    {
        if (temp_user_input != 0)   //clearing the data struction 
        {   
	    spin_lock_irqsave(&(phcsrdev->m_Lock), flags );
            for(i = 0;i < MAX_ENTRIES; i++)
            {   
                phcsrdev->user_data[i].distance = 0;
                phcsrdev->user_data[i].tsc = 0;
            }   
            phcsrdev->writeIndex = 0;
            phcsrdev->tsc1= 0;
            phcsrdev->tsc2= 0;
            spin_unlock_irqrestore(&(phcsrdev->m_Lock), flags);
        }
        start_measurement(phcsrdev);
    }   
       
    return 0;
}


/* start the measurement thrad */
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
    int ret = 0;

    DPRINTK("Inside hscr %s read operation\n",phcsrdev->miscDev.name);
    if(phcsrdev->maxEntries == 0) //the buffer is empty
    {
       start_measurement(phcsrdev);
       /* make the process sleep */
       phcsrdev->status = 1;
       DPRINTK("Acquiring the semaphore \n");
       ret =  down_interruptible(&phcsrdev->readBlock);
       if(ret != 0)
       {
           printk("Couldnt lock the interrupt\n");
           return 0; // No data Read
       }   
     }
    
    DPRINTK("maxEntries = %d  writeIndex = %d \n",phcsrdev->maxEntries,phcsrdev->writeIndex);
    spin_lock_irqsave(&(phcsrdev->m_Lock), flags);
    data.distance = phcsrdev->avg_distance; 
    data.tsc = phcsrdev->last_time;
    spin_unlock_irqrestore(&(phcsrdev->m_Lock), flags);
    if (copy_to_user((struct user_hcsr_data *)buf,&data, sizeof(struct user_hcsr_data)))
    {
        return -EFAULT;
    }

    
    return byte_read;
}

/*
* int calculate_distance(struct user_hcsr_data *arr,  int writeCurrIndex, int maxEntry)
* Calculate the average distance for m readings
*/
int calculate_distance(struct user_hcsr_data *arr,  int writeCurrIndex, int maxEntry)
{
     unsigned int sum = 0;
     unsigned int average = 0;
     int i = 0;

     DPRINTK("maxEntries = %d  writeIndex = %d\n",maxEntry,writeCurrIndex);

     if(maxEntry == 0)
     {
          DPRINTK("No entry in buffer, returning 0\n");
          return 0;
     }
     // we have less number of data than user asked for, incase of missing interrupts,or when interrupt down is not followed by interrupt up
     if(maxEntry < MAX_ENTRIES)
     {
         for(i = 0;i < maxEntry; i++)
         {
             printk("arr[%d].distrance = %d\t",i, arr[i].distance);
             sum = sum + arr[i].distance;
         }
         average = sum/maxEntry;
     }
     else
     {
         // if we get full data, we skip the 1st and last entry
         for(i = 1 ;i < MAX_ENTRIES - 1; i++) /* skipping the fist and last entry*/
         {
             printk("arr[%d].distrance = %d\t",i, arr[i].distance);
             sum = sum + arr[i].distance;
         }
         average = sum/maxEntry;
     }
     printk("\n");
     
     return average;
}

/*IRQ handler */

static irqreturn_t irq_handler(int irq, void *dev_id)
{
    
    struct hcsr_dev *phcsrdev = (struct hcsr_dev *)dev_id;
    unsigned long flags;
    unsigned int temp;
    unsigned int distance;
    int gpioVal;

    DPRINTK("irqhdlr %s  ",phcsrdev->miscDev.name);

    gpioVal = gpio_get_value(phcsrdev->echo_pin);
    if(irq == gpio_to_irq(phcsrdev->echo_pin))
    {
  
        if(gpioVal == 0)
        {
            DPRINTK("low\n");
            if(phcsrdev->tsc1 == 0) /* spurious interrupt, haven't seen the high but only low */
            {
                printk("got spurious interrupt\n");
                irq_set_irq_type(irq, IRQF_TRIGGER_RISING);
                return IRQ_NONE; 
            }
            phcsrdev->tsc2 = get_tsc();
            temp = phcsrdev->tsc2 - phcsrdev->tsc1;
            
            temp = (temp * 34)/2; /* to avoid overflow took away 10^3 from 34000 and 399076(cpu clock) */
            distance = temp/399076;

            if(distance > 400)      /*hcsr04 says distance can never exceed 4m*/
                distance = 50;
	    spin_lock_irqsave(&(phcsrdev->m_Lock), flags);

            phcsrdev->user_data[phcsrdev->writeIndex % MAX_ENTRIES].tsc = phcsrdev->tsc2;
            phcsrdev->user_data[phcsrdev->writeIndex % MAX_ENTRIES].distance = distance;
            if(phcsrdev->maxEntries < MAX_ENTRIES)
               phcsrdev->maxEntries++;
            phcsrdev->writeIndex++;
           // phcsrdev->status = 0;
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
*/

void set_mux(int pin, int mux1, int mux2, int high)
{
    
    gpio_request(pin, "sysfs");
    DPRINTK("Mux1 = %d  mux2 = %d high = %d \n",mux1, mux2,high);
    if(mux1 > -1)
    {
        gpio_set_value_cansleep(mux1, high);
    } 
    if(mux2 > -1)
    {
        gpio_set_value_cansleep(mux2, high);
    } 
}

/*
* int setpin(int shieldPin)
* Validates and maps the Shield Pin with Linux gpio Pins
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

int setirq(int pin, struct hcsr_dev *phcsrdev)
{
   int gpio_irq = 0;
   DPRINTK("func=%s echopin = %d name = %s\n",__func__,pin,phcsrdev->miscDev.name);
   if ((gpio_irq = gpio_to_irq(pin)) < 0)
   {
       printk("gpio_to_irq mapping failure \n");
       return -1;
   }
   if(request_irq(gpio_irq, (irq_handler_t ) irq_handler, IRQF_TRIGGER_RISING , GPIO_INTERRUPT, (void*)phcsrdev))
   {
       printk("irq request failed");
       return -1;
   }
   phcsrdev->irq_status = 1;

   printk("IRQ handler registered \n");
   return 0;
}


void resetirq(struct hcsr_dev *phcsrdev)
{
   printk("IRQ handler freed \n");
   if(phcsrdev->irq_status)
       free_irq(gpio_to_irq(phcsrdev->echo_pin), (void*)phcsrdev);
   phcsrdev->irq_status = 0;
}



static long hcsr_driver_ioctl(struct file *file, unsigned int ioctl_num, unsigned long ioctl_param) 
{

    gpioPinSetting gpioPin;
    gpioParSetting gpioSet;
    char *buffer;
    struct hcsr_dev *phcsrdev = file->private_data;
    int retval;
    unsigned long flags;
    int mux_1 = -1;
    int mux_2 = -1;
    int level_s = -1;
    int echo = -1;
    int trigger = -1;
    switch (ioctl_num) {

        case IOCTL_CONFIG_PINS:                    //this sets the flag
           buffer = kmalloc(sizeof(gpioPinSetting), GFP_KERNEL); 
           copy_from_user((void *)buffer, (unsigned long *)ioctl_param, sizeof(gpioPinSetting));


          /* We got Shield Pins from the user */ 
           gpioPin.triggerPin = ((gpioPinSetting *)buffer)->triggerPin;
           gpioPin.echoPin = ((gpioPinSetting *)buffer)->echoPin;
           kfree(buffer);
           DPRINTK("ioctl trigger=%d    echo=%d device_name = %s\n",gpioPin.triggerPin, gpioPin.echoPin,phcsrdev->miscDev.name);
           spin_lock_irqsave(&(phcsrdev->m_Lock), flags );

           /* Validate and map the Shield Pins to valid gpios*/
           trigger =  setpin(gpioPin.triggerPin, 0, 0, &mux_1, &mux_2, &level_s);  
           if(trigger < 0) {
               printk("Trigger Pin has problem\n");
               return -EINVAL;
           }
           echo = setpin(gpioPin.echoPin, 1, 0, &mux_1, &mux_2, &level_s);  
           if(echo < 0) {
               printk("ECHO Pin has problem\n");
               return -EINVAL;
           }

           //phcsrdev->trigger_pin = gpioPin.triggerPin;
           //phcsrdev->echo_pin = gpioPin.echoPin;
           phcsrdev->trigger_pin = trigger;
           phcsrdev->echo_pin = echo;
           phcsrdev->mux1 = mux_1; 
           phcsrdev->mux2 = mux_2; 
           phcsrdev->level_shifter = level_s; 
           
           spin_unlock_irqrestore(&(phcsrdev->m_Lock), flags);
           retval = setirq(echo, phcsrdev);
           if(retval < 0)
           {
               return -1;
           }
          // printk("Func=%s  Line=%d\n",__func__,__LINE__);
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

/* Sysfs Initialization */



static ssize_t trigger_show(struct device *dev, struct device_attribute *attr, char *buf)
{

	struct hcsr_dev *phcsr_dev_obj = dev_get_drvdata(dev);
        return sprintf(buf,"%d\n", phcsr_dev_obj->trigger_pin);
}

static ssize_t trigger_store(struct device *dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct hcsr_dev *phcsr_dev_obj = dev_get_drvdata(dev);
        int mux1 = -1;
        int mux2 = -1;
        int level_s = -1;
        int trigger = -1;

        /*We read the shield Pin*/
        kstrtoint(buf, 10, &phcsr_dev_obj->trigger_pin);
        /*validates and sets pin*/
        DPRINTK("name = %s\n",phcsr_dev_obj->miscDev.name); 
        trigger = setpin(phcsr_dev_obj->trigger_pin, 0, 0, &mux1, &mux2, &level_s);
        if(trigger < 0) {
            printk("Trigger pin has problem\n");
            return -EINVAL;
        }
        phcsr_dev_obj->trigger_pin = trigger;
        phcsr_dev_obj->mux1 = mux1;
        phcsr_dev_obj->mux2 = mux2;
        phcsr_dev_obj->level_shifter = level_s;
        
	return count;

}
static DEVICE_ATTR(trigger,S_IRWXU, trigger_show, trigger_store);

static ssize_t echo_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hcsr_dev *phcsr_dev_obj = dev_get_drvdata(dev);
        return sprintf(buf,"%d\n", phcsr_dev_obj->echo_pin);
}
static ssize_t echo_store(struct device *dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct hcsr_dev *phcsr_dev_obj = dev_get_drvdata(dev);
        int mux1 = -1;
        int mux2 = -1;
        int level_s = -1;
        int echo = -1;

        kstrtoint(buf, 10, &phcsr_dev_obj->echo_pin);
        /*validates and sets pin*/
        
        DPRINTK("name = %s\n",phcsr_dev_obj->miscDev.name); 
        echo = setpin(phcsr_dev_obj->echo_pin, 1, 0, &mux1, &mux2, &level_s);
        if(echo < 0) {
            printk("Echo pin has problem\n");
            return -EINVAL;
        }
        printk("echo = %d\n",echo);
        phcsr_dev_obj->echo_pin = echo;
        phcsr_dev_obj->mux1 = mux1;
        phcsr_dev_obj->mux2 = mux2;
        phcsr_dev_obj->level_shifter = level_s;

        //setirq(phcsr_dev_obj->echo_pin, phcsr_dev_obj);
	return count;

}
static DEVICE_ATTR(echo,S_IRWXU, echo_show, echo_store);

static ssize_t number_samples_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hcsr_dev *phcsr_dev_obj = dev_get_drvdata(dev);
        return sprintf(buf,"%d\n", phcsr_dev_obj->m);
}

static ssize_t number_samples_store(struct device *dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct hcsr_dev *phcsr_dev_obj = dev_get_drvdata(dev);
        kstrtoint(buf, 10, &phcsr_dev_obj->m);
	return count;

}

static DEVICE_ATTR(number_samples,S_IRWXU, number_samples_show, number_samples_store);

static ssize_t sampling_period_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hcsr_dev *phcsr_dev_obj = dev_get_drvdata(dev);
        return sprintf(buf,"%d\n", phcsr_dev_obj->delta);
}


static ssize_t sampling_period_store(struct device *dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct hcsr_dev *phcsr_dev_obj = dev_get_drvdata(dev);
        int delta = 0;
        kstrtoint(buf, 10, &delta);
        if(delta < 60)     //Min time between two trigger is 60ms else echo may overlap
            delta = 60;
        phcsr_dev_obj->delta = delta;
        //kstrtoint(buf, 10, &phcsr_dev_obj->delta);
	return count;

}
static DEVICE_ATTR(sampling_period,S_IRWXU, sampling_period_show, sampling_period_store);


static ssize_t enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct hcsr_dev *phcsr_dev_obj = dev_get_drvdata(dev);
        return sprintf(buf,"%d\n", phcsr_dev_obj->thread_state);
}

static ssize_t enable_store(struct device *dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	struct hcsr_dev *phcsr_dev_obj = dev_get_drvdata(dev);
        int userinput;
        kstrtoint(buf, 10, &userinput);
	if (userinput == 1) /*start measurement */
	{
            setirq(phcsr_dev_obj->echo_pin, phcsr_dev_obj);
	    if (phcsr_dev_obj->thread_state == 1) 
            {
                DPRINTK("measurement ongoing\n");
            }
            else  /* start measurement */
            {
                start_measurement(phcsr_dev_obj);
            }
	}
        else if (userinput == 0) /* stop measurement */
        {
            if(phcsr_dev_obj->thread_state == 1)
                kthread_stop(phcsr_dev_obj->task);
            resetirq(phcsr_dev_obj);
        }
	return count;
}

static DEVICE_ATTR(enable, S_IRWXU, enable_show, enable_store);


static ssize_t distance_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	
	struct hcsr_dev *phcsr_dev_obj = dev_get_drvdata(dev);

	DPRINTK("distance= %d\n", phcsr_dev_obj->avg_distance);
	return sprintf(buf,"%d\n",phcsr_dev_obj->avg_distance);
	
}


static ssize_t distance_store(struct device *dev, struct device_attribute *attr,
                                  const char *buf, size_t count)
{
	//struct hcsr_dev *phcsr_dev_obj = dev_get_drvdata(dev);
	return count;
}

static DEVICE_ATTR(distance, S_IRWXU, distance_show, distance_store);


static struct device_attribute *HCSR_attributes[] = { 
        &dev_attr_trigger,
        &dev_attr_echo,
        &dev_attr_number_samples,
        &dev_attr_sampling_period,
        &dev_attr_enable,
        &dev_attr_distance,
        NULL
};


static const struct platform_device_id hcsr_id_table[] = { 
         { "HCSR_1", 0 },
         { "HCSR_2", 0 },
         { "HCSR_3", 0 },
         { "HCSR_4", 0 },
         { "HCSR_5", 0 },
         { "HCSR_6", 0 },
         { "HCSR_7", 0 },
         { "HCSR_8", 0 },
         { "HCSR_9", 0 },
         { "HCSR_10", 0 },
};





/*static CLASS_ATTR_RW(HCSR,0644, show_debug, store_debug);*/

/*
 * Driver Initialization
 */


int hcsr_driver_init(struct hcsr_dev *pchsrdev)
{
	int ret;
        memset(pchsrdev, 0, sizeof (struct hcsr_dev));
        pchsrdev->miscDev.name = hcsr_id_table[device_instance].name;
        pchsrdev->miscDev.minor = device_instance + 1; 
        pchsrdev->miscDev.fops = &hcsr_fops;
        ret = misc_register(&pchsrdev->miscDev);
        if(ret)
        {
            printk("Couldn't register for device %d with error %d\n",device_instance,ret);
            return ret;
	}
        pchsrdev->writeIndex = 0; 
	return 0;
}


static int hcsr_driver_probe(struct platform_device *dev_found)
{
       int retval,i;
       int tempsize;

       DPRINTK("Found the device -- %s  \n",dev_found->name);
       //Device instance exceeds the number of devices listed
       if(device_instance >=  sizeof(hcsr_id_table)/sizeof(struct platform_device_id))
            return -ENOMEM;

       tempsize = sizeof(struct hcsr_dev);
       pchsrChipdev[device_instance] = kzalloc(tempsize, GFP_KERNEL);
       memset(pchsrChipdev[device_instance], 0, tempsize);
       if (!pchsrChipdev[device_instance])
             return -ENOMEM;
        
       pchsrChipdev[device_instance]->dev = &dev_found->dev;
       platform_set_drvdata(dev_found, pchsrChipdev[device_instance]);
       retval = hcsr_driver_init(pchsrChipdev[device_instance]);
       if(retval)
       {
          printk("Inti/Reg failure for device %s\n",dev_found->name);
       }
       /* populate syfs */
       if(device_instance == 0)
       {
           hcsrclass = class_create(THIS_MODULE, CLASS_NAME);
           if (IS_ERR(hcsrclass)) {
               printk( " cant create class %s\n", CLASS_NAME);
               goto class_error;
           }
       }
       pchsrChipdev[device_instance]->dev = device_create(hcsrclass, NULL, device_instance+1, pchsrChipdev[device_instance], dev_found->name);
       if (IS_ERR(pchsrChipdev[device_instance]->dev)) 
       {
           printk( " cant create device %s\n", dev_found->name);
           goto device_error;
       }
      for (i = 0; HCSR_attributes[i]; i++) 
      {
          retval = device_create_file(pchsrChipdev[device_instance]->dev, HCSR_attributes[i]);
          //retval = device_create_file(hcsrdevice, HCSR_attributes[i]);
          if (retval)
          {
            printk("Removing devcie create files \n");
            while (--i >= 0)
                device_remove_file(pchsrChipdev[device_instance]->dev, HCSR_attributes[i]);
            goto device_error; 
          }
      }
      device_instance++;
      return 0;

device_error:
      device_destroy(hcsrclass, device_instance+1);
class_error:
      class_unregister(hcsrclass);
      class_destroy(hcsrclass);
      return 0;
};




static int hcsr_driver_remove(struct platform_device *dev_found)
{
     struct hcsr_dev *phcsrChipdev = dev_get_drvdata(&dev_found->dev);
     printk("Found the device -- %s  %s\n",dev_found->name, phcsrChipdev->miscDev.name);
      /* clean up the sysfs */
      if(inuse == 0)
      {
          device_destroy(hcsrclass, phcsrChipdev->miscDev.minor);
      }
      misc_deregister(&phcsrChipdev->miscDev); /* deregister the misc driver */
      kfree(phcsrChipdev);
      device_instance--;
      if(device_instance == 0)
      {
          class_unregister(hcsrclass);
          class_destroy(hcsrclass);
      }
      return 0;
};


static struct platform_driver hcsr_platform_driver = { 
        .driver         = { 
                .name   = DRIVER_NAME,
                .owner  = THIS_MODULE,
        },  
        .probe          = hcsr_driver_probe,
        .remove         = hcsr_driver_remove,
        .id_table       = hcsr_id_table,
};

module_platform_driver(hcsr_platform_driver);
MODULE_LICENSE("GPL");

