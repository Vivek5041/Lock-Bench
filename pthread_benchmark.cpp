#include<iostream>
#include<stdio.h>
#include<cstdlib>
#include<pthread.h>
#include<sys/time.h>

#ifdef SHIELD
#include "shielding_array.h"
#endif
using namespace std;

#define NUM_ITERATIONS 100000000
#define NUM_WARMUPITERATIONS 10000
// CPU ranges
#define CPU_RANGE1_START 64
#define CPU_RANGE1_END 127
#define CPU_RANGE2_START 192
#define CPU_RANGE2_END 255

int numWorkers;
pthread_mutex_t mylock;
pthread_barrier_t my_barrier;
struct timeval timeStart, timeEnd;

void set_cpu_affinity(int thread_index) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    
    int cpu_id;
    if (thread_index < 64) {
        cpu_id =  CPU_RANGE1_START + thread_index;
    } else {
        cpu_id =  CPU_RANGE2_START + (thread_index - 64);
    }
    CPU_SET(cpu_id, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

void* warmupFunction(void* arg) {
    //long args = (long)arg;
    long thread_index = *(long*)arg;
    set_cpu_affinity(thread_index);
    // Warm-up phase
    for (long i = 0; i < NUM_WARMUPITERATIONS; i++) {
        pthread_mutex_lock(&mylock);
        pthread_mutex_unlock(&mylock);
    }
    return nullptr;
}

void* mainThreadFunction(void* arg) {
    //long args = (long)arg;
    long thread_index = *(long*)arg;
    set_cpu_affinity(thread_index);
    // Main timed phase

    // Calculate iterations per thread
    long long iterations_per_thread = NUM_ITERATIONS / numWorkers;
    long long remaining_iterations = NUM_ITERATIONS % numWorkers;
    
    // Give remaining iterations to the first few threads
    if (thread_index < remaining_iterations) {
        iterations_per_thread++;
    }

    for (long i = 0; i < NUM_WARMUPITERATIONS; i++) {
#ifdef SHIELD
	LS_ACQUIRE(&mylock, false, pthread_mutex_lock);
        LS_RELEASE(&mylock, false, pthread_mutex_unlock);
#else
        pthread_mutex_lock(&mylock);
        pthread_mutex_unlock(&mylock);
#endif
    }
    pthread_barrier_wait(&my_barrier);
    if(thread_index == 0)
	gettimeofday(&timeStart, 0);

    for (long i = 0; i < iterations_per_thread; i++) {
#ifdef SHIELD
        LS_ACQUIRE(&mylock, false, pthread_mutex_lock);
        LS_RELEASE(&mylock, false, pthread_mutex_unlock);
#else
        pthread_mutex_lock(&mylock);
        pthread_mutex_unlock(&mylock);
#endif
    }
     pthread_barrier_wait(&my_barrier);
     if(thread_index == 0)
         gettimeofday(&timeEnd, 0);
    return nullptr;
}

int main(int argc, char* argv[]){
    if(argc != 2) {
        printf("usage:./<exe> <num_threads>\n");
        exit(0);
    }

#ifdef RECURSIVE
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&mylock, &attr);
    pthread_mutexattr_destroy(&attr);
#elif defined(ERRORCHECK)
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    pthread_mutex_init(&mylock, &attr);
    pthread_mutexattr_destroy(&attr);    
#else
	pthread_mutex_init(&mylock,NULL);
#endif


	
	long int i=0;
	long long elapsed=0;
	//initializing number of workers.
	numWorkers=atoi(argv[1]);
	pthread_t Threads[numWorkers];
    pthread_barrier_init(&my_barrier,NULL, numWorkers);    
	long thread_indices[numWorkers];    
	// struct timeval timeStart, timeEnd;

	// printf("Num of thread:%d\n",numWorkers);
	// gettimeofday(&timeStart, 0);

    // Create threads and distribute workload
    for (int i = 0; i < numWorkers; i++) {
	thread_indices[i] = i;
        pthread_create(&Threads[i], nullptr, mainThreadFunction, &thread_indices[i]);
    }

    // Wait for main threads to finish
    for (int i = 0; i < numWorkers; i++) {
        pthread_join(Threads[i], nullptr);
    }
	
	// gettimeofday(&timeEnd, 0);
	elapsed = (timeEnd.tv_sec-timeStart.tv_sec)*1000000LL + timeEnd.tv_usec-timeStart.tv_usec;
	//printf ("\nDone. Throughput:	%f	calls/sec\n",NUM_ITERATIONS/(elapsed/(double)1000000));
	  
	
	pthread_mutex_destroy(&mylock);
    pthread_barrier_destroy(&my_barrier);    
	//printf ("\nDone.	%f	sec\n",elapsed/(double)1000000);
	printf ("%d,%f,%f\n",numWorkers, elapsed/(double)1000000, NUM_ITERATIONS/(elapsed/(double)1000000));
	return 0;
}


