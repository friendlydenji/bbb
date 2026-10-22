#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/sysfs.h>

static struct kobject* sysfs_module;

static int sysfs_variable = 0;

static ssize_t sysfs_variable_show(struct kobject* kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sysfs_variable);
}

static ssize_t sysfs_variable_store(struct kobject* kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &sysfs_variable);
	return count;
}

static struct kobj_attribute sysfs_variable_attribute = __ATTR(sysfs_variable, 0660, sysfs_variable_show, sysfs_variable_store);

static int __init sysfs_file_init(void)
{
	int errno = 0;
	
	sysfs_module = kobject_create_and_add("sysfs_module", kernel_kobj);
	if (!sysfs_module) {
		pr_err("sysfs_file_init: kobject_create_and_add failed");
		return -ENOMEM;
	}
	
	errno = sysfs_create_file(sysfs_module, &sysfs_variable_attribute.attr);
	if (errno) {
		kobject_put(sysfs_module);
		pr_err("sysfs_file_init: sysfs_create_file failed");
	}
	
	return errno;
}

static void __exit sysfs_file_exit(void)
{
	kobject_put(sysfs_module);
	pr_info("sysmodule exited");
}

module_init(sysfs_file_init);
module_exit(sysfs_file_exit);

MODULE_AUTHOR("godminhtri");
MODULE_DESCRIPTION("sysfs module shell");
MODULE_LICENSE("GPL");
