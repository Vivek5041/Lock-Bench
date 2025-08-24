#!/bin/bash
pwd; hostname; date
g++ -O3 pthread_benchmark.cpp -o pthread_benchmark_normal -lpthread

g++ -O3 pthread_benchmark.cpp -o pthread_benchmark_reentrant -lpthread -DRECURSIVE

g++ -O3 pthread_benchmark.cpp -o pthread_benchmark_errorcheck -lpthread -DERRORCHECK

g++ -O3 pthread_benchmark.cpp -o pthread_benchmark_shield -lpthread -DSHIELD

g++ -O3 omp_bench.cpp -o omp_bench -fopenmp

g++ -O3 omp_bench.cpp -o omp_bench_nested -DNESTED -fopenmp

g++ -O3 mutex_bench.cpp -o mutex_bench_recur -DNESTED

g++ -O3 mutex_bench.cpp -o mutex_bench


#export LD_LIBRARY_PATH=/home/vivek/Shield/lib:$LD_LIBRARY_PATH

for i in {1..5}
	do
#	./pthread_benchmark_normal 64 >>results/pthread_benchmark_normal.csv
#	./pthread_benchmark_reentrant 64 >>results/pthread_benchmark_reentrant.csv
#	./pthread_benchmark_errorcheck 64 >>results/pthread_benchmark_errorcheck.csv
        ./pthread_benchmark_shield 64 >>results/pthread_benchmark_shield.csv
#	./omp_bench 64 >>results/omp_bench.csv
#	./omp_bench_nested 64 >>results/omp_bench_nested.csv
#	./mutex_bench_recur 64 >>results/mutex_bench_recur.csv
#	./mutex_bench 64 >>results/mutex_bench.csv
#	./../../litl/libmcs_spinlock.sh ./pthread_benchmark_normal 64 >>results/mcs_pthread.csv
#	./../../PLiTL/libmcs_spinlock.sh ./pthread_benchmark_normal 64 >>results/mcs_pthread_pid.csv
#	./../../SLiTL/libmcs_spinlock.sh ./pthread_benchmark_normal 64 >>results/mcs_pthread_arr.csv
#       ./../../ELiTL/libmcs_spinlock.sh ./pthread_benchmark_normal 64 >>results/mcs_pthread_hash.csv
#	./pthread_benchmark_shield 3
	done
date


