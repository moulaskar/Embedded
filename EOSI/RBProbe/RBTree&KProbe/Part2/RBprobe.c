/* ----------------------------------------------- DRIVER rbprobe --------------------------------------------------
 
						RBprobe  
- Kprobe driver to probe the RBTree.
- Provdes a char device interface for a userspace application to  register and deregister multiple probes address.
- Pre-registers two instruction address from RBTree driver 1) key entry 2) key read 
 
 ----------------------------------------------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include <linux/kprobes.h>
#include<linux/init.h>
#include<linux/moduleparam.h>
#include <linux/uaccess.h>
//#include <linux/rbtree.h>
//#include "RBT_530.h"
#include "RBT_Nodes.h"
#define DEVICE_NAME                 "RBprobe"  // device name to be created and registered


#define MAX_KP 3                               // Maximun kprobe address to be registerd
#define MAX_USER_BUFFER 10                     // Maximun number of nodes to be read by probe

/* rbobject info to be passed to user */

struct rb_object_info {
    uint64_t timestamp;
    unsigned long addr;
    pid_t pid;
    int key;
    int value;
};


/* per device structure */
struct rbprobe_dev_info {
	struct cdev cdev;               /* The cdev structure */
	char name[20];                  /* Name of device*/
	char in_string[256];	        /* buffer for the input string i,e to read the address */
	int current_write_pointer;
} *rbprobe_devp;

/* kprobe input struct from user 
* address and flag to register or deregister
*/
struct kprobe_input{
    unsigned long addr;
    int flag;
};

static struct rb_object_info probe_info[MAX_USER_BUFFER];

static dev_t rbprobe_dev_info_number;      /* Allotted device number */
struct class *rbprobe_dev_info_class;          /* Tie with the device model */
static struct device *rbprobe_dev_info_device;

long address = 0x0;
static char *func_name = "RBrobe";  /* the default user name, can be replaced if a new name is attached in insmod command */

static struct kprobe kp[MAX_KP];    /*number of kprobes to be registered*/
int kpindex=0;

static int default_addr_probe(void);

/*Time Stamp Counter code*/
#if defined(__i386__)

static __inline__ unsigned long long rdtsc(void)
{
  unsigned long long int x;
     __asm__ volatile (".byte 0x0f, 0x31" : "=A" (x));
     return x;
}
#elif defined(__x86_64__)


static __inline__ unsigned long long rdtsc(void)
{
  unsigned hi, lo;
  __asm__ __volatile__ ("rdtsc" : "=a"(lo), "=d"(hi));
  return ( (unsigned long long)lo)|( ((unsigned long long)hi)<<32 );
}

#endif


static int userbuff = 0;

/* RBprobe pre handler 
*  Probe on Insert key operation is handled here 
*/
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
  
#if 1
     if(p->addr == kp[0].addr) //only for insert operation
     {
     
        rb_object_t *obj = (struct rb_object *)(void *)regs->dx;
         if(userbuff >= MAX_USER_BUFFER)
              userbuff = 0;
         probe_info[userbuff].timestamp = rdtsc();
         probe_info[userbuff].pid = task_pid_nr(current);
         probe_info[userbuff].value = obj->data;
         probe_info[userbuff].key = obj->key;
         probe_info[userbuff].addr = (unsigned long)p->addr;
         userbuff++;

   }
#if 0
   else if(p->addr == kp[1].addr)
   {
       struct file *rbtfp = (struct file *)(void *)regs->dx; 
       struct rbt_dev *devobj =  (struct rbt_dev *)rbtfp->private_data;
       printk("%s\n",devobj->name);

   }
#endif
#endif
     return 0;
}

#define MAX_DEVICES 2
struct rbt_dev *get_rb_pointer(struct file *file)
{
    return file->private_data;
}

extern struct rbt_dev *my_rbt_devp[MAX_DEVICES];
void populate_probe_info(struct rb_root *root, rb_object_t *temp, struct kprobe *p);

	
/* RBprobe post handler 
*  Probe on Read operation is handled here 
*/
static void handler_post(struct kprobe *p, struct pt_regs *regs,
       unsigned long flags)
{
     struct rbt_dev **rbt_dev1;// = get_rb_pointer((struct file *) regs->r14);
     struct rb_root *currobj;// =  (struct rb_node *)(void *)regs->r12;
     struct rb_object *rbtobj;// =  (struct rb_node *)(void *)regs->r12;
     if(p->addr == kp[1].addr) //rbt_device read
     {
        // For Reading the tree
        if (kallsyms_lookup_name("my_rbt_devp") == 0)
        {
            printk("NULL rbt_insert_key dereference");
            return;
        }
	printk("lookup : %p \n", (void **)(kallsyms_lookup_name("my_rbt_devp")));
	rbt_dev1 = (struct rbt_dev **)(void **)(kallsyms_lookup_name("my_rbt_devp"));
	printk("value %d and key %d  ",(*rbt_dev1)->last_node.data, (*rbt_dev1)->last_node.key);
	currobj = &(*rbt_dev1)->root_tree;
	rbtobj = &(*rbt_dev1)->last_node;
	populate_probe_info(currobj, rbtobj, p);
     }
     else  if(p->addr != kp[0].addr) //for instruction which are not preinstalled 
     {
         // For any other probe location input from user
         if(userbuff >= MAX_USER_BUFFER)
              userbuff = 0;
         probe_info[userbuff].timestamp = rdtsc();
         probe_info[userbuff].pid = task_pid_nr(current);
         probe_info[userbuff].value = regs->ax;
         probe_info[userbuff].key = regs->cs;
         probe_info[userbuff].addr = (unsigned long)p->addr;
         userbuff++;

    }
    return; 
}

