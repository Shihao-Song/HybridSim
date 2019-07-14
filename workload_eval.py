#!/usr/bin/env python

import sys
from os import listdir, mkdir
from os.path import exists, join
import subprocess

cpu_traces_dir=sys.argv[1]

workloads=[d for d in listdir(cpu_traces_dir)]

for workload in workloads:
    workload_dir=join(cpu_traces_dir, workload)
    traces=[f for f in listdir(workload_dir)]

    for trace in traces:
        print workload
        print trace.split(".")[0]

        args = ["./Sim", workload, trace.split(".")[0], "--config", "configs/sample-FRFCFS-64MB-eDRAM-32GB-PCM.cfg", "--traces", join(workload_dir, trace)]
        subprocess.call(args)
