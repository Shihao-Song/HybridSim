#!/usr/bin/env python

import sys
from os import listdir, mkdir
from os.path import exists, join
import subprocess

cpu_traces_dir=sys.argv[1]

workloads=[d for d in listdir(cpu_traces_dir)]

for workload in workloads:
    workload_dir=join(cpu_traces_dir, workload)
    traces=[join(workload_dir, f) for f in listdir(workload_dir)]
    
    args = ["./Sim", workload, "--config", "configs/sample-FRFCFS-64MB-eDRAM-32GB-PCM.cfg", "--traces"]
    args.extend(traces)
    print args
    # subprocess.call(args)
