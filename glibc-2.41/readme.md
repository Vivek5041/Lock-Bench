This directory contains all the changes necessary to integrate the C implementation of LockShield with glibc-2.41.

- Download glibc-2.41 (on Ubuntu 24.04, the default version is 2.39. However, downloading glibc2.39 source and building results in build errors, which were patched in glibc2.41) : `wget https://github.com/bminor/glibc/archive/refs/tags/glibc-2.41.zip`

- Copy all the files present in teh nptl directory of this repo (`Makefile`, `pthread_mutex_lock.c`, `pthread_mutex_unlock.c`, `shield_arr.c`, `shield_arr.h`) to the nptl directory of the downloaded source.

- Create a folder `build` inside the downloaded glibc source.

- execute `../configure --prefix=$HOME/glibc_install` followed by `make` and `make install` to build glibc-2.41 with LockShielding integrated.

- execute `gcc pthread_benchmark.c -o pthread_benchmark_normal -lpthread` to create an executable linked against the default glibc (2.39) on Ubuntu 24.04.

- execute `./pthread_ls.sh` to build and run the test program (synthetic benchmark calling lock()-unlock() operations) with pthread normal mutex, pthread recursive mutex, and pthread errorcheck mutex (all using glibc integrated with LockShielding). This creates 3 executables: `pthread_benchmark_ls_normal`, `pthread_benchmark_ls_reentrant`, `pthread_benchmark_ls_errorcheck`.

- the results folder would contain the results.

-the bin folder contains the binaries used to obtain the results files present in the `results` folder (`pthread_benchmark_ls_normal_ref.csv`, `pthread_benchmark_ls_reentrant_ref.csv`, `pthread_benchmark_ls_errorcheck_ref.csv`, `pthread_benchmark_normal_ref.csv`)
