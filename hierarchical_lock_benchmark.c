#ifdef PIN_THR
#define _GNU_SOURCE  // Add this at the very top
#endif
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>

#ifdef PIN_THR
#include <sched.h>        // For CPU_SET, CPU_ZERO, etc.
#endif
#define TOTAL_LOCKS 2000
#define HIERARCHY_LEVELS 40
#define LOCKS_PER_GROUP (TOTAL_LOCKS / HIERARCHY_LEVELS)  // 40 locks per group
#define NUM_ITERATIONS 1000000
#define WARMUP_ITERATIONS 10000  // Added warmup iterations

#ifdef PIN_THR
// Function to get number of available CPUs
int get_num_cpus() {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

// Function to set CPU affinity for a thread
int set_cpu_affinity(int cpu_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    //CPU_SET(cpu_id, &cpuset);
    for (int cpu = 0; cpu < 48; cpu++) {
         CPU_SET(cpu, &cpuset);     // CPUs 0-23
         CPU_SET(cpu + 48, &cpuset); // CPUs 48-71
    }
    
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}
#endif

// Volatile flag for graceful termination
volatile sig_atomic_t keep_running = 1;

// Signal handler
void handle_sigint(int sig) {
    keep_running = 0;
}

// Structure for a lock group in hierarchy
typedef struct {
    pthread_mutex_t* locks;      // Array of locks for this level
    int num_locks;              // Number of locks in this level
    int level;                  // Hierarchy level (0 to HIERARCHY_LEVELS-1)
} lock_group_t;

// Structure for protected counter
/*typedef struct {
    pthread_mutex_t mutex;
    int value;
} protected_counter_t;
*/
// Thread arguments structure
typedef struct {
    int thread_id;
    int num_threads;
    int nesting_depth;
    int work_amount;
    lock_group_t* lock_hierarchy;
    uint64_t* ops_completed;
    int is_warmup;              // Flag to indicate warmup phase
#ifdef PIN_THR
    int cpu_id;              // Added CPU ID for affinity
#endif
} thread_args_t;

// Global variables
uint64_t total_operations = 0;
lock_group_t lock_hierarchy[HIERARCHY_LEVELS];

#ifdef PIN_THR
int num_cpus;               // Store number of available CPUs
#endif

// Get time in microseconds
uint64_t get_time_usec() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

// Simulated work
void do_work(int amount) {
    volatile int dummy = 0;
    for(int i = 0; i < amount; i++) {
        dummy += i;
    }
}

// Initialize lock hierarchy
void init_lock_hierarchy() {
    for(int level = 0; level < HIERARCHY_LEVELS; level++) {
        lock_hierarchy[level].locks = malloc(sizeof(pthread_mutex_t) * LOCKS_PER_GROUP);
        lock_hierarchy[level].num_locks = LOCKS_PER_GROUP;
        lock_hierarchy[level].level = level;
        
        for(int i = 0; i < LOCKS_PER_GROUP; i++) {
            pthread_mutex_init(&lock_hierarchy[level].locks[i], NULL);
        }
    }
}

// Cleanup lock hierarchy
void cleanup_lock_hierarchy() {
    for(int level = 0; level < HIERARCHY_LEVELS; level++) {
        for(int i = 0; i < LOCKS_PER_GROUP; i++) {
            pthread_mutex_destroy(&lock_hierarchy[level].locks[i]);
        }
        free(lock_hierarchy[level].locks);
    }
}

// Acquire locks following hierarchy
void acquire_hierarchical_locks(lock_group_t* hierarchy, int start_level, int depth) {
    if (depth <= 0 || !keep_running) return;
    
    // Get random lock from current level
    unsigned int seed = (unsigned int)pthread_self();
    int lock_idx = rand_r(&seed) % hierarchy[start_level].num_locks;
    
    // Acquire lock
    pthread_mutex_lock(&hierarchy[start_level].locks[lock_idx]);
    
    // Recursively acquire locks at next level if needed
    if (depth > 1 && start_level + 1 < HIERARCHY_LEVELS) {
        acquire_hierarchical_locks(hierarchy, start_level + 1, depth - 1);
    }
    
    // Do some work while holding locks
    do_work(10);
    
    // Release lock
    pthread_mutex_unlock(&hierarchy[start_level].locks[lock_idx]);
}

void* worker_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;

#ifdef PIN_THR    
    // Set CPU affinity for this thread
    if (set_cpu_affinity(args->cpu_id) != 0) {
        fprintf(stderr, "Warning: Failed to set CPU affinity for thread %d to CPU %d\n", 
                args->thread_id, args->cpu_id);
    }
#endif 

    uint64_t local_ops = 0;
    unsigned int seed = args->thread_id;
    int iterations = args->is_warmup ? WARMUP_ITERATIONS : NUM_ITERATIONS;
    
    for(int i = 0; i < iterations && keep_running; i++) {
        // Random starting level
        int start_level = rand_r(&seed) % (HIERARCHY_LEVELS - args->nesting_depth + 1);
        
        // Acquire locks following hierarchy
        acquire_hierarchical_locks(args->lock_hierarchy, start_level, args->nesting_depth);
        
        // Simulated work between operations
        do_work(args->work_amount);
        
        local_ops++;
    }
    
    if (!args->is_warmup) {
        args->ops_completed[args->thread_id] = local_ops;
    }
    return NULL;
}

