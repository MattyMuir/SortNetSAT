#!/bin/bash
 
# Request resources:
#SBATCH -N 1
#SBATCH -c 128
#SBATCH --mem=30G
#SBATCH --time=00:30:00
#SBATCH --gres=tmp:1G
#SBATCH -p multi
#SBATCH -o runs/out-%j.log
#SBATCH -e runs/err-%j.log
 
# Commands to be run:
./build/src/SortNetSAT
