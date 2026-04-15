# Mumak Usage Guide

Make sure to disable address space randomization, as the analysis depends on it:
```
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
```
Additionally, you should also configure the coredump naming convention to obtain better debugging information:
```
echo '/tmp/core-%e.%p.%h.%t' | sudo tee /proc/sys/kernel/core_pattern
```
These last two steps can be automated by running:
```
./scripts/setup_host.sh
```

To reproduce the results from the paper, make sure you pull the other dependencies as well:
```
git submodule update --init
```

## Run Mumak

The [mumak](mumak) script acts as a frontend for our tool. Pass `-h` to get instructions on how to run it:
```
Usage: mumak -x target [-X target_recovery] [-c client] [-C client_recovery]
       [-k target_termination] [-t vanilla,bfi,onta,dofta,ndofta,coverage]
       [-i all,clflush,clflushopt,clwb,movnt,sfence,store] [-o dir]
       [-m pm_mount] [-d ramdisk] [-F fp] [-a size] [-f file] [-v]
Efficient and blackbox detection of crash-consistency bugs in
Persistent Memory programs.

  -x cmd       Target execution.
  -X cmd       Recovery execution. Only required for using fault-injection tools.
  -c cmd       Client invocation (used for client-server applications).
  -C cmd       Client recovery invocation (used for client-server applications).
  -k cmd       Explicit target termination.
  -t t1,t2,... Mumak tools to use during analysis separated by ','.
               Defaults to all.
  -i inst      Instruction to fail (all, clwb, clflush, clflushopt, sfence, movnt)
               Defaults to all.
  -o dir       Output directory for the results (created if it does not exist).
               Defaults to local folder (.).
  -m path      PM mount path, used only to collect PM usage.
               Defaults to /mnt/pmem0.
  -d path      RAM disk mount path, used for trace analysis tools.
               Defaults to /mnt/ramdisk.
  -s period    Sleep period (in seconds) between server and client invocations.
               Defaults to 1.
  -T period    Recovery timeout period (in seconds).
               Defaults to 5.
  -I period    Max fault injection period (in seconds).
               Defaults to 60.
  -F fp        Index of specific failure point to target.
  -a size      Allocation for failure point tree (in bytes).
               Defaults to 32768.
  -f file      Set options through a configuration file.
  -v           Increased verbosity.
               Defaults to false.
  -h           Display this help message.

Example:
mumak -o output -v -t bfi,dofta -x "./target args" -X "./target recovery"
```
