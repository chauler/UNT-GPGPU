#!/bin/bash


for benchmark in BFS DeviceMemory MaxFlops MD QTC Reduction Scan Spmv Stencil2D Triad
do
	    cd  shoc/$benchmark/
		qsub pbs_$benchmark.pbs
		cd -
done
