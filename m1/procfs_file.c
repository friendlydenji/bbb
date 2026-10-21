#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/version.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
#define HAVE_PROC_OPS
#endif

#define PROCFS_FILE_NAME "iteration"

static void *file_seq_start(struct seq_file *seq, loff_t *offset)
{
	static unsigned long counter = 0;
	
	if (*offset == 0) {
		return &counter;
	}
	
	*offset = 0;
	return NULL;
}

static void *file_seq_next(struct seq_file *seq, void *iter, loff_t *offset)
{
	unsigned long *curr_iter = (unsigned long *)iter;
	(*curr_iter)++;
	(*offset)++;
	return NULL;
}

static void file_seq_stop(struct seq_file *seq, void* iter)
{
	/* do nothing */
}

static int file_seq_show(struct seq_file *seq, void* iter)
{
	loff_t *offset = (loff_t *)iter;

	seq_printf(seq, "%Ld\n", *offset);
	return 0;
}

static struct seq_operations file_seq_ops = {
	.start = file_seq_start,
	.next = file_seq_next,
	.show = file_seq_show,
	.stop = file_seq_stop,
};

static int file_open(struct inode *inode, struct file * file)
{
	return seq_open(file, &file_seq_ops);
}

#ifdef HAVE_PROC_OPS
static const struct proc_ops file_ops = {
	.proc_open = file_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};
#else
static const struct file_operations file_ops = {
	.open = file_open,
        .read = seq_read,
        .llseek = seq_lseek,
        .release = seq_release,
};
#endif

static int __init procfs_file_init(void)
{
	struct proc_dir_entry *entry;

	entry = proc_create(PROCFS_FILE_NAME, 0, NULL, &file_ops);
	if (entry == NULL) {
		pr_err("procfs_file_init: procfs file create failed");
		return -ENOMEM;
	}
	
	pr_info("File created at /proc/%s\n", PROCFS_FILE_NAME);
	return 0;
}

static void __exit procfs_file_exit(void)
{
	remove_proc_entry(PROCFS_FILE_NAME, NULL);
	pr_info("File /proc/%s removed\n", PROCFS_FILE_NAME);
}

module_init(procfs_file_init);
module_exit(procfs_file_exit);

MODULE_AUTHOR("godminhtri");
MODULE_DESCRIPTION("procfs file");
MODULE_LICENSE("GPL");

