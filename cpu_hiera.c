#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <sched.h>

#define TOTAL_LOCKS 2000
#define HIERARCHY_LEVELS 400
#define LOCKS_PER_GROUP (TOTAL_LOCKS / HIERARCHY_LEVELS)
#define NUM_ITERATIONS 100000000 //100 Millions
#define WARMUP_ITERATIONS 10000
#define NUM_CPUS 128

// CPU ranges
#define CPU_RANGE1_START 64
#define CPU_RANGE1_END 127
#define CPU_RANGE2_START 192
#define CPU_RANGE2_END 255

// Volatile flag for graceful termination
volatile sig_atomic_t keep_running = 1;

// Structure for a lock group in hierarchy
typedef struct {
    pthread_mutex_t* locks;
    int num_locks;
    int level;
} lock_group_t;

// Thread arguments structure
typedef struct {
    int thread_id;
    int num_threads;
    int nesting_depth;
    int work_amount;
    lock_group_t* lock_hierarchy;
    uint64_t* ops_completed;
    uint64_t thread_seed;  // Per-thread random seed
} thread_args_t;

// Global variables
uint64_t total_operations = 0;
lock_group_t lock_hierarchy[HIERARCHY_LEVELS];
uint64_t global_time = 0;
pthread_barrier_t warmup_barrier;  // Barrier for warmup synchronization
pthread_barrier_t timing_barrier;  // Barrier for timing synchronization
struct timespec benchmark_start;   // Global timing start point
struct timespec benchmark_end;   // Global timing end point
struct timespec init_start;   // Global timing start point
struct timespec init_end;   // Global timing end point
struct timespec clean_start;   // Global timing start point
struct timespec clean_end;   // Global timing start point

// Fast random number generator (xoshiro256**)
static inline uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

uint64_t next_random(uint64_t* state) {
    const uint64_t result = rotl(state[1] * 5, 7) * 9;
    const uint64_t t = state[1] << 17;
    state[2] ^= state[0];
    state[3] ^= state[1];
    state[1] ^= state[2];
    state[0] ^= state[3];
    state[2] ^= t;
    state[3] = rotl(state[3], 45);
    return result;
}

// Initialize random state
void init_random_state(uint64_t* state, uint64_t seed) {
    state[0] = seed;
    state[1] = seed ^ 0x1234567890abcdefULL;
    state[2] = seed ^ 0xfedcba0987654321ULL;
    state[3] = seed ^ 0xabcdef0123456789ULL;
    // Warm up the RNG
    for(int i = 0; i < 10; i++) {
        next_random(state);
    }
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

// Get next CPU in the allowed ranges
int get_next_cpu(int thread_id) {
    if (thread_id < 64) {
        return CPU_RANGE1_START + thread_id;
    } else {
        return CPU_RANGE2_START + (thread_id - 64);
    }
}

// Set CPU affinity for the current thread
void set_cpu_affinity(int thread_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    int cpu = get_next_cpu(thread_id);
    CPU_SET(cpu, &cpuset);
    
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}

// Acquire locks following hierarchy
void acquire_hierarchical_locks(lock_group_t* hierarchy, int start_level, int depth, int work_amount, uint64_t* random_state) {
    if (depth <= 0 || !keep_running) return;
    
    // Get random lock from current level using our fast RNG
    int lock_idx = next_random(random_state) % hierarchy[start_level].num_locks;
    
    pthread_mutex_lock(&hierarchy[start_level].locks[lock_idx]);
    
    if (depth > 1 && start_level + 1 < HIERARCHY_LEVELS) {
        acquire_hierarchical_locks(hierarchy, start_level + 1, depth - 1, work_amount,  random_state);
    }
    
    do_work(work_amount);
    pthread_mutex_unlock(&hierarchy[start_level].locks[lock_idx]);
}

void* worker_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    uint64_t local_ops = 0;
    uint64_t random_state[4];
    
    // Set CPU affinity
    set_cpu_affinity(args->thread_id);
    
    // Initialize thread-local random state
    init_random_state(random_state, args->thread_seed);

    // Calculate iterations per thread
    int iterations_per_thread = NUM_ITERATIONS / args->num_threads;
    // Handle remainder for last thread
    if (args->thread_id == args->num_threads - 1) {
        iterations_per_thread += NUM_ITERATIONS % args->num_threads;
    }
    
    // WARMUP PHASE
    for(int i = 0; i < WARMUP_ITERATIONS && keep_running; i++) {
        int start_level = next_random(random_state) % (HIERARCHY_LEVELS - args->nesting_depth + 1);
        acquire_hierarchical_locks(args->lock_hierarchy, start_level, 1, 1, random_state);
        //do_work(args->work_amount);
    }
    
    // Wait for all threads to complete warmup
    pthread_barrier_wait(&warmup_barrier);
    
    // Timing barrier - thread 0 records start time, all threads sync before benchmark
    if (args->thread_id == 0) {
        clock_gettime(CLOCK_MONOTONIC, &benchmark_start);
    }
    pthread_barrier_wait(&timing_barrier);
    
    // BENCHMARK PHASE - This is what gets timed
    for(int i = 0; i <  iterations_per_thread && keep_running; i++) {
        int start_level = next_random(random_state) % (HIERARCHY_LEVELS - args->nesting_depth + 1);
        acquire_hierarchical_locks(args->lock_hierarchy, start_level, args->nesting_depth, args->work_amount, random_state);
        //do_work(args->work_amount);
        local_ops++;
    }

    // Wait for all threads to complete their iterations
    pthread_barrier_wait(&timing_barrier);
    
    // Thread 0 records the precise end time after all threads finish
    if (args->thread_id == 0) {
        clock_gettime(CLOCK_MONOTONIC, &benchmark_end);
    }    

    args->ops_completed[args->thread_id] = local_ops;
    return NULL;
}

