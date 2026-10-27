#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/unistd.h>
#include <linux/cred.h>
#include <linux/uidgid.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 7, 0))

#if defined(CONFIG_KPROBES)
#include <linux/kprobes.h>
#define HAVE_KPROBES 1

#if defined(CONFIG_X86_64)
#define USE_KPROBES_PRE_HANDLER_BEFORE_SYSCALL 1
#endif /* CONFIG_X86_64 */

#else /* NOT CONFIG_KPROBES */
#include <linux/kallsyms.h>
#define HAVE_PARAM 1
static unsigned long sym = 0;
module_param(sym, ulong, 0644);
#endif /* CONFIG_KPROBES */

#else /* LINUX_VERSION_CODE < 5.7.0*/

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
#include <linux/syscall.h>
#define HAVE_KSYS_CLOSE 1
#else /* LINUX_VERSION_CODE  > 5.4.0 */
#include <linux/kallsyms.h>
#endif

#endif

static uid_t uid = -1;
module_param(uid, int, 0644);

#if USE_KPROBES_PRE_HANDLER_BEFORE_SYSCALL

static char *syscall_sym = "__x86_sys_openat";
module_param(syscall_sym, charp, 0644);

static int syscall_kprobe_pre_handler(struct kprobe *probe, struct pt_regs *regs)
{
    if (__kuid_val(current_uid()) != uid) {
        return 0;
    }

    pr_info("%s called by uid %d\n", syscall_sym, uid);
    return 0;
}

static struct kprobe syscall_kprobe = {
    .symbol_name = "__x86_sys_openat",
    .pre_handler = syscall_kprobe_pre_handler
};


#else /* NOT USE_KPROBES_PRE_HANDLER_BEFORE_SYSCALL */

static unsigned long **syscall_table;

#if defined(CONFIG_ARCH_HAS_SYSCALL_WRAPPER)
static asmlinkage long (*original_call)(const struct pt_regs *);
#else /* NOT CONFIG_ARCH_HAS_SYSCALL_WRAPPER */
static asmlinkage long (*original_call)(int, const char __user*, int, umode_t);
#endif /* CONFIG_ARCH_HAS_SYSCALL_WRAPPER */
                                              
#ifdef CONFIG_ARCH_HAS_SYSCALL_WRAPPER
static asmlinkage long openat_hooking(const struct pt_regs *regs)
#else
static asmlinkage long openat_hooking(int fd, const char __user *filename,
                                      int flags, umode_t mode)
#endif
{
    int i = 0;
    char ch;

    if (__kuid_val(current_uid()) != uid) {
        goto orig_call;
    }

    pr_info("file opened by uid: %d, file: ", uid);
    do {
#if defined(CONFIG_ARCH_HAS_SYSCALL_WRAPPER)
            get_user(ch, (char __user *)regs->si + i);
#else
            get_user(ch, (char __user *)filename + i);
#endif
        pr_info("%c", ch);
        ++ i;
    } while (ch != 0);
    pr_info("\n");
orig_call:
#if defined(CONFIG_ARCH_HAS_SYSCALL_WRAPPER)
    return original_call(regs);
#else /* NOT CONFIG_ARCH_HAS_SYSCALL_WRAPPER */
    return original_call(fd, filename, flags, mode);
#endif /* CONFIG_ARCH_HAS_SYSCALL_WRAPPER */
}

