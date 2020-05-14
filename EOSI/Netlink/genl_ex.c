/* This file implements a the netlink interface for led configuration and range/speed calculaiton */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include <pthread.h>
#include <signal.h>
#include "genl_ex.h"


#ifndef GRADING
#define MAX7219_CS_PIN 12 
#define HCSR04_TRIGGER_PIN 2
#define HCSR04_ECHO_PIN 3
#define M               5
#define DELTA           100
#endif

//static char message[GENL_TEST_ATTR_MSG_MAX];
//static int send_to_kernel;
static unsigned int mcgroups;		/* Mask of groups */

pthread_mutex_t wmutex=PTHREAD_MUTEX_INITIALIZER;
//#define MAX_DEVICES_SUPPORTED 2 
#define MAX_DEVICES_SUPPORTED 1 
int sequence[14] = {0,300,1,300,2,300,3,300,4,300,5,300,0,0};
int sequence1[14] = {0,100,1,100,2,100,3,100,4,100,5,100,0,0};
//int sequence2[14] = {5,100,5,100,5,100,5,100,5,100,5,100,0,0};

typedef struct {
   int triggerPin;
   int echoPin;
   int ledPin;
}gpioPinSetting;

typedef struct {
   int frequency; /* number of samples per measurement */
   int samplingPeriod; /* period for each sample or the sampling period */
}gpioParSetting;

gpioPinSetting gpios[MAX_DEVICES_SUPPORTED] = 
{
    //{ .triggerPin= 7, .echoPin=10, .ledPin = MAX7219_CS_PIN},       
    { .triggerPin= HCSR04_TRIGGER_PIN, .echoPin=HCSR04_ECHO_PIN, .ledPin = MAX7219_CS_PIN},       
};
/* structure for frequency and sampling period configuration */
gpioParSetting gpiomf[MAX_DEVICES_SUPPORTED] = 
{
  //  { .frequency= 5, .samplingPeriod= 100},       
    { .frequency= M, .samplingPeriod= DELTA},       
};

/*static void add_group(char* group)
Adds multicast group to pass message to other modules */

static void add_group(char* group)
{
	unsigned int grp = strtoul(group, NULL, 10);

	if (grp > GENL_TEST_MCGRP_MAX-1) {
		fprintf(stderr, "Invalid group number %u. Values allowed 0:%u\n",
			grp, GENL_TEST_MCGRP_MAX-1);
		exit(EXIT_FAILURE);
	}

	mcgroups |= 1 << (grp);
}

/*static int send_msg_to_kernel(struct nl_sock *sock, int msgType, int devid)
  This function sends message to kernel for device = devid */

