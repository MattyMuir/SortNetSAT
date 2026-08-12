#!/bin/bash
 
# Request resources:
#SBATCH -N 1
#SBATCH -c 128
#SBATCH --mem=10G
#SBATCH --time=00:15:00
#SBATCH --gres=tmp:1G
#SBATCH -p test
#SBATCH -o runs/test-out.log
#SBATCH -e runs/test-err.log
 
# Commands to be run:
perf stat -e cycles,instructions,cache-references,cache-misses,task-clock -o perf_stat.txt -- ./build/src/SortNetSAT
#./build/src/SortNetSAT
