#!/bin/bash


#for benchmark in AES BFS BlackScholes CP DG JPEG kmeans LIB LPS MUM NN NQU RAY STO template WP 
for benchmark in AES BFS CP DG LIB LPS MUM NN NQU RAY STO WP
do
            cat script_base.pbs | sed -e "s/LPS/$benchmark/g" > pbs_$benchmark.pbs
done
