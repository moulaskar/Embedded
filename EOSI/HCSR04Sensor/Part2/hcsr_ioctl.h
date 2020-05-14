/*
* IOCTL 
*/
#ifndef HCSR04_IOCTL_H
#define HCSR04_IOCTL_H
#include <linux/ioctl.h>
#define HCSR_IOCTL_MAGIC 'k'

//Unique numbers for the ioctl
#define HCSR04_PIN_NUMBER 0x1
#define HCSR04_PARAMETER_NUMBER 0x2
#define GPIO_INTERRUPT "gpio_interrupt"

typedef struct {
   int triggerPin;
   int echoPin;
}gpioPinSetting;

typedef struct {
   int frequency; /* number of samples per measurement */
   int samplingPeriod; /* period for each sample or the sampling period */
}gpioParSetting;


#define IOCTL_CONFIG_PINS _IOW(HCSR_IOCTL_MAGIC, HCSR04_PIN_NUMBER, struct  gpioPinSetting *)
#define IOCTL_SET_PARAMETERS _IOW( HCSR_IOCTL_MAGIC, HCSR04_PARAMETER_NUMBER, struct gpioParSetting *)

#endif
