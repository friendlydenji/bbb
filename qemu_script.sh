#!/bin/bash

(
    cd qemu-root
    find . -print0 | cpio --null -o -H newc | gzip -9
) > qemu-initramfs.cpio.gz

sudo qemu-system-x86_64 \
  -m 1024 \
  -kernel /boot/vmlinuz-$(uname -r) \
  -initrd qemu-initramfs.cpio.gz \
  -append "console=ttyS0 rdinit=/init" \
  -nographic \
  -no-reboot
