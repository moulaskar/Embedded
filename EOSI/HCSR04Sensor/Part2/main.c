/*   A test program for red-black tree
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

#define GPIO_MUX_1 74
#define GPIO_MUX_1_NAME GPIO_MUX_1
#define GPIO_MUX_2 66
#define GPIO_MUX_2_NAME GPIO_MUX_2

struct user_hcsr_data{
  int distance;
  unsigned long long tsc;
};

void IOSetup_init(void)
{
	int gpiofd, pinfd;
        int retval;

	gpiofd = open("/sys/class/gpio/export", O_WRONLY);
	if (gpiofd < 0)
		perror("\n gpiofd open failed\n");
	
        sleep(1);
        retval = write(gpiofd, "74" ,2);
	//retval =  write(gpiofd, (void *)GPIO_MUX_1_NAME,2);
	if(retval < 0)
		perror("\n gpiofd export write 74 failed\n");

	
	//retval = write(gpiofd, (void *)GPIO_MUX_2_NAME,2);
        retval = write(gpiofd, "66" ,2);
	if(retval < 0)
		perror("\n gpiofd export write 66 failed\n");
	close(gpiofd);
        
        sleep(1);
        /* Set the value for the pins*/

	pinfd = open("/sys/class/gpio/gpio74/value", O_WRONLY);	
	if (pinfd < 0)
		perror("\n gpio gpio74/value open failed");

        retval = write(pinfd, "0" ,1);
	if(retval < 0)
	//if(0<write(pinfd,"0",1))
		perror("\n gpio gprio74/value write failed");

	pinfd = open("/sys/class/gpio/gpio66/value", O_WRONLY);
	if (pinfd < 0)
		perror("\n gpio gpio66/value open failed");

        retval = write(pinfd, "0" ,1);
	if(retval < 0)
		perror("\n gpio gpio66/value write failed");

        close(pinfd);
        close(gpiofd);
}

void IOSetup_exit(void)
{	int gpiofd;
        int retval;
	gpiofd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (gpiofd < 0)
		printf("\n exit gpio unexport open failed");

        retval = write(gpiofd, "74" ,2);
	//retval = write(gpiofd, GPIO_MUX_1_NAME,2);
	if(retval < 0)
		perror("exit write failed GPIO_MUX_1_NAME");
        retval = write(gpiofd, "66" ,2);
	//retval = write(gpiofd, GPIO_MUX_2_NAME,2);
	if( retval < 0)	
		perror("exit write failed GPIO_MUX_2_NAME");
	close(gpiofd);
}


int main(int argc, char **argv)
{
        int i,j;
        char buffer[256];
        int ret;
        int writebuffer=0;
        struct user_hcsr_data readOutput;
        int num = 0;

        gpioPinSetting gpios[MAX_DEVICES_SUPPORTED] = 
        {
            //{ .triggerPin=38, .echoPin=10 },       
            //{ .triggerPin=40, .echoPin=5 }
            { .triggerPin=7, .echoPin=10 },       
            { .triggerPin=8, .echoPin=5 }
        };
        gpioParSetting gpiomf[MAX_DEVICES_SUPPORTED] = 
        {
            { .frequency=4, .samplingPeriod=200 },       
            { .frequency=6, .samplingPeriod=200 }
        };
       // IOSetup_init();
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
        
        /* perform ioctl config pin operation */ 
        for(i=0; i<MAX_DEVICES_SUPPORTED; i++)
        {
           printf("TP=%d,  EP = %d\n",gpios[i].triggerPin, gpios[i].echoPin);
 
           if((ret = ioctl(fd[i], IOCTL_CONFIG_PINS, &gpios[i])) < 0)
           {
              perror("IOCTL HSR_IOCTL_SETPIN Failure\n");
              goto ERROR;
           }
        }
        /* perform ioctl set_parameter */
        for(i=0; i<MAX_DEVICES_SUPPORTED; i++)
        {
           if((ret = ioctl(fd[i], IOCTL_SET_PARAMETERS, &gpiomf[i])) < 0)
           {
              perror("IOCTL HSR_IOCTL_SETMODE Failure\n");
              goto ERROR;
           }
        }
        /* perform blocking read */
#if 0
        for(i=0; i<MAX_DEVICES_SUPPORTED; i++)
        {
           if((ret = read(fd[i],(void *)&writebuffer, sizeof(writebuffer) )) < 0)
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
            for(j=0; j< 10; j++)
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
              for(j=0; j<10 ; j++)
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
      // IOSetup_exit();

	return 0;
}

    
