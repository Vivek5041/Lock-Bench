#!/bin/bash

g++ omp_bench.cpp -o omp_bench -fopenmp
g++ omp_bench.cpp -o omp_bench_nested -DNESTED -fopenmp