static int send_msg_to_kernel(struct nl_sock *sock, int msgType, int devid)
{
	struct nl_msg* msg;
	int family_id, err = 0;
        int i;
        int temp;
        //printf("sending to kernel msgType=%d devid=%d  ",msgType, devid);
	family_id = genl_ctrl_resolve(sock, GENL_TEST_FAMILY_NAME);
	if(family_id < 0)
        {
             fprintf(stderr, "Unable to resolve family name!\n");
             exit(EXIT_FAILURE);
	}
	msg = nlmsg_alloc();
	if (!msg) 
        {
		fprintf(stderr, "failed to allocate netlink message\n");
		exit(EXIT_FAILURE);
	}
       switch(msgType)
       {
         case HCSR_SET_PIN:
         {
            //printf("sending pin\n");

	    if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, GENL_TEST_C_MSG, 0)) {
	 	fprintf(stderr, "failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	        }
             break;
         }
         case HCSR_SET_PATTERN_1:
         {
            //printf("sending pattern \n");
	    if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, HCSR_SET_PATTERN_1, 0)) {
	 	fprintf(stderr, "failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	     }
             break;
         }
         case HCSR_SET_PATTERN_2:
         {
            //printf("sending pattern 1\n");
	    if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, HCSR_SET_PATTERN_1, 0)) {
	 	fprintf(stderr, "failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	     }
             break;
         }
         case HCSR_GET_SPEED:
         {
            //printf("start speed measurment\n");
	    if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, HCSR_GET_SPEED, 0)) {
	 	fprintf(stderr, "failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	     }
             break;
         }
         case HCSR_CLEANUP:
         {
             printf("CTRL C is pressed, to clean up gpios\n");
	     if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, HCSR_CLEANUP, 0)) {
	 	fprintf(stderr, "failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	     }
             break;
         }
         default:
             printf("Unknown message type\n");
             return 0;
       }
     err = nla_put_string(msg, GENL_TEST_ATTR_MSG, "707");
     err = nla_put_u32(msg, GENL_TEST_ATTR_DEVID, devid);
     err = nla_put_u32(msg, GENL_TEST_ATTR_GPIO_TRIGGER, gpios[devid].triggerPin);
     err = nla_put_u32(msg, GENL_TEST_ATTR_GPIO_ECHO, gpios[devid].echoPin);
     err = nla_put_u32(msg, GENL_TEST_ATTR_GPIO_LED, gpios[devid].ledPin);
     err = nla_put_u32(msg, GENL_TEST_ATTR_FREQ, gpiomf[devid].frequency);
     err = nla_put_u32(msg, GENL_TEST_ATTR_SAMPLE, gpiomf[devid].samplingPeriod);
     if (err) {
	fprintf(stderr, "failed to put nl string!\n");
  	goto out;
     }
     temp = sizeof(sequence)/sizeof(int);
     if(msgType == HCSR_SET_PATTERN_1)
     {
	 for(i=0; i<temp; i++)
	     err = nla_put_u32(msg, GENL_TEST_ATTR_ANI1+i, sequence[i]);
     }
     else
     {

	 for(i=0; i<temp; i++)
	     err = nla_put_u32(msg, GENL_TEST_ATTR_ANI1+i, sequence1[i]);

     }

       err = nl_send_auto(sock, msg);
       if (err < 0) {
		fprintf(stderr, "failed to send nl message!\n");
	}
out:
	nlmsg_free(msg);
	return err;
}



static int skip_seq_check(struct nl_msg *msg, void *arg)
{
	return NL_OK;
}


/*static void prep_nl_sock(struct nl_sock** nlsock)
  This function creates the netlink socket and configures it */

static void prep_nl_sock(struct nl_sock** nlsock)
{
	int family_id, grp_id;
	unsigned int bit = 0;
	
	*nlsock = nl_socket_alloc();
	if(!*nlsock) {
		fprintf(stderr, "Unable to alloc nl socket! %d\n",__LINE__);
		exit(EXIT_FAILURE);
	}

	/* disable seq checks on multicast sockets */
	nl_socket_disable_seq_check(*nlsock);
	nl_socket_disable_auto_ack(*nlsock);

	/* connect to genl */
	if (genl_connect(*nlsock)) {
		fprintf(stderr, "Unable to connect to genl!\n");
		goto exit_err;
	}

	/* resolve the generic nl family id*/
	family_id = genl_ctrl_resolve(*nlsock, GENL_TEST_FAMILY_NAME);
	if(family_id < 0){
		fprintf(stderr, "Unable to resolve family name! %d\n",__LINE__);
		goto exit_err;
	}

	if (!mcgroups)
        {
            //printf("%d    %s\n",__LINE__,__func__);
		return;
        }
	while (bit < sizeof(unsigned int)) {
		if (!(mcgroups & (1 << bit)))
			goto next;

		grp_id = genl_ctrl_resolve_grp(*nlsock, GENL_TEST_FAMILY_NAME,
				genl_test_mcgrp_names[bit]);

		if (grp_id < 0)	{
			fprintf(stderr, "Unable to resolve group name for %u!\n",
				(1 << bit));
            goto exit_err;
		}
		if (nl_socket_add_membership(*nlsock, grp_id)) {
			fprintf(stderr, "Unable to join group %u!\n", 
				(1 << bit));
            goto exit_err;
		}
next:
		bit++;
	}

    return;

exit_err:
    nl_socket_free(*nlsock); // this call closes the socket as well
    exit(EXIT_FAILURE);
}

