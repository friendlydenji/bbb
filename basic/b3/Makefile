
ifneq ($(KERNELRELEASE),)

# Kernel build
obj-m := syscall_hooking.o

else

# Userspace build
KDIR ?= /lib/modules/$(shell uname -r)/build

USER_CC := gcc
USER_CFLAGS := -Wall -Wextra -O2

.PHONY: all kernel user clean

all: kernel user

kernel:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

user: syscall_test

userspace_ioctl: syscall_test.c
	$(USER_CC) $(USER_CFLAGS) -o $@ $<

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
	$(RM) syscall_test

endif
