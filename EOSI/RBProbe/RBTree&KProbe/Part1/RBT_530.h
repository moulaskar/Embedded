/*IOCTL */

#ifndef RBT_IOCTL_H
#define RBT_IOCTL_H
#include <linux/ioctl.h>
#define RBT_IOCTL_TYPE 'R'
#define SET_DISPMODE _IOW(RBT_IOCTL_TYPE, 2, char *)

#endif
