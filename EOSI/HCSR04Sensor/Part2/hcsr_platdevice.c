/*
 * A sample program to show the binding of platform driver and device.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include "hcsr_platdriver.h"

/* typically called from Device tree */
struct sampleChip {
    char name[64];
    struct platform_device plf_dev;
};

struct sampleChip *phcsrChip=NULL;
/* max_device is passed as module argument but bounded by MAX_DEVICE */
int max_device = MAX_DEVICE;

/* 
 * hcsr_platdevice_release
 * addeing release funcation otherwise kernel complains
 */
static void hcsr_platdevice_release(struct device *dev)
{
	 
}

int __init hcsr_platdevice_init(void){
    int retval = 0;
    int i;
    int tempsize;
    /* boundary check */
    if(max_device < 0 || max_device > MAX_DEVICE)
        max_device = MAX_DEVICE;

    /* allocate and initialize the platform device structure*/
    tempsize = max_device * sizeof(struct sampleChip );
    phcsrChip = kmalloc(tempsize, GFP_KERNEL);
    if (!phcsrChip)
    {
       return -ENOMEM; //try free all the memory before terurning
    }
    else
    {
        memset(phcsrChip, 0, tempsize);
    }

    for (i = 0; i < max_device; i++)
    {
       sprintf(phcsrChip[i].name,"%s%d",DEVICE_NAME,i+1);
       phcsrChip[i].plf_dev.name =  phcsrChip[i].name;
       phcsrChip[i].plf_dev.id = -1;
       phcsrChip[i].plf_dev.dev.release = hcsr_platdevice_release;
       retval=platform_device_register(&phcsrChip[i].plf_dev);
    }
    printk("Module Platfrom device initalized successfully...\n");
    return retval;
}
/*
 * hcsr_platdevice_exit 
 */
void __exit hcsr_platdevice_exit(void)
{
    int i;
    /* allocate and initialize the platform device structure*/
    for (i=0; i<max_device; i++)
    {
       platform_device_unregister(&phcsrChip[i].plf_dev);
    }
    kfree(phcsrChip);
    printk("Module Platfrom device removed successfully...\n");
    return;
}

module_param(max_device, int, 0660);
module_init(hcsr_platdevice_init);
module_exit(hcsr_platdevice_exit);
MODULE_LICENSE("GPL v2");
