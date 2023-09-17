#!/bin/bash


#for benchmark in AES BFS BlackScholes CP JPEG kmeans LIB MUM NN NQU RAY SCP STO 
for benchmark in AES BFS CP LIB MUM NN NQU RAY STO
do
	    cd  CUDA/$benchmark/
		qsub pbs_$benchmark.pbs
		cd -
done
