#include<iostream>
#include<stdio.h>
#include<cstdlib>
#include<omp.h>
#include<sys/time.h>
using namespace std;

#define NUM_ITERATIONS 100000000
#define NUM_WARMUPITERATIONS 10000
//omp_lock_t mylock;
pthread_mutex_t mylock;
int main(int argc, char* argv[]){
	//omp_init_lock(&mylock);
	pthread_mutex_init(&mylock,NULL);

	int warmupIterations=10000;
	if(argc != 2) {
		printf("usage:./<exe> <num_threads>\n");
		exit(0);
	}
	
	int threadID; 
	long int i=0;
	long long elapsed=0;
	//initializing number of workers.
	int numWorkers=atoi(argv[1]);

	struct timeval timeStart, timeEnd;
	//timeStart=(struct timeval*)malloc(sizeof(struct timeval)*numWorkers);
	//timeEnd=(struct timeval*)malloc(sizeof(struct timeval)*numWorkers);

	//allocating memory for matrices.
	
	omp_set_num_threads(numWorkers);

	//printf("Num of thread:%d\n",numWorkers);
	//initializing matrices in parallel.
#pragma omp parallel shared(timeStart, timeEnd) private(threadID)
	{
	#pragma omp for 
	for (i=0; i<NUM_WARMUPITERATIONS; i++){
		//omp_set_lock(&mylock);
		//omp_unset_lock(&mylock);
		pthread_mutex_lock (&mylock);
        	pthread_mutex_unlock (&mylock);
	}

	threadID=omp_get_thread_num();
	//printf("Num of thread:%d\n",numWorkers);
#pragma omp barrier
	if(threadID == 0)
		gettimeofday(&timeStart, 0);
	#pragma omp for
	for (i=0; i<NUM_ITERATIONS; i++){
		//omp_set_lock(&mylock);
		//omp_unset_lock(&mylock);
		pthread_mutex_lock(&mylock);
		pthread_mutex_unlock(&mylock);
	}
#pragma omp barrier
	if(threadID == 0){
		gettimeofday(&timeEnd, 0);
		elapsed = (timeEnd.tv_sec-timeStart.tv_sec)*1000000LL + timeEnd.tv_usec-timeStart.tv_usec;
		//printf ("\nDone. Throughput:	%f	calls/sec\n",NUM_ITERATIONS/(elapsed/(double)1000000));
	   }
	}

	//omp_destroy_lock(&mylock);
	pthread_mutex_destroy(&mylock);
	//printf ("\nDone.	%f	sec\n",elapsed/(double)1000000);
	printf ("%d,%f,%f\n",numWorkers, elapsed/(double)1000000, NUM_ITERATIONS/(elapsed/(double)1000000));
	return 0;
}

