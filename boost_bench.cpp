#include <iostream>
#include <cstdlib>
#include <vector>
#include <boost/thread/thread.hpp>    // for boost::thread
#include <boost/thread/mutex.hpp>     // for boost::mutex
#include <boost/thread/recursive_mutex.hpp> // for boost::recursive_mutex
#include <boost/thread/locks.hpp>     // for boost::lock_guard
#include <boost/thread/barrier.hpp>   // for boost::barrier
#include <boost/chrono.hpp>
#include <sched.h>

// Use a descriptive name for the total workload
#define TOTAL_ITERATIONS 100000000LL // Use LL for long long literal
#define NUM_WARMUPITERATIONS 10000

// CPU ranges
#define CPU_RANGE1_START 0
#define CPU_RANGE1_END 23
#define CPU_RANGE2_START 48
#define CPU_RANGE2_END 71

#ifdef RECURSIVE
boost::recursive_mutex mylock;
#else
boost::mutex mylock;
#endif

// Global timing variables
boost::chrono::high_resolution_clock::time_point timeStart, timeEnd;

// Sets the CPU affinity for the current thread
void set_cpu_affinity(int thread_index) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);

    // This logic correctly maps threads to two different CPU ranges
    int cpu_id = (thread_index < 24) ? CPU_RANGE1_START + thread_index : CPU_RANGE2_START + (thread_index - 24);
    CPU_SET(cpu_id, &cpuset);

    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == -1) {
        std::cerr << "Warning: Could not set CPU affinity for thread " << thread_index << std::endl;
    }
}

// Combined function that does both warmup and measurement
void combinedThreadFunction(int thread_index, long long iterations_per_thread, long long extra_iterations, boost::barrier& sync_barrier) {
    set_cpu_affinity(thread_index);

    // Warmup phase - not timed
    for (long i = 0; i < NUM_WARMUPITERATIONS; i++) {
#ifdef RECURSIVE
        boost::lock_guard<boost::recursive_mutex> lock(mylock);
#else
        boost::lock_guard<boost::mutex> lock(mylock);
#endif
    }

    // Wait for all threads to complete warmup before starting measurement
    sync_barrier.wait();

    // First thread to pass the barrier starts the timer
    if (thread_index == 0) {
        timeStart = boost::chrono::high_resolution_clock::now();
    }

    // Measurement phase - this will be timed
    long long total_iterations_for_this_thread = iterations_per_thread + (thread_index == 0 ? extra_iterations : 0);
    for (long long i = 0; i < total_iterations_for_this_thread; i++) {
#ifdef RECURSIVE
        boost::lock_guard<boost::recursive_mutex> lock(mylock);
#else
        boost::lock_guard<boost::mutex> lock(mylock);
#endif
    }
    sync_barrier.wait();
    if (thread_index == 0) {
        timeEnd = boost::chrono::high_resolution_clock::now();
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: ./<exe> <num_threads>" << std::endl;
        return 1;
    }

    int numWorkers = atoi(argv[1]);
    if (numWorkers <= 0) {
        std::cerr << "Error: Number of threads must be positive." << std::endl;
        return 1;
    }

    std::vector<boost::thread> Threads;

    // Calculate the number of iterations for each thread to perform.
    long long iterations_per_thread = TOTAL_ITERATIONS / numWorkers;
    long long extra_iterations = TOTAL_ITERATIONS % numWorkers;  // Handle remainder

    // Create a barrier that will wait for all 'numWorkers' threads.
    boost::barrier sync_barrier(numWorkers);

    // Start all threads - they will do warmup first, then synchronize, then do measured work
    for (int i = 0; i < numWorkers; i++) {
        Threads.emplace_back(combinedThreadFunction, i, iterations_per_thread, extra_iterations, boost::ref(sync_barrier));
    }

    for (auto& th : Threads) {
        th.join();
    }

    //timeEnd = boost::chrono::high_resolution_clock::now();

    double elapsed = boost::chrono::duration<double>(timeEnd - timeStart).count();

    // The throughput calculation now correctly reflects the total work done.
    double throughput = static_cast<double>(TOTAL_ITERATIONS) / elapsed;

    std::cout << numWorkers << "," << elapsed << "," << throughput << std::endl;

    return 0;
}
