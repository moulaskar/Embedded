/*   A test program for red-black tree which can perform various operation by opeoning RBT device node.
*    This program can additionally probe the instructions of RBT if it can open the RBprobe driver node 
*    successuflly. Otherwise it will only perform tree operations.
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "RBT_530.h"
#define DEVICE_NAME0 "/dev/rbt530_dev0"
#define DEVICE_NAME1 "/dev/rbt530_dev1"
/* controls the number of thread to access the RBTree in parallel */
#define THREAD_MAX 4 
#define MAX_KPROBE_DATA 10
/* controls the total number of nodes created per device. This value is divided across the number of threads on same tree */
#define MAX_RBT_NODE 100 

typedef struct rbt_object 
{

   int key;
   int data;

}rbt_objt;

struct kprobe_input{
    long int addr;
    int flag;
};


typedef struct rb_probe {
    long long tstamp;
    unsigned long addr;
    pid_t pid;
    int key;
    int value;
}rb_probe;




int fd1, fd2 ,fd3;   /*fd1 and fd2 for rbtree RBT530_dev0 and RBT530_dev1 and fd3 is kprobe fd */
int runflag=1;
int showflag=1;
pthread_mutex_t wmutex=PTHREAD_MUTEX_INITIALIZER;
static void *thread_func(void *arg);
static void *kprobe_func(void *arg);
void print_help(char *name)
{
   printf("\n\n=================================================\n");
   printf("operation %s ascending|descending\n",name);
   printf("Example:\n");
   printf("%s decending - for Red-Black Tree operation and output display in decending order \n",name);
   printf("=================================================\n");
   return;
}

struct threadargs {
   int fd;
   int count;
   int probefd;
   int randsleep;
};

static void display(int fd, int count);
static void showprobe(int filedes);


/* main */

int main(int argc, char **argv)
{
	int i = 0;
	pthread_t threadid[THREAD_MAX];
	pthread_t kpthreadid; /* threadid for kprobe */
	pthread_attr_t threadattr;
	struct sched_param threadparam[THREAD_MAX];
	pthread_attr_init(&threadattr);
        struct threadargs threadarg1, threadarg2, threadarg3;	
        int noprobeflag=0;

	/* open devices */
	fd1 = open(DEVICE_NAME0, O_RDWR);
	fd2 = open(DEVICE_NAME1, O_RDWR);
	fd3 = open("/dev/RBprobe", O_RDWR);

	if (fd1 < 0 || fd2 < 0)
        {
	    perror("Can not open device file");		
            return 0;
	}
        printf("Device(s) open successful\n");
        if(fd3 < 0)
        {
	    perror("Can not open probe device file file, continue without probing");		
            noprobeflag = 1;
        }

        if(argc < 2)
        {
          print_help(argv[0]);
          return 0;
        }
        else //register kprobe address and perform kernel read/write operation
        {
           
           printf("Device IOCTL call: Mode:%s\n",argv[1]);
           /* sets up the mode by invoking ioctl */
           if(!strcmp("ascending", argv[1]))
           {
              ioctl(fd1, SET_DISPMODE, "ascending");
           }
           else 
           {
              ioctl(fd1, SET_DISPMODE, "descending");   
           }
           threadarg1.fd=fd1;
           threadarg1.count=MAX_RBT_NODE; 
           threadarg1.probefd = fd3;
           threadarg1.randsleep = 3 + rand()%5; //random sleep time between 4 to 8 sec
           threadarg2.fd=fd2;
           threadarg2.count=MAX_RBT_NODE;
           threadarg2.probefd = fd3;
           threadarg2.randsleep = 3 + rand()%5; //sleep time between 4 to 8 sec

           /* starting two threads to perform operation on fd1 and another two thread
              to perform on fd2
           */
           printf("THREAD CREATED FOR RBT \n");
	   for(i=0; i<THREAD_MAX/2; i++ )
	   {
		threadparam[i].sched_priority = 10 + (i*2);
		pthread_attr_setschedparam (&threadattr, &threadparam[i]);
		pthread_create(&threadid[i],&threadattr,thread_func,(void*)&threadarg1);
	   }
	
	   for(i=THREAD_MAX/2; i<THREAD_MAX; i++ )
	   {
		threadparam[i].sched_priority = 10 + (i*2);
		pthread_attr_setschedparam (&threadattr, &threadparam[i]);
		pthread_create(&threadid[i],&threadattr,thread_func,(void*)&threadarg2);
	   }
           /* start a kprobe thread */
           threadarg3.probefd = fd3;
           threadarg3.randsleep = 3 + rand()%5;
           if(!noprobeflag)  //create kprobe thread
           {
                pthread_create(&kpthreadid, &threadattr, kprobe_func, (void *)&threadarg3);
           }

        }
        for(i=0;i<THREAD_MAX;i++)
            pthread_join(threadid[i],NULL);
        if(!noprobeflag)
            pthread_detach(kpthreadid);

        close(fd1);
        close(fd2);
        close(fd3);
	return 0;
}



