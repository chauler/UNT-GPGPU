#!/bin/bash

# cfd streamcluster
for benchmark in backprop hotspot heartwall srad_v2 pf_naive pathfinder myocyte lavaMD gaussian b+tree
do
		cd  rodinia/$benchmark/
		qsub pbs_$benchmark.pbs
		cd -
done
