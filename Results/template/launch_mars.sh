#!/bin/bash


for benchmark in PageViewCount PageViewRank SimilarityScore InvertedIndex 
do
	    cd  Mars/$benchmark/
	    qsub pbs_$benchmark.pbs
	    cd -
done

