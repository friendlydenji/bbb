#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/uaccess.h>
#include <linux/moduleparam.h>

#include <asm/errno.h>

#define DEVICE_NAME_PREFIX      "user_leds_"
#define DEVICE_NUM              4
#define BUF_LEN                 2
#define BUILTIN_LED_GPIO        533                 /* magic number */
#define LED_FLAGS               GPIOF_OUT_INIT_LOW
#define LED_LABEL_PREFIX        "user led "

static int base_gpio = BUILTIN_LED_GPIO;
module_param(base_gpio, uint, 0644);
static char control_signal[BUF_LEN];
static unsigned long device_buffer_size = 0;

struct LED_dev {
    dev_t devno;
    int major; 
    int minor;
    struct cdev cdev;
    struct class *class;
    struct device *device[DEVICE_NUM];
};

static struct LED_dev led_device;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
static unsigned int led_gpio[DEVICE_NUM];
static unsigned int led_flags[DEVICE_NUM];
static const char led_label[DEVICE_NUM][12];
#else
static struct gpio leds[DEVICE_NUM];
#endif /* LINUX_VERSION_CODE */

static inline unsigned int led_gpio_num(int index)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
    return led_gpio[index];
#else
    return leds[index].gpio;
#endif
}

static int device_open(struct inode *inode, struct file *filp) 
{
    int led_index = iminor(inode) - led_device.minor;   

    if (led_index < 0 || led_index >= DEVICE_NUM)
        return -ENODEV;

    filp->private_data = (void *)(uintptr_t)led_index;  
    return 0;
}

static int device_release(struct inode *inode, struct file *filp) 
{
    return 0;
}

static ssize_t device_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
    device_buffer_size = min(BUF_LEN, length);

    if (*offset < 0) {
        return -EINVAL;
    }

    if (*offset > device_buffer_size) {
        *offset = 0;
    }

    if (copy_to_user(buffer, control_signal, device_buffer_size)) {
        return -EINVAL;
    }

    *offset += device_buffer_size;

    return device_buffer_size;

}

static ssize_t device_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset)
{
    int led_index = (int)(uintptr_t)filp->private_data;

    device_buffer_size = min(BUF_LEN, length);

    if (*offset < 0) {
        return -EINVAL;
    }

    if (*offset > device_buffer_size) {
        *offset = 0;
    }

    if (copy_from_user(control_signal, buffer, device_buffer_size)) {
        return -EINVAL;
    }

    switch (control_signal[0]) {
        case '0':
            gpio_set_value(led_gpio_num(led_index), 0);
            pr_info("LED %d on\n", led_index);
            break;
        case '1':
            gpio_set_value(led_gpio_num(led_index), 1);
            pr_info("LED %d off\n", led_index);
            break;
        default:
            pr_warn("Invalid signal\n");
    }

    *offset += device_buffer_size;

    return device_buffer_size;
}

static struct file_operations fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 10, 0)
    .owner = THIS_MODULE,
#endif
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release
};

static int __init device_init(void)
{
    int retval = 0;

    if (led_device.major) {
        led_device.devno = MKDEV(led_device.major, led_device.minor);
        retval = register_chrdev_region(led_device.devno, DEVICE_NUM, DEVICE_NAME_PREFIX);
        if (retval) {
            pr_err("device_init: register_chrdev_region failed %d\n", retval);
            goto err_out;
        }
    }
    else {
        retval = alloc_chrdev_region(&led_device.devno, led_device.minor, DEVICE_NUM, DEVICE_NAME_PREFIX);
        if (retval) {
            pr_err("device_init: alloc_chrdev_region failed %d\n", retval);
            goto err_out;
        }
        led_device.major = MAJOR(led_device.devno);
    }
    pr_info("device_init: device registered with major %d, minor %d\n", led_device.major, led_device.minor);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
    led_device.cdev.owner = THIS_MODULE;
#endif

    cdev_init(&led_device.cdev, &fops);
    retval = cdev_add(&led_device.cdev, led_device.devno, DEVICE_NUM);
    if (retval) {
        pr_err("device_init: cdev_add failed %d\n", retval);
        goto err_unregister_chardev_region;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    led_device.class = class_create(DEVICE_NAME_PREFIX);
#else
    led_device.class = class_create(THIS_MODULE, DEVICE_NAME_PREFIX);
#endif
    if (IS_ERR(led_device.class)) {
        retval = PTR_ERR(led_device.class);
        pr_err("device_init: class_create failed %d\n", retval);
        goto err_cdev_del;
    }

    int i = 0;
    for (i = 0; i < DEVICE_NUM; ++ i) {
        char device_name[12];
        char label_name[12];
        sprintf(device_name, "%s%c", DEVICE_NAME_PREFIX, (char)i + '0');
        sprintf(label_name, "%s%c", LED_LABEL_PREFIX, (char)i + '0');
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
        led_gpio[i] = base_gpio + i;
        led_flags[i] = LED_FLAGS;
        strcpy(led_label[i], label_name);
#else
        leds[i] = (struct gpio){base_gpio + i, LED_FLAGS, device_name};
#endif
        led_device.device[i] = device_create(led_device.class, NULL, MKDEV(led_device.major, led_device.minor + i), NULL, device_name);
        if (IS_ERR(led_device.device[i])) {
            retval = PTR_ERR(led_device.device[i]);
            pr_err("device_init: device_create %d failed %d\n", i, retval);
            goto err_device_destroy;
        }
    }

    pr_info("device_init: device created at /dev/%s\n", DEVICE_NAME_PREFIX);

    for (i = 0; i < DEVICE_NUM; ++ i) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
        retval = gpio_request(led_gpio[i], led_label[i]);
#else
        retval = gpio_request(leds[i].gpio, leds[i].label);
#endif
        if (retval) {
            pr_err("device_init: gpio_request failed %d\n", retval);
            goto err_device_destroy;
        }
    }


    for (i = 0; i < DEVICE_NUM; ++ i) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 10, 0)
        retval = gpio_direction_output(led_gpio[i], led_flags[i]);
#else
        retval = gpio_direction_output(leds[i].gpio, leds[i].flags);
#endif
        if (retval) {
            pr_err("device_init: gpio_direction_output failed %d\n", retval);
            goto err_gpio_free;
        }
    }

    return 0;

err_gpio_free:
    for (i = 0; i < DEVICE_NUM; ++ i) {
        gpio_free(led_gpio_num(i));
    }
    i = DEVICE_NUM;
err_device_destroy:
    for (-- i; i >=0 ; -- i) {
        device_destroy(led_device.class, led_device.devno + i);
    }
// err_class_destroy:
    class_destroy(led_device.class);
err_cdev_del:
    cdev_del(&led_device.cdev);
err_unregister_chardev_region:
    unregister_chrdev_region(led_device.devno, DEVICE_NUM);
err_out:
    return retval;
}

static void __exit device_exit(void)
{
    int i;
    for (i = 0; i < DEVICE_NUM; ++ i) {
        gpio_set_value(led_gpio_num(i), 0);
        gpio_free(led_gpio_num(i));
        device_destroy(led_device.class, led_device.devno + i);
    }
    class_destroy(led_device.class);
    cdev_del(&led_device.cdev);
    unregister_chrdev_region(led_device.devno, DEVICE_NUM);

    pr_info("device_exit: device removed\n");
}

module_init(device_init);
module_exit(device_exit);

MODULE_AUTHOR("godminhtri");
MODULE_DESCRIPTION("gpio device driver");
MODULE_LICENSE("GPL");
