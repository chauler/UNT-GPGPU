#!/bin/bash

#for benchmark in AES BlackScholes CP CONS FWT JPEG NQU RAY SCP STO 
for benchmark in AES CP NQU RAY STO
do
	    cat script_base_CUDA_1.pbs | sed -e "s/JPEG/$benchmark/g" > CUDA/$benchmark/pbs_$benchmark.pbs
done

#for benchmark in kmeans BFS LIB LPS MUM NN SLA TRA 
for benchmark in BFS LIB LPS MUM NN
do
	    cat script_base_CUDA_2.pbs | sed -e "s/kmeans/$benchmark/g" > CUDA/$benchmark/pbs_$benchmark.pbs
done

