/*   A test program for hcsr distance sensor
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include "hcsr_ioctl.h"
#include <unistd.h>

#define DEVICE_NAME                 "/dev/HCSR_"  // Device name to be created and registered

#define MAX_DEVICES_SUPPORTED 2

int fd[MAX_DEVICES_SUPPORTED];

//HCSR 1 Sheild PINS
#define TRIGG_PIN_1 8
#define ECHO_PIN_1  5
#define M_1         5
#define DELTA_1     100

//HCSR 2 Sheild PINS
#define TRIGG_PIN_2 7
#define ECHO_PIN_2  10
#define M_2         7 
#define DELTA_2     100

/* structure used with read system call to fetch timestamd and distnace information */
struct user_hcsr_data{
  int distance;
  unsigned long long tsc;
};


int main(int argc, char **argv)
{
        int i,j;
        char buffer[256];
        int ret;
        int writebuffer=0;
        struct user_hcsr_data readOutput;
        int num = 0;
        /* structure containg gpio trigger and echo pin configuraton currently on 2 sets are populated */
        gpioPinSetting gpios[MAX_DEVICES_SUPPORTED] = 
        {
            { .triggerPin= TRIGG_PIN_1, .echoPin=ECHO_PIN_1},       
            { .triggerPin= TRIGG_PIN_2, .echoPin= ECHO_PIN_2}
        };
        /* structure for frequency and sampling period configuration */
        gpioParSetting gpiomf[MAX_DEVICES_SUPPORTED] = 
        {
            { .frequency= M_1, .samplingPeriod= DELTA_1 },       
            { .frequency= M_2, .samplingPeriod= DELTA_2 }
        };
#if 1

	/* open devices */
        for(i=0; i<MAX_DEVICES_SUPPORTED; i++)
        {  
            num = i + 1;
            sprintf(buffer,"%s%d",DEVICE_NAME,num);
            printf("%s\n",buffer);
	    fd[i] = open(buffer, O_RDWR);
	    if (fd[i] < 0)
            {
	        perror("Can not open device file");		
                goto ERROR;
	    }
        }
        printf("Device(s) open successful\n");
        
        /* perform ioctl config pin operation for each device node*/ 
        for(i=0; i<MAX_DEVICES_SUPPORTED; i++)
        {
           printf("TP=%d,  EP = %d\n",gpios[i].triggerPin, gpios[i].echoPin);
 
           if((ret = ioctl(fd[i], IOCTL_CONFIG_PINS, &gpios[i])) < 0)
           {
              perror("IOCTL HSR_IOCTL_SETPIN Failure\n");
              goto ERROR;
           }
        }
        /* perform ioctl set_parameter for each device node */
        for(i=0; i<MAX_DEVICES_SUPPORTED; i++)
        {
           if((ret = ioctl(fd[i], IOCTL_SET_PARAMETERS, &gpiomf[i])) < 0)
           {
              perror("IOCTL HSR_IOCTL_SETMODE Failure\n");
              goto ERROR;
           }
        }
        /* perform blocking read , this will block the entire process hence commenting */
#if 0
        for(i=0; i<MAX_DEVICES_SUPPORTED; i++)
        {
           memset(&readOutput, 0, sizeof(struct user_hcsr_data));
           if((ret = read(fd[i],(void *)&readOutput, sizeof(struct user_hcsr_data) )) < 0)
           {
              perror("read Failure\n");
              goto ERROR;
           }
        }
#endif 
        /* perform write */
        for(i=0; i<MAX_DEVICES_SUPPORTED; i++)
        {
           printf("\n############Performing write with noreset operations fd=%d \n",fd[i]);
           writebuffer = 0; /* performing write with no effect to existing value*/
           if((ret = write(fd[i],(void *)&writebuffer, sizeof(writebuffer) )) < 0)
           {
              perror("write Failure\n");
              goto ERROR;
           }
           else /* write successfule so perform read */
           { 
           printf("\n############Performing read operations fd=%d \n",fd[i]);
            sleep(3);
            for(j=0; j< 5; j++)
            {
               memset(&readOutput, 0, sizeof(struct user_hcsr_data));
               if((ret = read(fd[i],(void *)&readOutput, sizeof(struct user_hcsr_data) )) < 0)
               {
                  perror("read Failure\n");
                  goto ERROR;
               }
               printf("##  distance=%d, timestamp=%llu\n",readOutput.distance, readOutput.tsc);
            }
           }

           writebuffer = 1; /* performing write with  effect to existing value*/
           printf("\n############Performing write with reset operations fd=%d \n",fd[i]);
           if((ret = write(fd[i],(void *)&writebuffer, sizeof(writebuffer) )) < 0)
           {
              perror("write Failure\n");
              goto ERROR;
           }
           else /* write successfule so perform read */
           {
               sleep(3);
           printf("\n############Performing read operations fd=%d \n",fd[i]);
            for(j=0; j<5 ; j++)
            {
               memset(&readOutput, 0, sizeof(struct user_hcsr_data));
               if((ret = read(fd[i],(void *)&readOutput, sizeof(struct user_hcsr_data) )) < 0)
               {
                  perror("read Failure\n");
                  goto ERROR;

               }
               printf("##  distance=%d, timestamp=%llu\n",readOutput.distance, readOutput.tsc);
            }
           }
        }
        
ERROR:
        for(i=0; i<MAX_DEVICES_SUPPORTED; i++)
        {
            close(fd[i]);
        }
#endif

	return 0;
}

    