/*
* Read MAX_USER_BUFFER number of nodes from the RBTree
*/
void populate_probe_info(struct rb_root *root, rb_object_t *temp, struct kprobe *k)
{
      struct rb_node **p = &(root->rb_node);
      struct rb_node *parent = NULL;
      rb_object_t *leaf;
      while (*p) 
      {
         if(userbuff >= MAX_USER_BUFFER)
              userbuff = 0;
         parent = *p;
         leaf = rb_entry(parent, rb_object_t, node);
         probe_info[userbuff].timestamp = rdtsc();
         probe_info[userbuff].pid = task_pid_nr(current);
         probe_info[userbuff].value = leaf->data;
         probe_info[userbuff].key = leaf->key;
         probe_info[userbuff].addr = (unsigned long)k->addr;
         userbuff++;
         if (leaf->key > temp->key)
         {
            p = &((*p)->rb_left);
         }
         else if (leaf->key < temp->key)
         {
            p = &((*p)->rb_right);
         }
         else // keys are same then replace the data
         {
            leaf->data = temp->data;
            return;
         }
      }
      return;
}



/*
* Open rbprobe driver
*/
int rbprobe_driver_open(struct inode *inode, struct file *file)
{
	struct rbprobe_dev_info *rbprobe_devp;

	/* Get the per-device structure that contains this cdev */
	rbprobe_devp = container_of(inode->i_cdev, struct rbprobe_dev_info, cdev);


	file->private_data = rbprobe_devp;
	printk("\n%s is openning \n", rbprobe_devp->name);
	return 0;
}

/*
 * Release rbprobe driver
 */
int rbprobe_driver_release(struct inode *inode, struct file *file)
{
	return 0;
}

/*
 * Write to rbprobe driver
 * Retuns 0 on success 
 */

ssize_t rbprobe_driver_write(struct file *file, const char *buf,
           size_t count, loff_t *ppos)
{
	/*struct rbprobe_dev_info *rbprobe_devp = file->private_data;	*/
        struct kprobe_input input;
        int retval;
        int i;
        copy_from_user(&input, buf, count);
        printk("flag:address=0x%lu flag=%d\n",input.addr, input.flag);
        
        // input.flag = 1 => regitering the address 
        if(input.flag) 
        {
           for(i=0; i<MAX_KP; i++)
           {  
             
              if(kp[i].addr == (kprobe_opcode_t *) input.addr)
              {
                  printk("Address already in use\n");
                  return -1;
              }
           }
           /* first unregister the older address */
           if(kpindex >= MAX_KP)
               kpindex = 2; /* first two are reserved for default instructions hardcoded in code*/ 
           
           unregister_kprobe(&kp[kpindex]); /* fist unregistering previous instruction */
           kp[kpindex].addr = (kprobe_opcode_t *) input.addr;
           kp[kpindex].pre_handler = handler_pre;
           kp[kpindex].post_handler = handler_post;
           printk("Registertering kprobe now for address %lx\n",(unsigned long)kp[kpindex].addr);
           if ((retval  == register_kprobe(&kp[kpindex])) < 0) {
               printk("Can't register kprobe on 0x%lu\n",input.addr);
               return retval;
           }
           kpindex++;
        }
        else if(input.flag == 0) /* delete an registered probe point , deregistering*/
        {
           for(i=0; i<MAX_KP; i++)
           {  
              if(kp[i].addr == (kprobe_opcode_t *) input.addr)
              {
                  printk("Unregistering address 0x%lu\n",input.addr);
                  kp[i].pre_handler = NULL;
                  kp[i].post_handler = NULL;
                 
                  unregister_kprobe(&kp[i]);
                  kp[i].addr = (kprobe_opcode_t *)0;
              }
           }
        }
        else {
            printk("Not at valid input\n");
            return EINVAL;
        }
        
	return 0;
}

/*
 * Read to rbprobe driver
 * Return: On success returns the number of bytes read
 */
ssize_t rbprobe_driver_read(struct file *file, char *buf,
           size_t count, loff_t *ppos)
{
        
	int bytes_read = 0;
	printk("Inside rbProbe read\n");
	if (copy_to_user(buf, &probe_info, MAX_USER_BUFFER * sizeof(struct rb_object_info)))
        { 
            printk("unable to copy to  the user");
	    return -EFAULT;
	}
	memset(&probe_info, 0, MAX_USER_BUFFER * sizeof(struct rb_object_info));
	bytes_read = sizeof(probe_info);
	return bytes_read;

}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations rbprobe_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= rbprobe_driver_open,        /* Open method */
    .release	        = rbprobe_driver_release,     /* Release method */
    .write		= rbprobe_driver_write,       /* Write method */
    .read		= rbprobe_driver_read,        /* Read method */
};

