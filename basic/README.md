# Basic LKMPG
- These are some first Linux kernel module I reference from [LKMPG](https://sysprog21.github.io/lkmpg/#control-the-leds-onoff-state).
- Now, there are several module that divide into 4 parts:
  - In [b0](basic/b0/README.md), we'll get acquainted with kernel module with a simple module just only say hello and bye when load and unload it. However, it will be so easy if we only do that, so we have a debug form of the module that help us get the memory space that the module actually take and then auto free by the kernel with the __init and __exit. 
  - In [b1](basic/b1/README.md), we make the kernel module more complete by make it become a pseudo character device driver, get acquainted with some modules in others location including procfs, sysfs, debugfs and finally we can make "out-of-band communication" with device driver through ioctl.
  - In [b2](basic/b2/README.md), we'll complete the ioctl module with an userspace application to test it's functions.
  - In [b3](basic/b3/README.md), we put a hooking in the syscall __open_at.