#!/bin/bash

printf 'Disable ASLR: '
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
printf 'Set coredump location: '
echo '/tmp/core-%e.%p.%h.%t' | sudo tee /proc/sys/kernel/core_pattern
