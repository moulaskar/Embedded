/* ----------------------------------------------- DRIVER rbt --------------------------------------------------
 
						RBT530 driver module
- Character Driver using kernel Red Black Tree implementation.
- Exposes multiple device nodes to manage corresponding RBT database.
- Nodes are created, Modifed and Deleted via rbt_driver_wirte() function. 
   - A node is deleted if the corresponding key found and is non-zero however the data is zeor
   - A node is overwritten if the corresponding key is found however the data field is non zero
- The tree is delete on device close.
 
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
#include <linux/rbtree.h>
#include<linux/init.h>
#include<linux/moduleparam.h>
#include "RBT_530.h"
#include "RBT_Nodes.h"

#define DEVICE_NAME                 "rbt530_dev"  // Device name to be created and registered
#define MAX_DEVICES  2                            // Maximum device instances of RBT530 module


static int flag=0;

struct rbt_dev *my_rbt_devp[MAX_DEVICES];
EXPORT_SYMBOL(my_rbt_devp);

static dev_t rbt_dev_number;      /* Allotted device number */


int rbt_insert_key(struct rb_root *root, rb_object_t *temp);   
static int rbt_show_tree(struct rb_root *root, char *);
static rb_object_t *rbt_handle_lookup(struct rb_root *root, rb_object_t *buffer);
int rbt_delete_key(struct rb_root *root, rb_object_t *temp);
struct class *rbt_dev_class;          /* Tie with the device model */


/*
* Open rbt driver
*/
int rbt_driver_open(struct inode *inode, struct file *file)
{
	struct rbt_dev *rbt_devp;
	rbt_devp = container_of(inode->i_cdev, struct rbt_dev, cdev);
	file->private_data = rbt_devp;
        rbt_devp->root_tree = RB_ROOT;
	printk("\n%s is openning \n", rbt_devp->name);
	return 0;
}


/*
 * Release rbt driver
 */
int rbt_driver_release(struct inode *inode, struct file *file)
{
	struct rbt_dev *rbt_devp = file->private_data;
        struct rb_root *root = &rbt_devp->root_tree;
        struct rb_node *n; 
        rb_object_t *entry;
        int count = 0;
        rbt_show_tree(&rbt_devp->root_tree, rbt_devp->name);
	printk("\n%s is closing\n", rbt_devp->name);
        for (n = rb_first(root); n; n = rb_next(n)) 
        {
           count++;
           entry = rb_entry(n, rb_object_t, node);
           kfree(entry);
        }
	return 0;
}

/*
 * Write to rbt driver
 * Performs write operation on RBTree 
 */
ssize_t rbt_driver_write(struct file *file, const char *buf,
           size_t count, loff_t *ppos)
{
	struct rbt_dev *rbt_devp = file->private_data;
        rb_object_t *rbtkey;	
	rbtkey = kmalloc(sizeof(rb_object_t), GFP_KERNEL);
        copy_from_user(rbtkey, buf, count);
        memcpy(&rbt_devp->last_node, rbtkey, sizeof(rb_object_t)); 
        if(rbtkey->data != 0)  //add the node
        {
           rbt_insert_key(&rbt_devp->root_tree, rbtkey);
        }
        else // data is zero so remove the key
        {
           rbt_delete_key(&rbt_devp->root_tree, rbtkey);
        }
	return 0;
}
EXPORT_SYMBOL(rbt_driver_write);

/*
 * Read to rbt driver
 * Read the RBTree instances either ascending or descending
 */