static unsigned long **acquire_syscall_table(void)
{
#if defined(HAVE_KSYS_CLOSE)
    unsigned long int offset = PAGE_OFFSET;
    unsigned long **syscall_table;

    while (offset < ULONG_MAX) {
        syscall_table = (unsigned long **)offset;
        
        if (syscall_table[__NR_close] == (unsigned long *)ksys_close) {
            return syscall_table;
        }

        offser += sizeof(void *);
    }
    return NULL;
#endif /* HAVE_KSYS_CLOSE */

#if defined(HAVE_PARAM) 
    const char syscall_table_name[15] = "syscall_table";
    char symbol[40] = {0};

    if (sym == 0) {
        pr_err("For Linux v5.7+, Kprobes is the preferable way to get symbol.\n");
        pr_info("If Kprobes is absent, you have to specify the address of sys_call_table symbol"
                "\n\tby /boot/System.map or /proc/kallsyms, "
                "which contains all the symbol addresses, into sym parameter.\n");
        return NULL;
    }

    sprint_symbol(symbol, sym);
    if (!strncmp(syscall_table_name, symbol, sizeof(syscall_table_name) - 1)) {
        return ((unsigned long *)sym);
    }

    return NULL;
#endif /* HAVE_PARAM */

#if defined(HAVE_KPROBES) 
    unsigned long (*kallsyms_lookup_name)(const char *name);
    struct kprobe probe = {
        .symbol_name = "kallsyms_lookup_name",
    };

    if (register_kprobe(&probe) < 0) {
        return NULL;
    }
    kallsyms_lookup_name = (unsigned long (*)(const char *name))probe.addr;
    unregister_kprobe(&probe);
#endif /* HAVE_KPROBES */

    return (unsigned long **)kallsyms_lookup_name("syscall_table");
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 3, 0)

static inline void __write_cr0(unsigned long cr0)
{
    asm volatile("mov %0, %%cr0" : "+r"(cr0) : : "memory");
}

#else /* LINUX_VERSION_CODE < 5.3.0 */
#define __write_cr0 write_cr0
#endif /* LINUX_VERSION_CODE */

static void enable_write_protection(void) {
    unsigned long cr0 = read_cr0();
    set_bit(16, &cr0);
    __write_cr0(cr0);
}


static void disable_write_protection(void) {
    unsigned long cr0 = read_cr0();
    clear_bit(16, &cr0);
    __write_cr0(cr0);
}
#endif /* USE_KPROBES_PRE_HANDLER_BEFORE_SYSCALL */

static int __init syscall_hooking_init(void)
{
#if USE_KPROBES_PRE_HANDLER_BEFORE_SYSCALL
    int err;

    syscall_kprobe.symbol_name = syscall_sym;
    err = register_kprobe(&syscall_kprobe);

    if (err) {
        pr_err("syscall_hooking_init: register_kprobe failed %d\n", err);
        return err;
    }
#else /* NOT USE_KPROBES_PRE_HANDLER_BEFORE_SYSCALL */
    if (!(syscall_table = acquire_syscall_table())) {
        return -1;
    }

    disable_write_protection();
    original_call = (void *)syscall_table[__NR_openat];
    syscall_table[__NR_openat] = (unsigned long *)openat_hooking;
    enable_write_protection();
#endif /* USE_KPROBES_PRE_HANDLER_BEFORE_SYSCALL */
    pr_info("hooking on uid: %d\n", uid);
    return 0;
}

static void __exit syscall_hooking_exit(void)
{
#if USE_KPROBES_PRE_HANDLER_BEFORE_SYSCALL
    unregister_kprobe(&syscall_kprobe);
#else /* NOT USE_KPROBES_PRE_HANDLER_BEFORE_SYSCALL */
    if (!syscall_table) {
        return;
    }

    if (syscall_table[__NR_openat] != (unsigned long *)openat_hooking) {
        pr_alert("the syscall open is modified, unstable !!!\n");
    }

    disable_write_protection();
    syscall_table[__NR_openat] =  (unsigned long *)original_call;
    enable_write_protection();
#endif /* USE_KPROBES_PRE_HANDLER_BEFORE_SYSCALL */
    pr_info("exited\n");
    msleep(200);
}

module_init(syscall_hooking_init);
module_exit(syscall_hooking_exit);

MODULE_AUTHOR("godminhtri");
MODULE_DESCRIPTION("put a hooking on the syscall open");
MODULE_LICENSE("GPL");