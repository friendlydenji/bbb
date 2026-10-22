#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include <asm/errno.h>

#include "ioctl.h"

#define DEVICE_NAME "ioctl_chrdev"
#define BUF_LEN 20

enum {
    DEVICE_NOT_OPEN = 0,
    DEVICE_EXCLUSIVE_OPEN = 1
};

static atomic_t device_already_opened = ATOMIC_INIT(DEVICE_NOT_OPEN);

static int ioctl_driver_major = 0;
static int ioctl_driver_minor;
static int number_of_device = 1;
static dev_t devno;
static struct cdev ioctl_driver_cdev;
static struct device *device;
static struct class *cls;

static char msg[BUF_LEN + 1] = "init msg\n";
static int msg_len = sizeof("init msg\n");

static int ioctl_chrdev_open(struct inode *inode, struct file *filp)
{
    pr_info("ioctl_chrdev_open: device's opened\n");

    return 0;
}

static int ioctl_chrdev_released(struct inode *inode, struct file *filp)
{
    pr_info("ioctl_chrdev_release: device's released\n");

    return 0;
}

static ssize_t ioctl_chrdev_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
    int bytes_read = 0;

    if (*offset < 0) {
        return -EINVAL;
    }

    if (*offset > msg_len) {
        return 0;
    }

    if (length > msg_len - *offset) {
        length = msg_len - *offset;
    }

    const char *msg_ptr = msg + *offset;

    while (length && *msg_ptr) {
        put_user(*(msg_ptr ++), buffer ++);
        -- length;
        ++ bytes_read;
    }

    pr_info("ioctl_chrdev_read: read %d bytes, %u left\n", bytes_read, length);

    *offset += bytes_read;

    return bytes_read;
}

static ssize_t ioctl_chrdev_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset)
{
    if (*offset < 0) {
        return -EINVAL;
    }

    if (length == 0) {
        return 0;
    }
        
    if (*offset > msg_len) {
       return -EINVAL;
    }

    if (*offset >= BUF_LEN - 1) {
        return -ENOSPC;
    }

    if (length > BUF_LEN - 1 - *offset) {
        pr_alert("ioctl_chrdev_write: input exceeds buffer size, only write the string in limit\n");
        length = BUF_LEN - 1 - *offset;
    }

    int i;

    for (i = 0; i < length; ++ i) {
        get_user(msg[*offset + i], buffer + i);
    }

    pr_info("ioctl_chrdev_write: write %d bytes\n", i);

    return i;
}

static long ioctl_chrdev_ioctl(struct file *filp, unsigned int ioctl_command, unsigned long ioctl_param)
{
    int i;
    long retval = 0;

    if (atomic_cmpxchg(&device_already_opened, DEVICE_NOT_OPEN, DEVICE_EXCLUSIVE_OPEN)) {
        pr_err("ioctl_chrdev_ioctl: device currently ioctl-ed by other process");
        return -EBUSY;
    }
	
    loff_t offset;
    switch (ioctl_command) {
        case IOCTL_SET_MSG:
            char __user *tmp = (char __user *)ioctl_param;
            char ch;

            get_user(ch, tmp);
            for (i = 0; ch && i < BUF_LEN; i ++, tmp ++) {
                get_user(ch, tmp);
            }
            
	    offset = 0;
            ioctl_chrdev_write(filp, (char __user *)ioctl_param, i, &offset);
            pr_info("ioctl_chrdev_ioctl: IOCTL_SET_MSG succeed\n");
            break;
        case IOCTL_GET_MSG:
            offset = 0;    
            
            i = ioctl_chrdev_read(filp, (char __user *)ioctl_param, BUF_LEN, &offset);
            
            put_user('\0', (char __user *)ioctl_param + i);
            pr_info("ioctl_chrdev_ioctl: IOCTL_GET_MSG succeed\n");
            break;
        case IOCTL_GET_NTH_BYTE:
            if (ioctl_param > BUF_LEN) {
                pr_err("ioctl_chrdev_ioctl: IOCTL_GET_NTH_BYTE failed because exceed index\n");
                return -EINVAL;
            }
            retval = (long)msg[ioctl_param];
            pr_info("ioctl_chrdev_ioctl: IOCTL_GET_NTH_BYTE succeed: %c\n", (char)retval);
            break;
    }
    atomic_set(&device_already_opened, DEVICE_NOT_OPEN);

    return retval;
}

static struct file_operations ioctl_ops = {
    .owner = THIS_MODULE,
    .read = ioctl_chrdev_read,
    .write = ioctl_chrdev_write,
    .open = ioctl_chrdev_open,
    .release = ioctl_chrdev_released,
    .unlocked_ioctl = ioctl_chrdev_ioctl,
};

static int __init ioctl_chrdev_init(void)
{
    int retval = 0;

    retval = alloc_chrdev_region(&devno, 0, number_of_device, DEVICE_NAME);

    if (retval) {
        goto err_out;
    }

    ioctl_driver_major = MAJOR(devno);
    ioctl_driver_minor = MINOR(devno);

    cdev_init(&ioctl_driver_cdev, &ioctl_ops);
    retval = cdev_add(&ioctl_driver_cdev, devno, number_of_device);
    if (retval) {
        goto err_unregister_chrdev_region;
    }
    pr_alert("ioctl_chrdev_init: driver with major %d minor %d installed\n", ioctl_driver_major, ioctl_driver_minor);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    cls = class_create(DEVICE_NAME);
#else
    cls = class_create(THIS_MODULE, DEVICE_NAME);
#endif

    if (IS_ERR(cls)) {
        retval = PTR_ERR(cls);
        cls = NULL;
        pr_err("ioctl_chrdev_init: class_create failed %d\n", retval);
        goto err_cdev_del;
    }

    device = device_create(cls, NULL, devno, NULL, DEVICE_NAME);
    if (IS_ERR(device)) {
        retval = PTR_ERR(device);
        device = NULL;
        pr_err("ioctl_chrdev_init: device_create failed: %d\n", retval);
        goto err_class_destroy;
    }

    pr_info("ioctl_chrdev_init: device created at /dev/%s\n", DEVICE_NAME);
    return 0;
err_class_destroy:
    if (!IS_ERR_OR_NULL(cls)) {
        class_destroy(cls);
        cls = NULL;
    }
err_cdev_del:
    cdev_del(&ioctl_driver_cdev);
err_unregister_chrdev_region:
    unregister_chrdev_region(devno, number_of_device);
    devno = 0;
err_out:
    return retval;
}

static void __exit ioctl_chrdev_exit(void)
{
    if (!IS_ERR_OR_NULL(device)) {
        device_destroy(cls, devno);
        device = NULL;
    }

    if (!IS_ERR_OR_NULL(cls)) {
        class_destroy(cls);
        cls = NULL;
    }

    cdev_del(&ioctl_driver_cdev);
    unregister_chrdev_region(devno, number_of_device);
    devno = 0;
    pr_alert("ioctl_chrdev_exit: driver uninstalled\n");
}

module_init(ioctl_chrdev_init);
module_exit(ioctl_chrdev_exit);

MODULE_AUTHOR("godminhtri");
MODULE_DESCRIPTION("ioctl shell");
MODULE_LICENSE("GPL");
