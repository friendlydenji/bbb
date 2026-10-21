#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/sysfs.h>

static struct kobject* sysmodule;

static int sysvar = 0;

static ssize_t sysvar_show(struct kobject* kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", sysvar);
}

static ssize_t sysvar_store(struct kobject* kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &sysvar);
	return count;
}

static struct kobj_attribute sysvar_attribute = __ATTR(sysvar, 0660, sysvar_show, sysvar_store);

static int __init sysfs_file_init(void)
{
	int errno = 0;
	
	sysmodule = kobject_create_and_add("sysmodule", kernel_kobj);
	if (!sysmodule) {
		pr_err("sysfs_file_init: kobject_create_and_add failed");
		return -ENOMEM;
	}
	
	errno = sysfs_create_file(sysmodule, &sysvar_attribute.attr);
	if (errno) {
		kobject_put(sysmodule);
		pr_err("sysfs_file_init: sysfs_create_file failed");
	}
	
	return errno;
}

static void __exit sysfs_file_exit(void)
{
	kobject_put(sysmodule);
	pr_info("sysmodule exited");
}

module_init(sysfs_file_init);
module_exit(sysfs_file_exit);

MODULE_AUTHOR("godminhtri");
MODULE_DESCRIPTION("sysfs module");
MODULE_LICENSE("GPL");
