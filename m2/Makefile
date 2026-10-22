obj-m:=ioctl_chrdev.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

CC := gcc
CFLAGS := -Wall -Wextra -O2

.PHONY: all kernel user clean


all: kernel user

kernel:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

user: userspace_ioctl

userspace_ioctl: userspace_ioctl.c ioctl.h
	$(CC) $(CFLAGS) -o $@ userspace_ioctl.c

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) userspace_ioctl

