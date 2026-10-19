#!/bin/bash

# Mount tracefs
sudo mount -t tracefs tracefs /sys/kernel/tracing

T=/sys/kernel/tracing
printf '%s\n' 'p:initmem_free execmem_free ptr=$arg1' | sudo tee -a "$T/kprobe_events"
printf '%s\n' 'r:initmem_free_done execmem_free' | sudo tee -a "$T/kprobe_events"
echo 1 | sudo tee "$T/events/kprobes/initmem_free/enable"
echo 1 | sudo tee "$T/events/kprobes/initmem_free_done/enable"
echo | sudo tee "$T/trace"

# Load module & check free
sudo insmod ./simple_module_release.ko
sleep 2

sudo dmesg | grep 'hello addr'
sudo cat /sys/kernel/tracing/trace | grep initmem_free

# Unload module & check free
echo | sudo tee "$T/trace"
sudo rmmod ./simple_module_release.ko
sleep 2
sudo dmesg | grep 'bye addr'
sudo cat /sys/kernel/tracing/trace | grep initmem_free