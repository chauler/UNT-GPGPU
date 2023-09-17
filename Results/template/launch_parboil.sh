#!/bin/bash

# fft
# lbm tpacf
for benchmark in histo mm sad spmv
do
        	cd  parboil/$benchmark/
		qsub pbs_$benchmark.pbs
		cd -
done
