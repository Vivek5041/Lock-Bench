#!/bin/bash
gcc -O3 -o hierarchical_benchmark hierarchical_lock_benchmark.c -lpthread
gcc -O3 -o hierarchical_benchmark_PIN hierarchical_lock_benchmark.c -lpthread -DPIN_THR
gcc -O3 cpu_affinity.c -o cpu_affinity -lpthread
gcc -O3 cpu_hiera.c -o cpu_hiera -lpthread