/*kprobe_func
* This function runs continiously to accept intpu from user for
* - instruction address to register  or deregister
* - stop the entire program
*/
static void *kprobe_func(void *arg)
{
   struct threadargs *arg1 = (struct threadargs *)arg;
   struct kprobe_input probeip;
   int res=0;
   char inputbuf[256];
   int fdes = arg1->probefd;
   int threadid = syscall(SYS_gettid);
   printf("\n[Thread %d ] Kprobe Thread\n",threadid);
   sleep(2);
   while(1)
   {
          /* wait for user input */ 
          memset(inputbuf, 0, 256);
          printf("\n===================================\n");
          printf("\nadd: To add an address for probing\n");
          printf("stop: To exit\n");
          printf("===================================\n");
          printf("Enter add or stop: ");
          scanf("%s",inputbuf);
          if(!strcmp(inputbuf, "add"))
          {
             printf("\nEnter address in hex and flag-1:register, 0:unregister: \n");
             scanf("%lx %d",&probeip.addr, &probeip.flag);
             //scanf("%d",&probeip.flag);
	     if(probeip.flag >1 || probeip.flag < 0)
	         probeip.flag = 0;
	     printf("\naddr = %lx -  %s\n",probeip.addr,probeip.flag==1?"registering":"unregistering");  
             pthread_mutex_lock(&wmutex);
	     res = write(fdes, &probeip, sizeof(struct kprobe_input));
	     if(res < 0)
	     {
	        perror("Can't perform probe.\n");		
	     }
             pthread_mutex_unlock(&wmutex);
             runflag=1;
          }
          else
          {
              printf("Waiting for threads to complete ...\n");
              runflag = 0; /* will exit other thread performing RBT operations */
          }
   }
   printf("\nThread %d: RBT Probe thread exiting.. \n",threadid);
   return (void *)0;
}



/* fuction to dump the RBT tree using the read system call */
static void display(int fd, int count)
{

    int retval,i;
    rbt_objt *buffptr;
    int selfid = syscall(SYS_gettid);
    printf("\n[Thread %d  ]: Read Operation on RBT\n",selfid);
    char *buffer = malloc(MAX_RBT_NODE*sizeof(rbt_objt));
    retval = read(fd, buffer, 200);
    printf("\n===================Tree(%d)========================\n",fd%2);
    buffptr = (rbt_objt *)buffer;
    int dispPerLine = 0;
    if(count == 1)
    {
        printf("READ ONE VALUE : -> [Key=%d Value=%d]\n",buffptr->key, buffptr->data);
    }
    else
    {
        printf("READ FULL TREE \n"); 
        for(i=0; i<retval/sizeof(rbt_objt); i++)
        {
            printf("[Key=%d Value=%d]\t",buffptr->key, buffptr->data);
            buffptr++;// += sizeof(rbt_objt);
            dispPerLine++;
            if(dispPerLine == 5)
            {
                printf("\n");
                dispPerLine = 0;
            }
       }
        
    }
    printf("\n==================================================\n");
    free(buffer);
}

/* thread function performing add and delete nodes using write system call 
*  burst: burst control the number of delete in one call
*/
void deletekey(int burst, int inputfd, int threadid, int keymax);
/* burst_count controls the number of nodes to be added in one call */
void addkey(int burst_count, int inputfd, int selfid, int keymax);

/* main thread function performing read/write operation on RBT tree 
*  it will perform 25 addition to the tree in the beginning and remaining 25
*  add operations  along with delte and display operations
*  so overall each thread will perform 50 add operations and random number of 
*  delete/display operations.
*/

