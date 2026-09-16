#!/bin/bash
 
# Request resources:
#SBATCH -N 1
#SBATCH -c 128
#SBATCH --mem=100G
#SBATCH --time=00:15:00
#SBATCH --gres=tmp:1G
#SBATCH -p test
#SBATCH -o runs/test-out-%j.log
#SBATCH -e runs/test-err-%j.log
 
# Commands to be run:
./build/src/SortNetSAT