/* Adds default probing points at probe index 0,1 for read and insert instructions */

static int default_addr_probe(void)
{

       int retval;
       if (kallsyms_lookup_name("rbt_insert_key") == 0)
       {
           printk("NULL rbt_insert_key dereference");
           return -1;
       }
       // hardcoded to address "rbt_insert_key" + 0x27 instruction for insert key
       kp[kpindex].pre_handler = handler_pre;
       kp[kpindex].post_handler = handler_post;
       printk("probing rbt_insert_key \n");
       printk("lookup : %pi \n", (void **)(kallsyms_lookup_name("rbt_insert_key")));
       kp[kpindex].addr = (kprobe_opcode_t *)(void **)kallsyms_lookup_name("rbt_insert_key") + 0x27; 
       retval = register_kprobe(&kp[kpindex]);
       if (retval < 0) 
       {
   	   printk("register_kprobe error for rbt_insert_key\n");
           //return -1;
       }
       kpindex++;
       // hardcoded to address "rbt_driver_read" for read operation
       if (kallsyms_lookup_name("rbt_driver_read") == 0)
       {
           printk("NULL rbt_driver_key dereference");
       }
       kp[kpindex].pre_handler = handler_pre;
       kp[kpindex].post_handler = handler_post;
       printk("Probing rbt_driver_read \n");
       printk("lookup : %pi \n", (void **)(kallsyms_lookup_name("rbt_driver_read")));
       kp[kpindex].addr = (kprobe_opcode_t *)(void **)kallsyms_lookup_name("rbt_driver_read"); //read regs->r12
       retval = register_kprobe(&kp[kpindex]);
       kpindex++;
       if (retval < 0) {
   	    printk("register_kprobe error for rbt_driver_read\n");
  	}
  	return 0;
	
}


/*
 * Driver Initialization
 */
int __init rbprobe_driver_init(void)
{
	int ret;
	int time_since_boot;

        printk("RBprobe driving init\n");
	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&rbprobe_dev_info_number, 0, 1, DEVICE_NAME) < 0) {
			printk(KERN_DEBUG "Can't register device\n"); return -1;
	}

	/* Populate sysfs entries */
	rbprobe_dev_info_class = class_create(THIS_MODULE, DEVICE_NAME);

	/* Allocate memory for the per-device structure */
	rbprobe_devp = kmalloc(sizeof(struct rbprobe_dev_info), GFP_KERNEL);
		
	if (!rbprobe_devp) {
		printk("Bad Kmalloc\n"); return -ENOMEM;
	}

	/* Request I/O region */
	sprintf(rbprobe_devp->name, DEVICE_NAME);

	/* Connect the file operations with the cdev */
	cdev_init(&rbprobe_devp->cdev, &rbprobe_fops);
	rbprobe_devp->cdev.owner = THIS_MODULE;

	memset(rbprobe_devp->in_string, 0, 256);

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&rbprobe_devp->cdev, (rbprobe_dev_info_number), 1);

	if (ret) {
		printk("Bad cdev\n");
		return ret;
	}

	/* Send uevents to udev, so it'll create /dev nodes */
	rbprobe_dev_info_device = device_create(rbprobe_dev_info_class, NULL, MKDEV(MAJOR(rbprobe_dev_info_number), 0), NULL, DEVICE_NAME);		

	time_since_boot=(jiffies-INITIAL_JIFFIES)/HZ;//since on some systems jiffies is a very huge uninitialized value at boot and saved.
	sprintf(rbprobe_devp->in_string, "Hi %s, this machine has been on for %d seconds", func_name, time_since_boot);
	
	rbprobe_devp->current_write_pointer = 0;
  
        default_addr_probe();

	printk("rbprobe driver initialized.\n'%s'\n",rbprobe_devp->in_string);
	return 0;
}
/* Driver Exit */
void __exit rbprobe_driver_exit(void)
{
	// device_remove_file(rbprobe_dev_info_device, &dev_attr_xxx);
	/* Release the major number */
        int i;
        printk("Unregistering driver\n");
        for(i=0; i<MAX_KP; i++)
           unregister_kprobe(&kp[i]);
	unregister_chrdev_region((rbprobe_dev_info_number), 1);

	/* Destroy device */
	device_destroy (rbprobe_dev_info_class, MKDEV(MAJOR(rbprobe_dev_info_number), 0));
	cdev_del(&rbprobe_devp->cdev);
	kfree(rbprobe_devp);

        //unregister_kprobe(&kp);
	
	/* Destroy driver_class */
	class_destroy(rbprobe_dev_info_class);

	printk("rbprobe driver removed.\n");
}

module_init(rbprobe_driver_init);
module_exit(rbprobe_driver_exit);
MODULE_LICENSE("GPL v2");
