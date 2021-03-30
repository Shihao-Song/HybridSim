import sys
from os import listdir
from os.path import isfile, join
import subprocess
import math

workloads_dir = sys.argv[1]
pref = sys.argv[2]
pref_size = sys.argv[3]
num_worker = sys.argv[4]
worker_id = sys.argv[5]

if __name__ == "__main__":

    workloads_all = [f for f in listdir(workloads_dir)]
    workloads_all.sort()
    # print(workloads_all)

    workloads = []
    stride = int(math.ceil(float(len(workloads_all)) / float(num_worker)))
    for i in range(int(worker_id) * stride, int(worker_id) * stride + stride):
        if i == len(workloads_all): break
        workloads.append(workloads_all[i])

    # print(workloads)
    # exit(0)
    for workload in workloads:
        trace_fn = join(workloads_dir, workload)
        pattern_fn = join("patterns/50", workload.split('.')[0] + ".pattern")
        out_fn = join("stats", workload.split(".")[0] + "." + pref + "." + pref_size + ".stats")
        # print(trace_fn, pattern_fn, out_fn)

        if workload == "roms.cpu_trace":
            repeat = 4
        else:
            repeat = 8

        trace_list = ["--trace", trace_fn, "--pref-patterns", pattern_fn] * repeat
        # print(trace_list)

        calls = ["./HybridSim", "--mode", "hybrid", "--dram-config", "configs/hybrid-dram-small.cfg", "--pcm-config", "configs/hybrid-pcm.cfg"]
        calls = calls + trace_list
        calls.append("--pattern-selection")
        calls.append(pref)
        calls.append("--pref-num")
        calls.append(pref_size)
        calls.append("--stat_output")
        calls.append(out_fn)
        print(calls)
        subprocess.call(calls)
        print("")
'''
    exe = join("./", thr)
    out_dir = join("patterns", thr + "0")

    for workload in workloads:
        trace_fn = join(workloads_dir, workload)
        out_fn = join(out_dir, workload.split('.')[0] + ".pattern")
        calls = [exe, "--mode", "pref-patterns", "--trace", trace_fn, "--pref_patterns_output", out_fn]
        print calls
        subprocess.call(calls)
'''
