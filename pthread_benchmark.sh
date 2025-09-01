#!/bin/bash
pwd; hostname; date
g++ -O3 pthread_benchmark.cpp -o pthread_benchmark_normal -lpthread

g++ -O3 pthread_benchmark.cpp -o pthread_benchmark_reentrant -lpthread -DRECURSIVE

g++ -O3 pthread_benchmark.cpp -o pthread_benchmark_errorcheck -lpthread -DERRORCHECK

g++ -O3 pthread_benchmark.cpp -o pthread_benchmark_shield_hash_re -lpthread -DSHIELD_H

g++ -O3 pthread_benchmark.cpp -o pthread_benchmark_shield_array_re -lpthread -DSHIELD_A

g++ -O3 pthread_benchmark.cpp -o pthread_benchmark_rwlock -lpthread -DRWLOCK

g++ -O3 omp_bench.cpp -o omp_bench -fopenmp

g++ -O3 omp_bench.cpp -o omp_bench_nested -DNESTED -fopenmp

g++ -O3 mutex_bench.cpp -o mutex_bench_recur -DNESTED

g++ -O3 mutex_bench.cpp -o mutex_bench

g++ -O3 mutex_bench.cpp -o mutex_bench_shield -DSHIELD_A

g++ -O3 mutex_bench.cpp -o mutex_bench_rw -DRW

g++ -std=c++11 -O3 -o pthread_rwbenchmark pthread_rwbench.cpp -lpthread

#export LD_LIBRARY_PATH=/home/vivek/Shield/lib:$LD_LIBRARY_PATH

for i in {1..10}
	do
#	./pthread_benchmark_normal 1 >>results/pthread_benchmark_normal1.csv
#	./pthread_benchmark_reentrant 1 >>results/pthread_benchmark_reentrant1.csv
#	./pthread_benchmark_errorcheck 1 >>results/pthread_benchmark_errorcheck1.csv
#	./pthread_benchmark_shield_array_re 1 >>results/pthread_benchmark_shield_a_re1.csv
#	./pthread_benchmark_shield_hash_re 1 >>results/pthread_benchmark_shield_h_re1.csv
#	./omp_bench 1 >>results/omp_bench1.csv
#	./omp_bench_nested 1 >>results/omp_bench_nested1.csv
#	./mutex_bench_recur 1 >>results/mutex_bench_recur1.csv
#	./mutex_bench 1 >>results/mutex_bench1.csv
	./../../litl/libmcs_spinlock.sh ./pthread_benchmark_normal 1 >>results/mcs_pthread1.csv
	./../../PLiTL/libmcs_spinlock.sh ./pthread_benchmark_normal 1 >>results/mcs_pthread_pid1.csv
	./../../SLiTL/libmcs_spinlock.sh ./pthread_benchmark_normal 1 >>results/mcs_pthread_arr1.csv
       ./../../ELiTL/libmcs_spinlock.sh ./pthread_benchmark_normal 1 >>results/mcs_pthread_hash1.csv
#	./pthread_benchmark_shield 3
	done
date


