#!/bin/bash

export PIN_ROOT=$HOME/pin
export PATH=$PATH:$PIN_ROOT
export PMEM_IS_PMEM_FORCE=1
export LD_PRELOAD=~/pmdk/src/debug/libpmem.so.1:~/pmdk/src/debug/libpmemobj.so.1
export MUMAK_ROOT=/vagrant/
export MUMAK_TOOLS=/vagrant/instrumentation/src/obj-intel64/

