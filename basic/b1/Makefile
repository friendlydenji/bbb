obj-m:=normal_driver.o procfs_shell.o sysfs_shell.o debugfs_shell.o ioctl_shell.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