static void *thread_func(void *arg)
{
        struct threadargs *arg1 = (struct threadargs *)arg;
        int temp;
        int inputfd = arg1->fd; /* RB tree driver fd */
        int sleeptime = arg1->randsleep;
        int pfd = arg1->probefd; /* Kprobe driver fd */
        int selfid = syscall(SYS_gettid);
        int randflag= 0;
        printf("\nThread created with ID: %d  \n",selfid);
        temp = arg1->count/4;  /*arg1->count maximum initial nodes per thread to be inserted */
        sleep (2);
        pthread_mutex_lock(&wmutex);
        addkey(temp, inputfd, selfid, arg1->count);
        pthread_mutex_unlock(&wmutex);
	while(temp && runflag)
	{
           randflag=rand()%4;
           pthread_mutex_lock(&wmutex);
           switch(randflag) {
               case 0:
                   deletekey(1, inputfd, selfid, arg1->count);
                   //temp++;
                   break;
               case 1: 
                   addkey(1, inputfd, selfid, arg1->count);
                   temp--;
                   showprobe(pfd);
                   break;
               case 2:
                   display(inputfd, arg1->count); 
                   showprobe(pfd);
                   break;
               case 3:
                   display(inputfd, 1);
                   showprobe(pfd);
                   break;
            }
            //printf("===================== temp i%d\n",temp);
            pthread_mutex_unlock(&wmutex);
            sleep(sleeptime);
        }
        printf("\nThread %d: Write/Delete/Display RBT Exiting.. after write/delete ops \n",selfid);
        return (void *)0;
}



void deletekey(int burst, int inputfd, int selfid, int keymax)
{
        int res;
        rbt_objt buff;
        /* perform delete */
        printf("\nThread %d: Random Delete RBT: ",selfid);
	while(burst)
	{
	   buff.key = rand()%keymax + 1; //random number between 1 to 50
	   buff.data = 0;
	   res = write(inputfd, &buff, sizeof(rbt_objt));
	   if(res < 0){
		perror("Can not write to the device file.\n");
	   }
           else
           {
               printf("[Key=%d  Value=%d]   ",buff.key, buff.data);
           }
           burst--;
       }
    return;
}

void addkey(int burst_count, int inputfd, int selfid, int keymax)
{

           rbt_objt buff;
           int res;
           printf("\nThread %d: Write RBT: ",selfid);
           while(burst_count)
           {
	       buff.key = rand()%keymax + 1; //random number between 1 to 50
	       buff.data = rand()%200;
	       res = write(inputfd, &buff, sizeof(rbt_objt));
	       if(res < 0)
                {
	          perror("Can't write to the device file.\n");		
	        }
                else
                {
                   printf("[Key=%d  Value=%d]   ",buff.key, buff.data);
                }
                //sleep(1);
                burst_count--;
           }
           printf("\n");

}

static void showprobe(int filedes)
{
       rb_probe *probebuff;
       int res,i,tempread;
       char *buffer, dispflag=0;

       if(filedes < 0)
           return;
       buffer = malloc(MAX_KPROBE_DATA * sizeof(rb_probe));
       memset(buffer, 0, MAX_KPROBE_DATA * sizeof(rb_probe));
       //pthread_mutex_lock(&wmutex);
       res = read(filedes, buffer, MAX_KPROBE_DATA * sizeof(rb_probe));
       //pthread_mutex_unlock(&wmutex);
       if(res < 0)
	   perror("Read failure");
       else
       {  
          tempread = res / sizeof(rb_probe);
          probebuff = (rb_probe *)buffer;
          
          for(i=0; i<tempread; i++)
          {
              if(probebuff->key > 0 && probebuff->key <= MAX_RBT_NODE)
                 dispflag=1;
          }


          if(dispflag)
              printf("\n=======Probe Result ===============\n");
          for(i=0; i<tempread; i++)
          {
              if(probebuff->key > 0 && probebuff->key <= MAX_RBT_NODE)
	         printf("[Timestamp= %llu ] [Address=%p ] [ProcessId=%d ]  [Key= %d Value= %d]\n", probebuff->tstamp, \
		      (void *) probebuff->addr, probebuff->pid, probebuff->key, probebuff->value);
              probebuff++;
          }
          if(dispflag)
              printf("========================================================\n");
       }
       free(buffer);
       return;
} 


    
