#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

MODULE_AUTHOR("godminhtri");
MODULE_DESCRIPTION("first module");
MODULE_LICENSE("GPL");

static int __init simple_module_init(void)
{
	printk(KERN_INFO "hello\n");
	return 0;
}

static void __exit simple_module_exit(void)
{
	printk(KERN_INFO "bye\n");
}

module_init(simple_module_init);
module_exit(simple_module_exit);

