#!/bin/bash
#SBATCH --job-name=PARALLEL    	      # Job name
#SBATCH --mail-type=END,FAIL          # Mail events (NONE, BEGIN, END, FAIL, ALL)
#SBATCH --mail-user=212011006@iitdh.ac.in #re to send mail	
#SBATCH --nodes=1                     # Run on a single node
#SBATCH --ntasks-per-node=1           # Single task
#SBATCH --cpus-per-task=16            # 32 CPUs per task
#SBATCH --time=00:55:00               # Time limit hrs:min:sec
#SBATCH --output=output_%j.log   # Standard output and error log
pwd; hostname; date
for i in 1 2
do
	for k in 1 2 3 4 5
	do
		./omp_lock_benchmark $i
	done
done
date


