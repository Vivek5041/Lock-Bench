#include <iostream>
#include <vector>
#include <thread>
#include <mutex>
#include <cstdlib>
#include <unistd.h>
#include <sys/time.h>
#include <atomic>
#include <pthread.h>
#include <sched.h>
#ifdef RW
#include <shared_mutex>
#endif
#ifdef SHIELD_A
#include "shielding_array.h"
#endif
using namespace std;

// CPU ranges
#define CPU_RANGE1_START 64
#define CPU_RANGE1_END 127
#define CPU_RANGE2_START 192
#define CPU_RANGE2_END 255

#define NUM_ITERATIONS 1000000000
#define NUM_WARMUPITERATIONS 10000

#ifdef NESTED
std::recursive_mutex myNestLock;
#elif defined(RW)
std::shared_mutex rwlock;
#else
std::mutex mylock;
#endif


std::atomic<int> ready_count(0);
std::atomic<bool> start_flag(false);

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

// Simulated work function
void do_work(int amount) {
    volatile int dummy = 0;
    for (int i = 0; i < amount; i++) {
        dummy += i;
    }
}

// Function executed by each thread
void thread_function(int thread_id, int num_threads)  {
    // Warm-up phase
    set_cpu_affinity(thread_id);
    for (long int i = 0; i < NUM_WARMUPITERATIONS; i++) {
#ifdef NESTED
        myNestLock.lock();
        myNestLock.unlock();

#elif defined(SHIELD_A)
        LS_ACQUIRE(&mylock, false, [](void* l){ ((std::mutex*)l)->lock(); });
        LS_RELEASE(&mylock, false, [](void* l){ ((std::mutex*)l)->unlock(); });
#elif defined(RW)
	std::shared_lock<std::shared_mutex> lock(rwlock);
#else
        mylock.lock();
        mylock.unlock();
#endif
    }

    // Calculate iterations per thread
    long long iterations_per_thread = NUM_ITERATIONS / num_threads;
    long long remaining_iterations = NUM_ITERATIONS % num_threads;
    
    // Give remaining iterations to the first few threads
    if (thread_id < remaining_iterations) {
        iterations_per_thread++;
    }

    // Signal ready and wait at atomic barrier
    ready_count.fetch_add(1);
    while (!start_flag.load(std::memory_order_acquire))
        std::this_thread::yield();


    // Lock testing loop
    for (long int i = 0; i < iterations_per_thread; i++) {
#ifdef NESTED
        myNestLock.lock();
//        do_work(100);
        myNestLock.unlock();
#elif defined(SHIELD_A)
        LS_ACQUIRE(&mylock, false, [](void* l){ ((std::mutex*)l)->lock(); });
        LS_RELEASE(&mylock, false, [](void* l){ ((std::mutex*)l)->unlock(); });

#elif defined(RW)
	std::shared_lock<std::shared_mutex> lock(rwlock);

#else
        mylock.lock();
//        do_work(100);
        mylock.unlock();
#endif
    }

    // End timing
    // auto end = std::chrono::high_resolution_clock::now();

    // Compute elapsed time in microseconds
    // if (thread_id == 0) {
    //     elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    // }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        cout << "Usage: ./<exe> <num_threads>" << endl;
        return 1;
    }

    int numWorkers = atoi(argv[1]);
    vector<thread> threads;
    struct timeval timeStart, timeEnd;
    long long elapsed = 0;

    // Launch threads
    for (int i = 0; i < numWorkers; i++) {
        threads.emplace_back(thread_function, i, numWorkers);
    }

    // Wait until all threads are ready
    while (ready_count.load(std::memory_order_acquire) < numWorkers)
        std::this_thread::yield();

    // Start benchmark
    gettimeofday(&timeStart, nullptr);
    start_flag.store(true, std::memory_order_release);

    // Join all threads
    for (auto &t : threads)
        t.join();

    // End benchmark
    gettimeofday(&timeEnd, nullptr);
    elapsed = (timeEnd.tv_sec - timeStart.tv_sec) * 1000000LL +
              (timeEnd.tv_usec - timeStart.tv_usec);

    printf("%d,%f,%f\n", numWorkers, elapsed / 1e6, NUM_ITERATIONS / (elapsed / 1e6));
    return 0;
}



