#ifndef __IOCTL_H__
#define __IOCTL_H__

#include <linux/ioctl.h>

struct ioctl_param {
    char *msg;
};

#define IOCTL_MAGIC         'T'
#define IOCTL_SET_MSG       _IOW(IOCTL_MAGIC, 0, char *)
#define IOCTL_GET_MSG       _IOR(IOCTL_MAGIC, 1, char *)
#define IOCTL_GET_NTH_BYTE  _IOR(IOCTL_MAGIC, 2, int)

#define IOCTL_VAL_MAXNR     2
#define DEVICE_NAME         "ioctl_chrdev"
#define DEVICE_PATH 	    "/dev/ioctl_chrdev"

#endif /* __IOCTL_H__ */
