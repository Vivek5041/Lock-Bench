#!/bin/bash
#pwd; hostname; date
export glibc_install=/home/nikhil/glibc_install

gcc \
  -L "${glibc_install}/lib" \
  -I "${glibc_install}/include" \
  -Wl,--rpath="${glibc_install}/lib" \
  -Wl,--dynamic-linker="${glibc_install}/lib/ld-linux-x86-64.so.2" \
  -std=c11 \
  -o pthread_benchmark_ls_normal \
  -v \
  pthread_benchmark.c \
  -lpthread \
;
#ldd ./pthread_benchmark_ls_normal
#./pthread_benchmark_ls_normal 64
gcc \
  -L "${glibc_install}/lib" \
  -I "${glibc_install}/include" \
  -Wl,--rpath="${glibc_install}/lib" \
  -Wl,--dynamic-linker="${glibc_install}/lib/ld-linux-x86-64.so.2" \
  -std=c11 \
  -o pthread_benchmark_ls_reentrant \
  -v \
  pthread_benchmark.c \
  -lpthread -DRECURSIVE\
;

gcc \
  -L "${glibc_install}/lib" \
  -I "${glibc_install}/include" \
  -Wl,--rpath="${glibc_install}/lib" \
  -Wl,--dynamic-linker="${glibc_install}/lib/ld-linux-x86-64.so.2" \
  -std=c11 \
  -o pthread_benchmark_ls_errorcheck \
  -v \
  pthread_benchmark.c \
  -lpthread -DERRORCHECK\
;

for i in {1..10}
	do
	./pthread_benchmark_normal 64 >>results/pthread_benchmark_normal.csv
  ./pthread_benchmark_ls_normal 64 >>results/pthread_benchmark_ls_normal.csv
	./pthread_benchmark_ls_reentrant 64 >>results/pthread_benchmark_ls_reentrant.csv
	./pthread_benchmark_ls_errorcheck 64 >>results/pthread_benchmark_ls_errorcheck.csv
	done
date