int tx_delay = 3000000;

/*static int print_rx_msg(struct nl_msg *msg, void* arg)
Depending on the distance, it sends pattern to make the bouncing of ball faster or slower*/

static int print_rx_msg(struct nl_msg *msg, void* arg)
{
	struct nlattr *attr[GENL_TEST_ATTR_MAX+1];
	struct nl_sock* nlsock = NULL;
        int distance;

	genlmsg_parse(nlmsg_hdr(msg), 0, attr, 
			GENL_TEST_ATTR_MAX, genl_test_policy);

	if (!attr[HCSR_GET_SPEED]) {
		fprintf(stdout, "Kernel sent empty message!!\n");
		return NL_OK;
	}
        
        /* Received speed from kernel now send it to kernel again from write */
        distance = nla_get_u32(attr[HCSR_GET_SPEED]);
	prep_nl_sock(&nlsock);
        if(distance > 150)
        {
           send_msg_to_kernel(nlsock, HCSR_SET_PATTERN_1, 0);
        } 
        else if(distance > 100)
        {
           send_msg_to_kernel(nlsock, HCSR_SET_PATTERN_1, 0);
        } 
        else if(distance  > 75)
        {
           send_msg_to_kernel(nlsock, HCSR_SET_PATTERN_2, 0);
        } 
        else 
        {
           send_msg_to_kernel(nlsock, HCSR_SET_PATTERN_2, 0);
        } 
        //printf("Distance=%d Delay= %d \n",distance, tx_delay);
        
	return NL_OK;
}


void * readFromHSCR()
{
	struct nl_sock* nlsock = NULL;
	prep_nl_sock(&nlsock);
        while(1)
        {
            pthread_mutex_lock(&wmutex);
	    prep_nl_sock(&nlsock);
            send_msg_to_kernel(nlsock, HCSR_GET_SPEED, 0);
            pthread_mutex_unlock(&wmutex);
            sleep(2); /* trigger measurement every 2 second */
        } 
    return 0;
}


pthread_t led_thread;
pthread_t spi_thread;
/* Signal Handler for SIGINT */
void sighandler(int sig_num) 
{ 
    struct nl_sock* nlsock = NULL;
    /* Reset handler to catch SIGINT next time. */
    //printf("Inside signal handler\n");
    prep_nl_sock(&nlsock);
    send_msg_to_kernel(nlsock, HCSR_CLEANUP, 0); /* for device 0 */
    printf("Program has exited\n");
    exit(0);

} 

int main(int argc, char** argv)
{
	struct nl_sock* nlsock = NULL;
        //struct sigaction act;
	struct nl_cb *cb = NULL;

        add_group(GENL_TEST_MCGRP0_NAME);
	prep_nl_sock(&nlsock);
        //act.sa_handler = &sighandler;
        
        send_msg_to_kernel(nlsock, HCSR_SET_PIN, 0); /* for device 0 */
        /* thread to send pattern to spi matrix device */
        pthread_create(&spi_thread, NULL, readFromHSCR, NULL);
	/* prep the cb */
	cb = nl_cb_alloc(NL_CB_DEFAULT);
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, skip_seq_check, NULL);
        if(signal(SIGINT, sighandler ) == SIG_ERR)
        {
            printf("Cannot catch sigquit \n"); 
        }
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_rx_msg, NULL);
	do {
	    nl_recvmsgs(nlsock, cb);
	} while (1);
	nl_cb_put(cb);
        nl_socket_free(nlsock);
	return 0;
}
