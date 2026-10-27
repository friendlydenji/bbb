#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/version.h>

struct ioctl_arg {
    unsigned int val;
};

#define IOC_MAGIC '\x66'
#define IOCTL_VALSET _IOW(IOC_MAGIC, 0, struct ioctl_arg)
#define IOCTL_VALGET _IOR(IOC_MAGIC, 1, struct ioctl_arg)
#define IOCTL_VALGET_NUM _IOR(IOC_MAGIC, 2, int)
#define IOCTL_VALSET_NUM _IO(IOC_MAGIC, 3)

#define IOCTL_VAL_MAXNR 3
#define DRIVER_NAME "ioctl_shell"

static unsigned int ioctl_major = 0;
static unsigned int ioctl_minor = 0;
static unsigned int number_of_device = 1;
static struct cdev ioctl_cdev;
static int ioctl_num = 0;

struct ioctl_shell_data {
    unsigned char val;
    rwlock_t lock;
};

static long ioctl_shell_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct ioctl_shell_data *ioctl_data = filp->private_data;
    int retval = 0;
    unsigned char val;
    struct ioctl_arg data;
    memset(&data, 0, sizeof(data));
    
    switch (cmd) {
        case IOCTL_VALSET:
            if (copy_from_user(&data, (int __user *)arg, sizeof(data))) {
                pr_err("iotctl: IOCTL_VALSET failed");
                retval = -EINVAL;
                goto done;
            }

            write_lock(&ioctl_data->lock);
            ioctl_data->val = data.val;
            pr_alert("IOCTL val set: %x\n", data.val);
            write_unlock(&ioctl_data->lock);
            break;
        case IOCTL_VALGET:
            read_lock(&ioctl_data->lock);
            val = ioctl_data->val;
            read_unlock(&ioctl_data->lock);

            data.val = val;
            if (copy_to_user((int __user *)arg, &data, sizeof(data))) {
                pr_err("iotctl: IOCTL_VALGET failed");
                retval = -EINVAL;
                goto done;
            }
            pr_alert("IOCTL val got: %x\n", data.val);
            break;
        case IOCTL_VALGET_NUM:
            retval = __put_user(ioctl_num, (int __user *)arg);
            pr_alert("IOCTL num set: %x\n", ioctl_num);
            break; 
        case IOCTL_VALSET_NUM:
            ioctl_num = arg;
            pr_alert("IOCTL num got: %x\n", ioctl_num);
            break;
        default:
            pr_err("There is no other choice");
            retval = -ENOTTY;
    }
done:
    return retval;
}

static ssize_t ioctl_shell_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
    struct ioctl_shell_data *ioctl_data = filp->private_data;
    unsigned char val;
    int retval = 0;
    int i = 0;

    read_lock(&ioctl_data->lock);
    val = ioctl_data->val;
    read_unlock(&ioctl_data->lock);

    for (; i < length; ++ i) {
        if (copy_to_user(&buffer[i], &val, 1)) {
            retval = -EINVAL;
            goto out;
        }
    }
    retval = length;
out:
    return retval;
}

static int ioctl_shell_open(struct inode *inode, struct file *filp)
{
    pr_alert("%s is called", __func__);

    struct ioctl_shell_data *ioctl_data;
    ioctl_data = kmalloc(sizeof(struct ioctl_shell_data), GFP_KERNEL);

    if (ioctl_data == NULL) {
        return -ENOMEM;
    }

    rwlock_init(&ioctl_data->lock);
    ioctl_data->val = 0xff;
    filp->private_data = ioctl_data;

    return 0;
}

static int ioctl_shell_close(struct inode *inode, struct file *filp)
{
    pr_alert("%s is called", __func__);

    if (filp->private_data) {
        kfree(filp->private_data);
        filp->private_data = NULL;
    }

    return 0;
}

static struct file_operations ioctl_ops = {
    .owner = THIS_MODULE,
    .read = ioctl_shell_read,
    .open = ioctl_shell_open,
    .release = ioctl_shell_close,
    .unlocked_ioctl = ioctl_shell_ioctl,
};

dev_t device;

static int __init ioctl_shell_init(void)
{
    int retval = 0;

    retval = alloc_chrdev_region(&device, 0, number_of_device, DRIVER_NAME);

    if (retval) {
        goto err_out;
    }

    ioctl_major = MAJOR(device);

    cdev_init(&ioctl_cdev, &ioctl_ops);
    retval = cdev_add(&ioctl_cdev, device, number_of_device);
    if (retval) {
        goto err_unregister_chrdev_region;
    }
    pr_alert("ioctl_shell_init: driver with major %d installed\n", ioctl_major);

    return 0;
err_unregister_chrdev_region:
    unregister_chrdev_region(device, number_of_device);
err_out:
    return retval;
}

static void __exit ioctl_shell_exit(void)
{
    cdev_del(&ioctl_cdev);
    unregister_chrdev_region(device, number_of_device);
    pr_alert("ioctl_shell_exit: driver uninstalled\n");
}

module_init(ioctl_shell_init);
module_exit(ioctl_shell_exit);

MODULE_AUTHOR("godminhtri");
MODULE_DESCRIPTION("ioctl shell");
MODULE_LICENSE("GPL");