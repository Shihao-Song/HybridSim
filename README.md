## A Cycle-Level, Multi-Core CPU Trace Driven PCM-Based System Simulator
This simulator has been used in our CASES 2019 submission:
+ Enabling and Exploiting Partition-Level Parallelism (PALP) in Phase Change Memories, **CASES 2019**
This repository is for development only. Please use the [stable release](https://github.com/drexel-DISCO/PALP) for your own research.

### Dependencies:
+ C++17 and above
+ [Google Protocol Buffers](https://github.com/protocolbuffers/protobuf)
+ C++ Boost Library

### Usage:
1. **Modular System Components**: The following code snapshot shows an example of how to create a PCM and a shared last-level L2 cache.

```c++
std::unique_ptr<MemObject> PCM(createMemObject(cfg, Memories::PCM));

std::unique_ptr<MemObject> L2(createMemObject(cfg, Memories::L2_CACHE, isLLC));
L2->setNextLevel(PCM.get());

```
2. **Highly Configurable**: The following code snapshot shows an example of how to create a custom configuration file for a shared last-level 4MB 8-way L2 cache and a 32GB PCM.
```python
L2_assoc = 8
L2_size = 4096
L2_write_only = false
L2_num_mshrs = 32
L2_num_wb_entries = 32
L2_tag_lookup_latency = 12

...
num_of_word_lines_per_tile = 512
num_of_bit_lines_per_tile = 2048
num_of_tiles = 128
num_of_parts = 64
num_of_banks = 8
num_of_ranks = 4
num_of_channels = 1
```
3. **Example (1)**: The following code snapshot shows an example of how to simulate a 2-core PCM-based system with an FCFS PCM memory controller.
```console
$./PCMSim --config configs/sample-FCFS-32GB-PCM.cfg --cpu_trace 503.bwaves_r.cpu_trace --cpu_trace 503.bwaves_r.cpu_trace --stat_output stats
```
4. **Example (2)**: The following code snapshot shows an example of how to simulate a 2-core PCM-based system with a PLP PCM memory controller.
```console
$./PCMSim --config configs/sample-PLP-32GB-PCM.cfg --cpu_trace 503.bwaves_r.cpu_trace --cpu_trace 503.bwaves_r.cpu_trace --stat_output stats
```