void run_benchmark(int num_threads, int nesting_depth, int work_amount) {
    pthread_t* threads = malloc(sizeof(pthread_t) * num_threads);
    thread_args_t* thread_args = malloc(sizeof(thread_args_t) * num_threads);
    uint64_t* ops_completed = calloc(num_threads, sizeof(uint64_t));
    uint64_t base_seed = 67890;

    clock_gettime(CLOCK_MONOTONIC, &init_start);    
    init_lock_hierarchy();
    clock_gettime(CLOCK_MONOTONIC, &init_end);    
    // Initialize barriers for synchronization
    if (pthread_barrier_init(&warmup_barrier, NULL, num_threads) != 0) {
        perror("Failed to initialize warmup barrier");
        exit(1);
    }
    if (pthread_barrier_init(&timing_barrier, NULL, num_threads) != 0) {
        perror("Failed to initialize timing barrier");
        exit(1);
    }
    
    //printf("\nStarting benchmark with warmup phase...\n");
    
    // Create all threads - they will do warmup, sync at barriers, then benchmark
    for(int i = 0; i < num_threads; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].num_threads = num_threads;
        thread_args[i].nesting_depth = nesting_depth;
        thread_args[i].work_amount = work_amount;
        thread_args[i].lock_hierarchy = lock_hierarchy;
        thread_args[i].ops_completed = ops_completed;
        thread_args[i].thread_seed = base_seed + i;
        
        pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]);
    }
    
    // Wait for all threads to complete
    for(int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_operations += ops_completed[i];
    }
    
    // Record end time after all threads complete
    // struct timespec benchmark_end;
    // clock_gettime(CLOCK_MONOTONIC, &benchmark_end);
    
    // Calculate duration of benchmark phase only
    double duration = (benchmark_end.tv_sec - benchmark_start.tv_sec) + 
                     (benchmark_end.tv_nsec - benchmark_start.tv_nsec) / 1e9;
    
    //printf("%d,%d,%d,%.2f,%.2f\n",num_threads, nesting_depth, work_amount,duration, total_operations / duration);
    
    // Cleanup
    pthread_barrier_destroy(&warmup_barrier);
    pthread_barrier_destroy(&timing_barrier);
    clock_gettime(CLOCK_MONOTONIC, &clean_start);
    cleanup_lock_hierarchy();
    clock_gettime(CLOCK_MONOTONIC, &clean_end);
    free(threads);
    free(thread_args);
    free(ops_completed);
    double init_time = (init_end.tv_sec - init_start.tv_sec) + 
                     (init_end.tv_nsec - init_start.tv_nsec) / 1e9;
    double clean_time = (clean_end.tv_sec - clean_start.tv_sec) + 
                     (clean_end.tv_nsec - clean_start.tv_nsec) / 1e9;
    printf("%d,%d,%d,%.4f,%.4f,%.2f,%.2f\n",num_threads, nesting_depth, work_amount, init_time, clean_time, duration, total_operations / duration);
}

int main(int argc, char* argv[]) {
    if(argc != 4) {
        printf("Usage: %s <num_threads> <nesting_depth> <work_amount>\n", argv[0]);
        exit(1);
    }
    
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
