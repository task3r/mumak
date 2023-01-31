# Mumak: efficient and black-box bug detection for Persistent Memory

Gonçalves, João and Matos, Miguel, and Rodrigues, Rodrigo, "Mumak: efficient and black-box bug detection for Persistent Memory", in Proceedings of the EuroSys Conference, May 8-12, 2023, Rome, Italy.

## Artifact evaluation

The [Artifact Evaluation folder](artifact-evaluation) contains all the necessary instructions and scripts used to reproduce the results and the figures from the original paper.


## Installation

Clone the repository:
```
git clone https://github.com/task3r/mumak.git
cd mumak && git submodule update --init
```
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

We provide containers with all the required dependencies. The [Mumak base dockerfile](Dockerfile) offers build arguments to control the PMDK version and base operating system used (helpful when combining with target applications with specific dependencies). Here are some example builds:
```
docker build -t mumak:1.6 . --build-arg PMDK_VERSION=tags/1.6
docker build -t mumak:1.8 . --build-arg PMDK_VERSION=tags/1.8
docker build -t mumak:1.12.1-ubuntu20 . --build-arg PMDK_VERSION=tags/1.12.1 --build-arg BASE_OS=ubuntu:20.04
```
To guarantee the freshest PMDK version when rebuilding Mumak, either pass `--no-cache` (very time consuming)
or `--build-arg REFRESH="$(date)"` (uses cache for all dependencies, rebuilds PMDK and Mumak from scratch).
```
sudo docker build -t mumak:master --build-arg PMDK_VERSION=master --build-arg BASE_OS=ubuntu:18.04 --build-arg REFRESH="$(date)"
```

When running the Mumak container, make sure to mount the PM filesystem, as well as the auxiliary tmpfs:
```
docker run -it -v /mnt/pmem0:/mnt/pmem0 -v /mnt/ramdisk:/mnt/ramdisk mumak:version
```

If you want to build Mumak from scratch, make sure you have gcc, g++ and make installed. In addition to this:
1. Download and uncompress [PIN](http://software.intel.com/sites/landingpage/pintool/downloads/pin-3.21-98484-ge7cd811fd-gcc-linux.tar.gz).
2. Set `PIN_ROOT` and add it to `PATH`:
```
export PIN_ROOT=/path/to/pin
export PATH=$PATH:$PIN_ROOT
```
3. Compile the instrumentation tools inside the [instrumentation/src](instrumentation/src/) folder:
```
cd instrumentation/src
make
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
## Code structure

* [instrumentation/src](instrumentation/src) contains the core of Mumak, with a file for each PIN tool implemented and some utilities.
* [docker](docker) contains auxiliary docker configurations for targets and other state-of-the-art bug detection tools
* [mumak](mumak) is a script that acts as a frontend to our tool
* [configs](configs) contains pre-defined configuration files to analyze different targets using the frontend script
* [scripts](scripts) contains some auxiliary scripts
* [artifact-evaluation](artifact-evaluation) contains instructions to reproduce the results presented in the original paper

## Additional help
### How to configure Intel Optane DCPMM

1. Ensure Persistent Memory is working in App Direct Mode (this requires a reboot afterwards, more information [here](https://docs.pmem.io/ipmctl-user-guide/provisioning/create-memory-allocation-goal))
```
sudo ipmctl create -goal PersistentMemoryType=AppDirect
```
If the latter fails with an error such as:
> Create region configuration goal failed: Error 124 - Namespaces exist on specified DIMMs. They have to be removed before running this command.

Try this:
```
sudo ndctl destroy-namespace -f all
```
2. Create a namespace in fsdax mode (more information [here](https://docs.pmem.io/ndctl-user-guide/managing-namespaces)) using the following command which defaults to the fsdax mode and region 0:
```
sudo ndctl create-namespace -r 0
```
3. Confirm the created namespace with the following command:
```
sudo ndctl list
```
4. Mount Ext4-DAX using the previous created namespace:
```
sudo mkdir /mnt/pmem0
sudo mkfs.ext4 -b 4096 -E stride=512 -F /dev/pmem0
sudo mount -o dax /dev/pmem0 /mnt/pmem0
sudo chmod -R 777 /mnt/pmem0
```

Note that Mumak uses an additional tmpfs mount to store temporary data. Create it as follows:
```
sudo mkdir /mnt/ramdisk
sudo mount -t tmpfs -o rw,size=50G tmpfs /mnt/ramdisk
sudo chmod -R 777 /mnt/ramdisk
```

The results reported in the original paper were obtained using the configuration described above.
Although it is not recommended for artifact evaluation purposes, you can simulate PM using a tmpfs as described below:

```
sudo mkdir /mnt/pmem0
sudo mount -t tmpfs -o rw,size=50G tmpfs /mnt/pmem0
sudo chmod -R 777 /mnt/pmem0
```
For applications that use PMDK, ensure that `PMEM_IS_PMEM_FORCE` is `1`:
```
export PMEM_IS_PMEM_FORCE=1
```
