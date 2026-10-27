#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/stat.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>

#include <asm/errno.h>
static int normal_device_open(struct inode *, struct file *);
static int normal_device_release(struct inode *, struct file *);
static ssize_t normal_device_read(struct file *, char *, size_t, loff_t *);
static ssize_t normal_device_write(struct file *, const char *, size_t, loff_t *);

#define DEVICE_NAME "normal_driver"
#define BUF_LEN 10

static int major = -1;
enum {
    DEVICE_NOT_OPEN = 0,
    DEVICE_EXCLUSIVE_OPEN = 1
};

static atomic_t device_already_opened = ATOMIC_INIT(DEVICE_NOT_OPEN);
static struct class *cls;

static struct file_operations device_fops = {
    .read = normal_device_read,
    .write = normal_device_write,
    .open = normal_device_open,
    .release = normal_device_release
};

static int auto_create = 1;
static char msg[BUF_LEN + 1];
static int msg_len = 0;
static char* input_msg;

static int minor = 0;
static int minor_count = 1;
static int register_option = 1;
static dev_t devno;
static struct cdev driver_cdev;
static struct device *device;

module_param(auto_create, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(auto_create, "Auto create device file at /dev/");
module_param(input_msg, charp, 0000);
MODULE_PARM_DESC(input_msg, "Init string user write into device to read after that");
module_param(major, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(major, "Driver major number");
module_param(minor, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(minor, "Driver minor number");
module_param(minor_count, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(minor_count, "Driver minor number count");
module_param(register_option, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP);
MODULE_PARM_DESC(register_option, "Driver register option: 1 for register_chrdev, \
    2 for register_chrdev_region and 3 for alloc_chrdev_region");

static int __init normal_device_register(void)
{
    int result = 0;
    if (auto_create) {
        pr_info("normal_device_register: device file will auto create at /dev/");
    }
    else {
        pr_info("normal_device_register: user has to manually mknod to create device file at /dev/");
    }

    if (input_msg) {
        pr_info("normal_device_register: init msg from device will be %s\n", input_msg);
        memcpy(msg, input_msg, strlen(input_msg));
    } 
    else {
        pr_info("normal_device_register: there's no init msg from the user\n");
        msg_len = sizeof("hello\n");
        memcpy(msg, "hello\n", msg_len);
    }
    switch (register_option) {
        case 1: 
            pr_info("normal_device_register: let's register by register_chrdev\n");
            major = register_chrdev(0, DEVICE_NAME, &device_fops);
            if (major < 0) {
                pr_alert("normal_device_register: register_chrdev failed: %d\n", major);
                result = major;
                goto err_out;
            }
            devno = MKDEV(major, minor);
            break;

        case 2: 
            pr_info("normal_device_register: let's register by alloc_chrdev_region\n");
            result = alloc_chrdev_region(&devno, minor, minor_count, DEVICE_NAME);
            if (result) {
                pr_err("normal_device_register: alloc_chrdev_region failed: %d\n", result);
                goto err_out;
            }
            major = MAJOR(devno);
            minor = MINOR(devno);
            break;

        case 3: 
            pr_info("normal_device_register: let's register by register_chrdev_region\n");
            if (major < 0) {
                pr_err("normal_device_register: you must set device major number if want to use register_chrdev_region\n");
                result = -EINVAL;
                goto err_out;
            }
            devno = MKDEV(major, minor);
            result = register_chrdev_region(devno, minor_count, DEVICE_NAME);
            if (result) {
                pr_err("normal_device_register: alloc_chrdev_region failed: %d\n", result);
                goto err_out;
            }
            break;

        default: 
            pr_err("normal_device_register: curruntly only support register_option 1/2/3");
            result = -EINVAL;
            goto err_out;
    }

    if (register_option != 1) {
        cdev_init(&driver_cdev, &device_fops);
        driver_cdev.owner = THIS_MODULE;
        result = cdev_add(&driver_cdev, devno, minor_count);
        if (result) {
            pr_err("normal_device_register: cdev_add failed: %d\n", result);
            goto err_unregister_chrdev;
        }
    }

    pr_info("normal_device_register: device registered, major=%d, minor=%d\n", major, minor);

    if (auto_create) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
        cls = class_create(DEVICE_NAME);
#else
        cls = class_create(THIS_MODULE, DEVICE_NAME);
#endif
        if (IS_ERR(cls)) {
            result = PTR_ERR(cls);
            cls = NULL;
            pr_err("normal_device_register: class_create failed %d\n", result);
            goto err_cdev_del;
        }
        device = device_create(cls, NULL, devno, NULL, DEVICE_NAME);
        if (IS_ERR(device)) {
            result = PTR_ERR(device);
            device = NULL;
            pr_err("normal_device_register: device_create failed: %d\n", result);
            goto err_class_destroy;
        }
        pr_notice("normal_device_register: device created on /dev/%s\n", DEVICE_NAME);
    }
    return 0;
err_class_destroy:
    if (!IS_ERR_OR_NULL(cls))
    {
        class_destroy(cls);
        cls = NULL;
    }
err_cdev_del:
    if (register_option != 1) {
        cdev_del(&driver_cdev);
    }
err_unregister_chrdev:
    if (devno)
    {
        if (register_option == 1) {
            unregister_chrdev(major, DEVICE_NAME);
        }
        else {
            unregister_chrdev_region(devno, minor_count);
        }
        devno = 0;
    }
err_out:
    return result;
}

static void __exit normal_device_unregister(void)
{
    if (!IS_ERR_OR_NULL(device)) {
        device_destroy(cls, devno);
        device = NULL;
    }

    if (!IS_ERR_OR_NULL(cls)) {
        class_destroy(cls);
        cls = NULL;
    }
    
    if (register_option == 1) {
        unregister_chrdev(major, DEVICE_NAME);
    }
    else {
        cdev_del(&driver_cdev);
        if (devno)
        {
            unregister_chrdev_region(devno, minor_count);
            devno = 0;
        }
    }
    pr_notice("normal_device_unregister: driver unregistered\n");
}

static int normal_device_open(struct inode *inode, struct file *file)
{
    static int count = 0;
    if (atomic_cmpxchg(&device_already_opened, DEVICE_NOT_OPEN, DEVICE_EXCLUSIVE_OPEN) != DEVICE_NOT_OPEN) {
        pr_alert("normal_device_open: device is already open\n");
        return -EBUSY;
    }
    pr_info("normal_device_open: device opened %d times\n", ++count);
    return 0;
}

static int normal_device_release(struct inode *inode, struct file *file)
{
    atomic_set(&device_already_opened, DEVICE_NOT_OPEN);
    pr_info("normal_device_release: device released\n");
    return 0;
}

static ssize_t normal_device_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
    if (*offset < 0) {
        return -EINVAL;
    }

    if (*offset >= msg_len) {
        return 0;
    }

    if (length > msg_len - *offset) {
        length = msg_len - *offset;
    }

    int result;
    if ((result = copy_to_user(buffer, msg + *offset, length)) != 0) {
        pr_err("normal_device_read: copy_to_user failed %d\n", result);
        return -EFAULT;
    }

    *offset += length;
    return length;
}

static ssize_t normal_device_write(struct file *file, const char *buffer, size_t length, loff_t *offset) {
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
        pr_alert("normal_device_write: input exceeds buffer size, only write the string in limit\n");
        length = BUF_LEN - 1 - *offset;
    }

    int result;
    if ((result = copy_from_user(msg + *offset, buffer, length)) != 0) {
        pr_err("normal_device_write: copy_from_user failed %d\n", result);
        return -EFAULT;
    }
    if (*offset == 0) {
        msg_len = length;
    }
    else if (*offset + length > msg_len) {
        msg_len = *offset + length;
    }

    msg[msg_len] = '\0';
    *offset += length;

    pr_info("normal_device_write: write %d bytes\n", (int)length);
    return length;
}

module_init(normal_device_register);
module_exit(normal_device_unregister);

MODULE_AUTHOR("godminhtri");
MODULE_DESCRIPTION("normal driver");
MODULE_LICENSE("GPL");