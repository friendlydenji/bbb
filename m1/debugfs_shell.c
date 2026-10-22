#include <linux/debugfs.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/printk.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>

#define DEBUGFS_DIR "debugfs_shell"
#define MSG_MAX_LEN 128

static struct dentry *debugfs_dir;
static DEFINE_MUTEX(msg_lock);
static char msg[MSG_MAX_LEN] = "hello\n";
static int msg_len = sizeof("hello\n");

static const char blob_data[] = "This blob is read-only\n";
static struct debugfs_blob_wrapper debugfs_blob = {
    .data = (void *)blob_data,
    .size = sizeof(blob_data) - 1,
};

static u32 debug_value = 42;
static bool debug_enabled = true;

module_param(debug_value, uint, 0600);
MODULE_PARM_DESC(debug_value, "Value export through debugfs");
module_param(debug_enabled, bool, 0600);
MODULE_PARM_DESC(debug_enabled, "Debug enable");

static ssize_t msg_read(struct file *filp, char __user *buffer, size_t len, loff_t *offset)
{
    ssize_t ret;

    mutex_lock(&msg_lock);
    ret = simple_read_from_buffer(buffer, len, offset, msg, msg_len);
    mutex_unlock(&msg_lock);

    return ret;
}

static ssize_t msg_write(struct file *filp, const char __user *buffer, size_t len, loff_t *offset)
{
    ssize_t copied;

    if (*offset != 0) {
        return -EINVAL;
    }

    mutex_lock(&msg_lock);
    copied = simple_write_to_buffer(msg, sizeof(msg) - 1, offset, buffer, len);
    if (copied > 0) {
        msg_len = *offset;
        msg[msg_len] = '\0';
    }
    mutex_unlock(&msg_lock);
    return copied;
}

static const struct file_operations debugfs_msg_ops = {
    .owner = THIS_MODULE,
    .read = msg_read,
    .write = msg_write,
    .llseek = default_llseek,
};

static int __init debugfs_shell_init(void)
{
    debugfs_dir = debugfs_create_dir(DEBUGFS_DIR, NULL);
    
    if (IS_ERR(debugfs_dir)) {
        pr_err("debugfs_shell_init: debugfs_create_dir failed: %ld\n", PTR_ERR(debugfs_dir));
        return PTR_ERR(debugfs_dir);
    }

    debugfs_create_u32("debug_value", 0600, debugfs_dir, &debug_value);
    debugfs_create_bool("debug_enabled", 0600, debugfs_dir, &debug_enabled);
    debugfs_create_file("message", 0600, debugfs_dir, NULL, &debugfs_msg_ops);
    debugfs_create_blob("blob", 0400, debugfs_dir, &debugfs_blob);

    pr_info("debugfs_shell_init: debugfs files created at /sys/kernel/debug/%s\n", DEBUGFS_DIR);
    return 0;
}

static void __exit debugfs_shell_exit(void)
{
    debugfs_remove_recursive(debugfs_dir);
    pr_info("debugfs_shell_exit: debugfs removed\n");
}

module_init(debugfs_shell_init);
module_exit(debugfs_shell_exit);

MODULE_AUTHOR("godminhtri");
MODULE_DESCRIPTION("debugfs shell");
MODULE_LICENSE("GPL");