# Practical Bug Detection for Persistent Memory Systems

João Gonçalves, Miguel Matos, and Rodrigo Rodrigues. 2023.
Mumak: Efficient and Black-Box Bug Detection for Persistent Memory.
In Eighteenth European Conference on Computer Systems (EuroSys ’23),
May 8–12, 2023, Rome, Italy. ACM, New York, NY, USA, 17 pages.
https://doi.org/10.1145/3552326.3587447

João Gonçalves, José Fragoso Santos, Rodrigo Rodrigues, and Miguel Matos. 2026.
Vardalith: Hybrid Detection of Persistent Memory Concurrency Bugs.
To appear in ECOOP'26.

## Installation

We provide containers with all the required dependencies. The [base dockerfile](Dockerfile) offers build arguments to control the PMDK version and base operating system used (helpful when combining with target applications with specific dependencies). Here are some example builds:
```
docker build -t mumak:1.6 . --build-arg PMDK_VERSION=tags/1.6
docker build -t mumak:1.8 . --build-arg PMDK_VERSION=tags/1.8
docker build -t mumak:1.12.1-ubuntu20 . --build-arg PMDK_VERSION=tags/1.12.1 --build-arg BASE_OS=ubuntu:20.04
```
To guarantee the freshest PMDK version when rebuilding the container, either pass `--no-cache` (very time consuming)
or `--build-arg REFRESH="$(date)"` (uses cache for all dependencies, rebuilds from scratch).
```
sudo docker build -t mumak:master --build-arg PMDK_VERSION=master --build-arg BASE_OS=ubuntu:18.04 --build-arg REFRESH="$(date)"
```

When running the container, make sure to mount the PM filesystem, as well as the auxiliary tmpfs:
```
docker run -it -v /mnt/pmem0:/mnt/pmem0 -v /mnt/ramdisk:/mnt/ramdisk mumak:version
```

If you want to build Mumak & Vardalith from scratch, make sure you have rust, cargo, gcc, g++ and make installed. In addition to this:
1. Download and uncompress [PIN](http://software.intel.com/sites/landingpage/pintool/downloads/pin-3.21-98484-ge7cd811fd-gcc-linux.tar.gz).
2. Set `PIN_ROOT` and add it to `PATH`:
```
export PIN_ROOT=/path/to/pin
export PATH=$PATH:$PIN_ROOT
```
3. Compile the tools inside the [src/](src/) folder:
```
cd src
make
```

## Usage Guides

- [Mumak Usage Guide](MUMAK.md)
- [Vardalith Usage Guide](src/vardalith/USAGE.md)


## EuroSys'23 Artifact Evaluation

[Mumak's Artifact Evaluation folder](mumak-artifact-evaluation) contains all the necessary instructions and scripts used to reproduce the results and the figures from the original paper.

[![DOI](https://zenodo.org/badge/595640078.svg)](https://zenodo.org/badge/latestdoi/595640078)

## Code structure

* [src/instrumentation](src/instrumentation) contains various instrumentation tools implemented using PIN and some utilities, used both by Mumak and Vardalith.
* [src/analysis](src/analysis) contains Vardalith's static analysis.
* [src/patching](src/patching) contains Vardalith's patching scripts.
* [src/runtime](src/runtime) contains Vardalith's PIFR monitoring race detection runtime.
* [src/vardalith](src/vardalith) contains Vardalith's cli.
* [docker](docker) contains auxiliary docker configurations for targets and other state-of-the-art bug detection tools
* [mumak](mumak) is a script that acts as a frontend to our tool
* [configs](configs) contains pre-defined configuration files to analyze different targets using the frontend script
* [scripts](scripts) contains some auxiliary scripts

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

Note that these tools uses an additional tmpfs mount to store temporary data. Create it as follows:
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
