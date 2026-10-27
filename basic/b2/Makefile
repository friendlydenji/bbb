
ifneq ($(KERNELRELEASE),)

# Kernel build
obj-m := ioctl_chrdev.o

else

# Userspace build
KDIR ?= /lib/modules/$(shell uname -r)/build

USER_CC := gcc
USER_CFLAGS := -Wall -Wextra -O2

.PHONY: all kernel user clean

all: kernel user

kernel:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

user: userspace_ioctl

userspace_ioctl: userspace_ioctl.c ioctl.h
	$(USER_CC) $(USER_CFLAGS) -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	$(RM) userspace_ioctl

endif
