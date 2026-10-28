# Learn comprehensive linux with Beaglebone Black
This is the nth time I've restarted the Beaglebone Black embedded Linux study and this time maybe I'll make it a comprehensive course to learn Linux.

## Prerequisites
- Before diving into this shjt, we could download all stuffs first to avoid wasting time to wait them in the future.
- I divide them into 2 groups of device: BBB and QEMU on x86 host
- And 3 groups of hardware (BBB) images: BeagleBoard images, build system images and scratch linux system.
- Every group has its own advantages and goals to achieve.

### QEMU
- Fast deploy and convenient for Linux kernel module and Linux system programming

### Image provided by BeagleBoard 
- Fast deploy and convenient for Linux kernel module and Linux system programming 
- Available from https://www.beagleboard.org/distros

### Build system images
- Including Yocto and Buildroot. 
- Buildroot help us reduce build time and easier when compare to Yocto, but Yocto give us better performance.

### Embedded Linux from "scratch"
- Not the Linux From Scrath (If I have a lot of time, maybe I'll try).
- Build components of a embedded Linux system (toolchains, bootloader, kernel, rootfs) separate then gather them to boot the board.
- This method help us understand more about the boot flow and has high customization.

## My plan
- Currently, we will focus on 3 aspects of Linux programming including: Linux kernel module, Linux system programming and device driver.
- For Linux kernel module and Linux system programming, we can both test them on Beaglebone Black or QEMU.
- For device driver, we will test on the real hardware bbb for sure.
  
### Basic LKMPG
- [x] b0: simple driver 
- [x] b1: a normal driver and some "shell" module that currently only for know how to use (including procfs, sysfs, debugfs, ioctl) 
- [x] b2: pseudo ioctl character device driver and its user space application 
- [ ] b3: syscall probe/ kernel module 

### LKMPG & Device driver
- [ ] d0: GPIO
- [ ] d1: sensor
- [ ] d2: USB driver
- [ ] d3: PCI Ethernet driver
- [ ] d4: USB Ethernet driver

### LKMPG & System programming
- [ ] s1: Multiplexed I/O 
- [ ] s2: Scatter/gather I/O 
- [ ] s3: Advice 
- [ ] s4: Asynchronous I/O 
- [ ] s5: Optimize for block device I/O 
- [ ] s6: Daemon 
- [ ] s7: Scheduling 