void run_warmup(int num_threads, int nesting_depth, int work_amount) {
    // printf("Starting warmup phase with %d iterations per thread...\n", WARMUP_ITERATIONS);
    
    pthread_t* threads = malloc(sizeof(pthread_t) * num_threads);
    thread_args_t* thread_args = malloc(sizeof(thread_args_t) * num_threads);
    
    // Create warmup threads
    for(int i = 0; i < num_threads; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].num_threads = num_threads;
        thread_args[i].nesting_depth = nesting_depth;
        thread_args[i].work_amount = work_amount;
        thread_args[i].lock_hierarchy = lock_hierarchy;
        thread_args[i].is_warmup = 1;

#ifdef PIN_THR
        thread_args[i].cpu_id = i % num_cpus;  // Distribute threads across CPUs
#endif         
        pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]);
    }
    
    // Wait for warmup threads
    for(int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    free(threads);
    free(thread_args);
    // printf("Warmup phase completed.\n");
}

void run_benchmark(int num_threads, int nesting_depth, int work_amount) {
    pthread_t* threads = malloc(sizeof(pthread_t) * num_threads);
    thread_args_t* thread_args = malloc(sizeof(thread_args_t) * num_threads);
    uint64_t* ops_completed = calloc(num_threads, sizeof(uint64_t));
    
    // Initialize lock hierarchy
    init_lock_hierarchy();
    
    // Run warmup phase
    run_warmup(num_threads, nesting_depth, work_amount);
    
    // printf("Starting main benchmark phase...\n");
    
    // Start timing
    uint64_t start_time = get_time_usec();
    
    // Create threads for main benchmark
    for(int i = 0; i < num_threads; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].num_threads = num_threads;
        thread_args[i].nesting_depth = nesting_depth;
        thread_args[i].work_amount = work_amount;
        thread_args[i].lock_hierarchy = lock_hierarchy;
        thread_args[i].ops_completed = ops_completed;
        thread_args[i].is_warmup = 0;

#ifdef PIN_THR
        thread_args[i].cpu_id = i % num_cpus;  // Distribute threads across CPUs
#endif          
        pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]);
    }
    
    // Wait for threads
    for(int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_operations += ops_completed[i];
    }
    
    uint64_t end_time = get_time_usec();
    double duration = (end_time - start_time) / 1000000.0;    
    // Print results
/*    printf("\nHierarchical Lock Benchmark Results:\n");
    printf("Configuration:\n");
    printf("- Total Locks: %d\n", TOTAL_LOCKS);
    printf("- Hierarchy Levels: %d\n", HIERARCHY_LEVELS);
    printf("- Locks per Level: %d\n", LOCKS_PER_GROUP);
    printf("\nTest Parameters:\n");
    printf("- Threads: %d\n", num_threads);
    printf("- Nesting Depth: %d\n", nesting_depth);
    printf("- Work Amount: %d\n", work_amount);
    printf("\nResults:\n");
    printf("- Total Operations: %lu\n", total_operations);
    printf("- Duration: %.2f seconds\n", duration);
    printf("- Operations/second: %.2f\n", total_operations / duration);
    printf("- Average latency: %.2f microseconds\n", 
           (duration * 1000000) / total_operations);
*/
    printf("%d,%d,%d,%.2f,%.2f\n",num_threads, nesting_depth, work_amount,duration,total_operations / duration);    
    // Cleanup
    cleanup_lock_hierarchy();
    free(threads);
    free(thread_args);
    free(ops_completed);
}

int main(int argc, char* argv[]) {
    signal(SIGINT, handle_sigint);

    if(argc != 4) {
        printf("Usage: %s <num_threads> <nesting_depth> <work_amount>\n", argv[0]);
        exit(1);
    }
    
#ifdef PIN_THR
    // Get number of available CPUs
    num_cpus = get_num_cpus();
    // printf("Number of available CPUs: %d\n", num_cpus);
#endif
    int thread_counts = atoi(argv[1]);
    int nesting_depths = atoi(argv[2]);
    int work_amounts = atoi(argv[3]);
    
    if (thread_counts <= 0 || nesting_depths <= 0 || work_amounts < 0) {
        fprintf(stderr, "Error: Arguments must be positive numbers\n");
        exit(1);
    }
    
    if (nesting_depths > HIERARCHY_LEVELS) {
        fprintf(stderr, "Error: Nesting depth cannot exceed hierarchy levels (%d)\n", 
                HIERARCHY_LEVELS);
        exit(1);
    }

    run_benchmark(thread_counts, nesting_depths, work_amounts);
    
    return 0;
}