struct user_rbt_key{
  int key;
  int data;
};
ssize_t rbt_driver_read(struct file *file, char *buf,
           size_t count, loff_t *ppos)
{
	struct rbt_dev *rbt_devp = file->private_data;
        struct user_rbt_key *urbtkey=NULL;	
        struct rb_root *root = &rbt_devp->root_tree;
        struct rb_node *n; 
        int byte_read;
        int node_count=0;
        int retval;
        rb_object_t *entry;
        short lastnode_flag=1;
        //printk("Device struct address %lx    %lx\n",rbt_devp,&rbt_devp);
       
        for (n = rb_first(root); n; n = rb_next(n)) 
        {
           //entry = rb_entry(n, rb_object_t, node);
           node_count++;
        }
        //printk("inside read %d\n",node_count);
        if(node_count == 0)
          return 0;
	urbtkey = kmalloc(sizeof(struct user_rbt_key)*node_count, GFP_KERNEL);
	if(!urbtkey)
        {		
           printk("Kmalloc unsuccessful\n");
	   return -ENOMEM;
        }
        /* count the number of nodes in the tree */
        node_count = 0;
        byte_read=0;
        if(flag == 1) //accending order
        {
           for (n = rb_first(root); n; n = rb_next(n)) 
           {
               entry = rb_entry(n, rb_object_t, node);
               //memcpy(&urbtkey[node_count++], entry, sizeof(struct user_rbt_key));
               urbtkey[node_count].key = entry->key;
               urbtkey[node_count].data = entry->data;
               if(lastnode_flag)
               {
                  memcpy(&rbt_devp->last_node, entry, sizeof(rb_object_t)); 
                  //printk("emcpy value %d and key %d  ",rbt_devp->last_node.data, rbt_devp->last_node.key);

                  lastnode_flag = 0;
               }
               byte_read = byte_read + sizeof(struct user_rbt_key);
               node_count++;
           }
        }
        else //descending order
        {
           for (n = rb_last(root); n; n = rb_prev(n)) 
           {
               entry = rb_entry(n, rb_object_t, node);
               urbtkey[node_count].key = entry->key;
               urbtkey[node_count].data = entry->data;
               if(lastnode_flag)
               {
                  memcpy(&rbt_devp->last_node, entry, sizeof(rb_object_t)); 
                  lastnode_flag = 0;
                  printk("memcpy value %d and key %d  ",rbt_devp->last_node.data, rbt_devp->last_node.key);
               }
               //memcpy(&urbtkey[node_count++], entry, sizeof(struct user_rbt_key));
               byte_read = byte_read + sizeof(struct user_rbt_key);
               node_count++;
           }

        }  
        retval = copy_to_user(buf, (char *)urbtkey, byte_read);
        kfree(urbtkey);
        return byte_read;
}

EXPORT_SYMBOL(rbt_driver_read);

# define BUFF_SIZE 256

/*
* ioctl operations
* sets the flag for the tree to be read in ascending or descending order
* returns 0 on success
*/
static long rbt_driver_ioctl(struct file *file, unsigned int ioctl_num,
                                unsigned long ioctl_param) 
{

  char buffer[BUFF_SIZE];
  switch (ioctl_num) {
    case SET_DISPMODE:                    //this sets the flag
       copy_from_user(buffer, (unsigned long *)ioctl_param, BUFF_SIZE);
       printk("%s \n",buffer);
       if(strcmp("ascending",buffer) == 0)
           flag = 1;
       else
           flag = 0;
        break;
    default:
            return -EINVAL;
    }
    printk("inside ioctl %d\n",flag);
 
    return 0;
}


/*
* rbt_show_tree
* displays the tree in ascending or descending order based on flag set by user space application
* returns 0 on success
*/
static int rbt_show_tree(struct rb_root *root, char *treename)
{
        struct rb_node *n; 
        printk("\n------------For Tree %s------------------------\n",treename);
        if(flag == 1) //accending order
        {

           for (n = rb_first(root); n; n = rb_next(n)) 
           {
               rb_object_t *entry = rb_entry(n, rb_object_t, node);
               printk("[Key]=>%d  [Data]=>%d      ", entry->key, entry->data);
           }
       }
      else
      {
          for (n = rb_last(root); n; n = rb_prev(n)) 
          {
               rb_object_t *entry = rb_entry(n, rb_object_t, node);
               printk("%d  %d\n", entry->key, entry->data);
          }

      }
      printk("\n----------------------------------------------------\n");
      return 0;
}


