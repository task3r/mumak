# Artifact evaluation

## Hardware configuration
In order to reproduce the results reported in the original paper, the machine should be configured with an Intel x86 CPU and a persistent memory module, such as Intel's Optane DCPMM, mounted using a DAX-enabled filesystem.
The machine used to obtain the results presented in the paper is configured as follows:
* 128 core Intel(R) Xeon(R) Gold 6338N CPU @ 2.20GHz
* 256 GB of RAM
* 1 TB Intel Optane DCPMM in App Direct mode

## Software dependencies
The system requirements to evaluate this artifact are:
* Linux (tested with Ubuntu 22.04 LTS, kernel 5.15.0)
* Docker (tested with version 20.10)
* gnuplot (tested with version 5.4)

## Set-up
The evaluation of this artifact depends on the use of a machine equipped
with an Intel x86 processor with support for 
`clwb`, `clflushopt`, `clflush` and `sfence` instructions, and
a physical persistent memory module (e.g., Intel Optane DCPMM) mounted using a DAX-enabled
file system.
To format and mount the drive (assuming the device name is `/dev/pmem0`), follow the instructions below:
```
sudo mkdir /mnt/pmem0
sudo mkfs.ext4 /dev/pmem0
sudo mount -t ext4 -o dax /dev/pmem0 /mnt/pmem0
sudo chmod -R 777 /mnt/pmem0
```

Additionally, Mumak uses an auxiliary tmpfs mount to store temporary data. Create it as follows:
```
sudo mkdir /mnt/ramdisk
sudo mount -t tmpfs -o rw,size=50G tmpfs /mnt/ramdisk
sudo chmod -R 777 /mnt/ramdisk
```

Obtain Mumak's artifact:
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

Although it is not recommended for artifact evaluation purposes since it will impact the final performance measurements, it is possible to simulate PM using a tmpfs as described below:
```
sudo mkdir /mnt/pmem0
sudo mount -t tmpfs -o rw,size=50G tmpfs /mnt/pmem0
sudo chmod -R 777 /mnt/pmem0
```

### Minimal working example (~15 min)
During the kick the tires phase, you can run a minimal working example:
```
cd artifact-evaluation
./minimal_example.sh
```
This will analyze `btree` and `rbtree` using Mumak and PMDK version 1.12.1 and output more verbose logging than the rest of the experiments.
The fault-injection component should detect 118 and 93 failure points (with minor variances, depending on the underlying architecture) for btree and rbtree, respectively , and the trace analysis component should report over 6000 flushes and fences. Otherwise, your PM configuration might not be correct. 


## Experiments execution
This directory contains scripts to create the docker images, run the containers and carry out each experiment with the appropriate configurations.
To reproduce the experiments, make sure you are in the correct path:
```
cd artifact-evaluation
```
Before running the experiments, make sure to build the required images:
```
./build_images.sh
```

### Workload coverage benchmarks(~1 h)
To obtain the coverage of varying size workloads for targets `btree`, `rbtree` and `hashmap_atomic` for PMDK 1.6, run:
``` 
./run_coverage.sh
```

### PMDK 1.6 performance benchmarks(~100 h)
To obtain the performance comparison when analyzing `btree`, `rbtree` and `hashmap_atomic` for PMDK 1.6, run:
``` 
./run_all_pmdk1dot6.sh
```
Alternatively, you can run each tool separately:
```
./run_mumak_pmdk1dot6.sh # ~6 h
./run_agamotto.sh # ~60 h
./run_xfdetector.sh # ~36 h
```

### PMDK 1.8 performance benchmarks(~40 h)
To obtain the performance comparison when analyzing `btree` and `rbtree` for PMDK 1.8, run:
``` 
./run_all_pmdk1dot8.sh
```
Alternatively, you can run each tool separately:
```
./run_mumak_pmdk1dot8.sh # ~4 h
./run_pmdebugger.sh # ~12 h
./run_witcher.sh # ~24 h
```

Each experiment will run three times, as stated in the paper. In the cases where these experiments are expected to reach the defined 12h timeout (XFDetector and Witcher), we changed the number of iterations to 1 to reduce the time needed to evaluate the artifact. This behavior can be changed in the respective scripts, as commented.

### Scalability (~12 h)
To analyze pmemkv, Montage, Redis and RocksDB run:
``` 
./run_all_scalability.sh
```
Alternatively, you can analyze each system separately:
```
./run_pmemkv.sh # ~6 h
./run_montage.sh # ~4 h
./run_redis.sh # ~45 min
./run_rocksdb.sh # ~90 min
```

## Generate plots
After running the experiments, generate the plots and resource usage table by running:
```
./plot_coverage.sh # Figures 3a and 3b
./plot_pmdk1dot6.sh # Figure 4a
./plot_pmdk1dot8.sh # Figure 4b
./plot_scalability.sh # Figure 5
```
The figures will be available as eps files in the [plots](plots) directory.


## Code structure
For more information about the source code structure please see [the main readme](../README.md).
