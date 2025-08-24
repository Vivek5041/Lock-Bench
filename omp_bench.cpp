#include<iostream>
#include<stdio.h>
#include<cstdlib>
#include<omp.h>
#include<sys/time.h>
#include <unistd.h>
using namespace std;

#define NUM_ITERATIONS 100000000
#define NUM_WARMUPITERATIONS 10000
#ifdef NESTED
omp_nest_lock_t myNestLock;
#else
omp_lock_t mylock;
#endif

// Simulated work
void do_work(int amount) {
    volatile int dummy = 0;
    for(int i = 0; i < amount; i++) {
        dummy += i;
    }
}

int main(int argc, char* argv[]){

    // Set CPU binding environment variables with corrected format
    // setenv("OMP_PROC_BIND", "spread", 1);  // Using 'spread' to distribute threads evenly
    // setenv("OMP_PLACES", "cores", 1);      // Use cores as the basic unit
    // Define explicit list of processors
    // setenv("OMP_WAIT_POLICY", "active", 1);
    // setenv("GOMP_CPU_AFFINITY", "64-127 192-255", 1);
    setenv("GOMP_CPU_AFFINITY", "64-127", 1);
#ifdef NESTED
	omp_init_nest_lock(&myNestLock);
#else
	omp_init_lock(&mylock);
#endif

	int warmupIterations=10000;
	if(argc != 2) {
		printf("usage:./<exe> <num_threads>\n");
		exit(0);
	}
	
	// int threadID; 
	long int i=0;
	long long elapsed=0;
	//initializing number of workers.
	int numWorkers=atoi(argv[1]);

	struct timeval timeStart, timeEnd;

	//allocating memory for matrices.
	omp_set_num_threads(numWorkers);

	//initializing matrices in parallel.
#pragma omp parallel shared(timeStart, timeEnd) //private(threadID)
	{
        int threadID = omp_get_thread_num();

        // Print thread binding information
        /*#pragma omp critical
        {
            printf("Thread %d is running on processor %d\n", 
                   threadID, sched_getcpu());
        }*/

	#pragma omp for 
	for (i=0; i<NUM_WARMUPITERATIONS; i++){
#ifdef NESTED
        omp_set_nest_lock(&myNestLock);
        omp_unset_nest_lock(&myNestLock);
#else
		omp_set_lock(&mylock);
		omp_unset_lock(&mylock);
#endif
	}


#pragma omp barrier
	if(threadID == 0)
		gettimeofday(&timeStart, 0);
	#pragma omp for
	for (i=0; i<NUM_ITERATIONS; i++){
#ifdef NESTED
        omp_set_nest_lock(&myNestLock);
        do_work(100);
        omp_unset_nest_lock(&myNestLock);
#else
	omp_set_lock(&mylock);
	do_work(100);
	omp_unset_lock(&mylock);
#endif
	}
#pragma omp barrier
	if(threadID == 0){
		gettimeofday(&timeEnd, 0);
		elapsed = (timeEnd.tv_sec-timeStart.tv_sec)*1000000LL + timeEnd.tv_usec-timeStart.tv_usec;
	   }
	}

#ifdef NESTED
    omp_destroy_nest_lock(&myNestLock);
#else
	omp_destroy_lock(&mylock);
#endif

	printf ("%d,%f,%f\n",numWorkers, elapsed/(double)1000000, NUM_ITERATIONS/(elapsed/(double)1000000));
	return 0;
}