/*
* rbt_insert_key 
* insert key on RBTree 
* returns 0 on success
*/
int rbt_insert_key(struct rb_root *root, rb_object_t *temp)
{
      struct rb_node **p = &(root->rb_node);
      struct rb_node *parent = NULL;
      rb_object_t *leaf;
      int key,value;
      while (*p) {
              parent = *p;
              leaf = rb_entry(parent, rb_object_t, node);
              key = leaf->key;
              value = leaf->data;
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
                     return 0;
              }
              if(p==NULL)
                printk("p has become null\n");
      }
      rb_link_node(&temp->node, parent, p);
      rb_insert_color(&temp->node, root);
      
      return 0;
}
EXPORT_SYMBOL(rbt_insert_key);


/*
* rbt_delete_key 
* delete the node for particular key value if data filed if 0
*  returns 0 on success
*/

int rbt_delete_key(struct rb_root *root, rb_object_t *temp)
{
      rb_object_t *nodetodel=NULL;
      nodetodel = rbt_handle_lookup(root, temp);
      if(nodetodel != NULL)
      { 
          rb_erase(&nodetodel->node,root);
      }
      return 1;
}



/*
* rbt_handle_lookup 
* searches the tree for a node 
* returns struct rb_object on success
*/
static rb_object_t *rbt_handle_lookup(struct rb_root *root,
                                            rb_object_t *buffer)
{
      struct rb_node *p = root->rb_node;//, *parent = NULL;
      //rb_object_t *leaf;

        while (p) {
                rb_object_t *entry = rb_entry(p, rb_object_t, node);

                if (buffer->key < entry->key)
                        p = p->rb_left;
                else if (buffer->key > entry->key)
                        p = p->rb_right;
                else
                        return entry;
        }
        return NULL;
}

/* File operations structure. Defined in linux/fs.h */
static struct file_operations rbt_fops = {
    .owner		= THIS_MODULE,           /* Owner */
    .open		= rbt_driver_open,        /* Open method */
    .release	        = rbt_driver_release,     /* Release method */
    .write		= rbt_driver_write,       /* Write method */
    .read		= rbt_driver_read,        /* Read method */
    .unlocked_ioctl     = rbt_driver_ioctl
};

/*
 * Driver Initialization
 */
int __init rbt_driver_init(void)
{
	int ret;
        int i;
        int major;
	dev_t my_device;

	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&rbt_dev_number, 0, MAX_DEVICES, DEVICE_NAME) < 0) {
			printk(KERN_DEBUG "Can't register device\n"); return -1;
	}

	/* Allocate memory for the per-device structure */
        major = MAJOR(rbt_dev_number);
	rbt_dev_class = class_create(THIS_MODULE, DEVICE_NAME);
        
        // creating and initializing devices
        for (i = 0; i < MAX_DEVICES; i++) {
	            my_rbt_devp[i] = kmalloc(sizeof(struct rbt_dev), GFP_KERNEL);
	            if (!my_rbt_devp[i]) {
		        printk("Bad Kmalloc\n"); return -ENOMEM; //try free all the memory before terurning
	            }
                    sprintf(my_rbt_devp[i]->name,"RBT-%d",i); 
		    my_device = MKDEV(major, i);
	            sprintf(my_rbt_devp[i]->name,"%s%d",DEVICE_NAME,i);
		    cdev_init(&my_rbt_devp[i]->cdev, &rbt_fops);
		    ret = cdev_add(&my_rbt_devp[i]->cdev, my_device, 1);
		    if (ret) {
			    pr_info("%s: Failed in adding cdev to subsystem "
					    "retval:%d\n", __func__, ret);
                            return ret;
		    }
		    else {
	                device_create(rbt_dev_class, NULL, my_device, NULL, DEVICE_NAME"%d",i);
		    }
	    }

	printk("rbt driver initialized. Major=%d\n",major);
	return 0;
}
/* Driver Exit */
void __exit rbt_driver_exit(void)
{
        int i;
        int major;
	/* Release the major number */

	/* Destroy device */
        major = MAJOR(rbt_dev_number);
        for (i = 0; i < MAX_DEVICES; i++) {
	   device_destroy (rbt_dev_class, MKDEV(major, i));
	   kfree(my_rbt_devp[i]);
	}
	class_destroy(rbt_dev_class);
        unregister_chrdev(major, DEVICE_NAME);
	printk("rbt driver removed.\n");
}

module_init(rbt_driver_init);
module_exit(rbt_driver_exit);
MODULE_LICENSE("GPL v2");
