#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <errno.h>
#include <pthread.h>

#define MAX_SYMBOL 4 
#define MAX_MODE 3

#ifdef  FS_SYSCALL
#define TEST_SYMBOL1 "sys_open"
#define TEST_SYMBOL2 "sys_read"
#define TEST_SYMBOL3 "sys_time"
#define TEST_SYMBOL4 "sys_write"
#endif

#define OTHER_SYSCALL 1

#ifdef OTHER_SYSCALL
#define TEST_SYMBOL1 "sys_getcpu"
#define TEST_SYMBOL2 "sys_time"
#define TEST_SYMBOL3 "sys_stime"
#define TEST_SYMBOL4 "__ksymtab_fuse_file_poll"    //this symbol wont get registered as its not in text segment in System.map
#endif

pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
struct symbol_index
{

   char *symbol; 
   unsigned int index;
};

//mode > 1 is for all the processes, so its not seperately tested, as any process (eg journal) who opens a file
// dumps the stack

unsigned int mode[MAX_MODE] = { 0, 1, 2};

void syscall_test(int pid)
{
#ifdef  FS_SYSCALL
     int fd;
     char buff[50];
     int ret;
     printf("Testing for pid %d\n",pid);
     fd = open("/home/root/test", O_CREAT|O_RDWR);
     if(0 > fd)
        perror("open failed\n");
     ret = write(fd,"abcd",4);
     if(0 > ret)
        perror("write failed\n");
        
     ret = read(fd,buff,5);
     if(0 > ret)
        perror("read failed\n");
     if(close(fd)<0){
	printf("Cant close file for pid %d\n",pid);
     
     return
#else
     unsigned cpu, node;
     time_t t;
     syscall(SYS_getcpu, &cpu, &node, NULL);
     syscall(SYS_time, &t);
     syscall(SYS_stime, &t);
#endif 

}
/*
m => which process can use this trace
m = 0; only the process who registered symbol with kprobe
m = 1; process who registered and all other process
Two threads tests for m=0 and m=1.
m > 1; all the process; all the process includes all process as seen in ps
       if sys_open is used; we can see many dump_stack traces from processes in ps which is trying to open a file
*/
void  * test_func()
{
    
     int ret = 0;
     int count = 0;
     int r = 0;
     unsigned int m;
  
     struct symbol_index sym_idx[MAX_SYMBOL] = {
         { .symbol = TEST_SYMBOL1 , .index = 0 },
         { .symbol = TEST_SYMBOL2 , .index = 0 },
         { .symbol = TEST_SYMBOL3 , .index = 0 },
         { .symbol = TEST_SYMBOL4 , .index = 0 },
     };
 
     srand(time(NULL));
     pid_t tid = syscall(SYS_gettid);
     
     while(count < MAX_SYMBOL)
     {
         r = rand() % MAX_SYMBOL;
         //m = mode[r % MAX_MODE];
         m = 1;
         pthread_mutex_lock( &mutex1);
         //ret = syscall(359, sym_idx[r].symbol, &m);
         ret = syscall(359, sym_idx[count].symbol, &m);
         if (ret > 0)
         {
            sym_idx[count].index = ret;
            printf("Successfully added Symbol: %s with %d for pid = %d\n",sym_idx[count].symbol, sym_idx[count].index,tid);
         }
         else
         {
            printf("Unsuccessfull in adding Symbol: %s with %d for pid = %d\n",sym_idx[count].symbol, sym_idx[count].index,tid);
         }
         pthread_mutex_unlock( &mutex1 );
         sleep(2);
         count++;
     }
     //pthread_mutex_lock( &mutex1);
     printf("Starting to test pid %d\n",tid);
     syscall_test(tid);
     sleep(2);
     syscall_test(tid);
     sleep(2);
     syscall_test(tid);
     sleep(2);
     syscall_test(tid);
     sleep(2);
     //pthread_mutex_unlock( &mutex1 );
     sleep(2);

#if  1 /* perfrom some random deltes */
     printf("Testing Removing nodes\n");
     count = 0;
     while(count < 2)
     {
         r = rand() % MAX_SYMBOL;
         if(sym_idx[r].index > 0) 
         {
             pthread_mutex_lock( &mutex1);
             ret = syscall(360, sym_idx[r].index);
             if(ret == 0){
                 printf("Successfully random removed %s with %d for pid = %d\n",sym_idx[r].symbol, sym_idx[r].index,tid);
                 count++;
             }
             pthread_mutex_unlock( &mutex1 );
         }
     }
     sleep(2);
#endif
     /* deleting all dump_stack probes for this pid while exiting */
     syscall(360,0);
     return 0; 
     
}

pthread_t td1,td2;
int main()
{
    pthread_create(&td1, NULL, test_func, NULL);
    pthread_create(&td2, NULL, test_func, NULL);
    pthread_join(td1,NULL);
    pthread_join(td2,NULL);
    
    return 0;
}
