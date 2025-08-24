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
#include <errno.h>

#define TOTAL_LOCKS 4000         
#define HIERARCHY_LEVELS 1000    
#define LOCKS_PER_GROUP (TOTAL_LOCKS / HIERARCHY_LEVELS)
#define NUM_ITERATIONS 1000000   
#define WARMUP_ITERATIONS 10000  
#define NUM_CPUS 24              

#define MAX_THREADS 128
#define MAX_NESTING_DEPTH 16
#define MAX_WORK_AMOUNT 100000

/*volatile sig_atomic_t keep_running = 1;

typedef struct {
    pthread_mutex_t* locks;
    int num_locks;
    int level;
} lock_group_t;

typedef struct {
    int thread_id;
    int num_threads;
    int nesting_depth;
    int work_amount;
    lock_group_t* lock_hierarchy;
    uint64_t* ops_completed;
    int is_warmup;
    uint64_t thread_seed;
} thread_args_t;
*/

// Enhanced CPU ranges for more thread distribution
#define CPU_RANGE1_START 0
#define CPU_RANGE1_END 47
#define CPU_RANGE2_START 48
#define CPU_RANGE2_END 95

volatile sig_atomic_t keep_running = 1;

typedef struct {
    pthread_mutex_t* locks;
    int num_locks;
    int level;
} lock_group_t;

typedef struct {
    int thread_id;
    int num_threads;
    int nesting_depth;
    int work_amount;
    lock_group_t* lock_hierarchy;
    uint64_t* ops_completed;
    int is_warmup;
    uint64_t thread_seed;
} thread_args_t;

uint64_t total_operations = 0;
lock_group_t lock_hierarchy[HIERARCHY_LEVELS];
uint64_t global_time = 0;

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
int init_lock_hierarchy() {
    memset(lock_hierarchy, 0, sizeof(lock_hierarchy));
    
    for(int level = 0; level < HIERARCHY_LEVELS; level++) {
        if (LOCKS_PER_GROUP <= 0) {
            fprintf(stderr, "Invalid locks per group\n");
            return -1;
        }
        
        lock_hierarchy[level].locks = malloc(sizeof(pthread_mutex_t) * LOCKS_PER_GROUP);
        if (!lock_hierarchy[level].locks) {
            fprintf(stderr, "Memory allocation failed\n");
            return -1;
        }
        
        lock_hierarchy[level].num_locks = LOCKS_PER_GROUP;
        lock_hierarchy[level].level = level;
        
        for(int i = 0; i < LOCKS_PER_GROUP; i++) {
            if (pthread_mutex_init(&lock_hierarchy[level].locks[i], NULL) != 0) {
                fprintf(stderr, "Mutex initialization failed\n");
                return -1;
            }
        }
    }
    return 0;
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
    if (thread_id < 24) {
        return CPU_RANGE1_START + thread_id;
    } else {
        return CPU_RANGE2_START + (thread_id - 24);
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
/*void set_cpu_affinity(int thread_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(thread_id % NUM_CPUS, &cpuset);
    pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}
*/


void acquire_hierarchical_locks(lock_group_t* hierarchy, int start_level, int depth, uint64_t* random_state) {
    if (depth <= 0 || start_level >= HIERARCHY_LEVELS || !keep_running) return;
    
    int lock_idx = next_random(random_state) % (hierarchy[start_level].num_locks);
    
    if (lock_idx < 0 || lock_idx >= hierarchy[start_level].num_locks) {
        fprintf(stderr, "Invalid lock index\n");
        return;
    }
    
    pthread_mutex_lock(&hierarchy[start_level].locks[lock_idx]);
    
    if (depth > 1 && start_level + 1 < HIERARCHY_LEVELS) {
        acquire_hierarchical_locks(hierarchy, start_level + 1, depth - 1, random_state);
    }
    
    do_work(1000);
    pthread_mutex_unlock(&hierarchy[start_level].locks[lock_idx]);
}

void* worker_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    uint64_t local_ops = 0;
    uint64_t random_state[4];
    
    if (!args || args->thread_id < 0 || args->thread_id >= MAX_THREADS) {
        fprintf(stderr, "Invalid thread arguments\n");
        return NULL;
    }
    
    args->nesting_depth = (args->nesting_depth > 0 && args->nesting_depth <= MAX_NESTING_DEPTH) 
                          ? args->nesting_depth : 1;
    args->work_amount = (args->work_amount > 0 && args->work_amount <= MAX_WORK_AMOUNT) 
                        ? args->work_amount : 1000;
    
    set_cpu_affinity(args->thread_id);
    init_random_state(random_state, args->thread_seed);
    
    int iterations = args->is_warmup ? WARMUP_ITERATIONS : NUM_ITERATIONS;
    
    for(int i = 0; i < iterations && keep_running; i++) {
        int start_level = next_random(random_state) % (HIERARCHY_LEVELS - args->nesting_depth + 1);
        acquire_hierarchical_locks(args->lock_hierarchy, start_level, args->nesting_depth, random_state);
        do_work(args->work_amount);
        local_ops++;
    }
    
    if (!args->is_warmup) {
        args->ops_completed[args->thread_id] = local_ops;
    }
    return NULL;
}

void run_warmup(int num_threads, int nesting_depth, int work_amount) {
    pthread_t threads[MAX_THREADS];
    thread_args_t thread_args[MAX_THREADS];
    uint64_t base_seed = 12345;
    
    for(int i = 0; i < num_threads; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].num_threads = num_threads;
        thread_args[i].nesting_depth = nesting_depth;
        thread_args[i].work_amount = work_amount;
        thread_args[i].lock_hierarchy = lock_hierarchy;
        thread_args[i].is_warmup = 1;
        thread_args[i].thread_seed = base_seed + i;
        
        if (pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]) != 0) {
            perror("Thread creation failed");
            exit(1);
        }
    }
    
    for(int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
}

void run_benchmark(int num_threads, int nesting_depth, int work_amount) {
    pthread_t threads[MAX_THREADS];
    thread_args_t thread_args[MAX_THREADS];
    uint64_t ops_completed[MAX_THREADS] = {0};
    uint64_t base_seed = 67890;
    
    if (init_lock_hierarchy() != 0) {
        fprintf(stderr, "Lock hierarchy initialization failed\n");
        return;
    }
    
    run_warmup(num_threads, nesting_depth, work_amount);
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for(int i = 0; i < num_threads; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].num_threads = num_threads;
        thread_args[i].nesting_depth = nesting_depth;
        thread_args[i].work_amount = work_amount;
        thread_args[i].lock_hierarchy = lock_hierarchy;
        thread_args[i].ops_completed = ops_completed;
        thread_args[i].is_warmup = 0;
        thread_args[i].thread_seed = base_seed + i;
        
        if (pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]) != 0) {
            perror("Thread creation failed");
            exit(1);
        }
    }
    
    total_operations = 0;
    for(int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_operations += ops_completed[i];
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("%d,%d,%d,%.2f,%.2f\n", num_threads, nesting_depth, work_amount, 
           duration, total_operations / duration);
    
    cleanup_lock_hierarchy();
}

int main(int argc, char* argv[]) {
    if(argc != 4) {
        printf("Usage: %s <num_threads> <nesting_depth> <work_amount>\n", argv[0]);
        exit(1);
    }
    
    int thread_counts = atoi(argv[1]);
    int nesting_depths = atoi(argv[2]);
    int work_amounts = atoi(argv[3]);
    
    if (thread_counts <= 0 || thread_counts > MAX_THREADS ||
        nesting_depths <= 0 || nesting_depths > MAX_NESTING_DEPTH ||
        work_amounts < 0 || work_amounts > MAX_WORK_AMOUNT) {
        fprintf(stderr, "Error: Invalid arguments\n");
        exit(1);
    }

    run_benchmark(thread_counts, nesting_depths, work_amounts);
    
    return 0;
}




/*
// Acquire locks following hierarchy
void acquire_hierarchical_locks(lock_group_t* hierarchy, int start_level, int depth, uint64_t* random_state) {
    if (depth <= 0 || !keep_running) return;
    
    // Reduce random range to increase lock contention
    int lock_idx = next_random(random_state) % (hierarchy[start_level].num_locks / 4);
    
    pthread_mutex_lock(&hierarchy[start_level].locks[lock_idx]);
    
    // More aggressive nesting
    if (depth > 1 && start_level + 1 < HIERARCHY_LEVELS) {
        acquire_hierarchical_locks(hierarchy, start_level + 1, depth - 1, random_state);
    }
    
    // Increased simulated work to create longer lock hold times
    do_work(5000);  
    pthread_mutex_unlock(&hierarchy[start_level].locks[lock_idx]);
}

void* worker_thread(void* arg) {
    thread_args_t* args = (thread_args_t*)arg;
    uint64_t local_ops = 0;
    uint64_t random_state[4];
    
    set_cpu_affinity(args->thread_id);
    init_random_state(random_state, args->thread_seed);
    
    int iterations = args->is_warmup ? WARMUP_ITERATIONS : NUM_ITERATIONS;
    
    for(int i = 0; i < iterations && keep_running; i++) {
        // More aggressive lock selection strategy
        int start_level = next_random(random_state) % (HIERARCHY_LEVELS - args->nesting_depth + 1);
        
        // Simulate more complex locking patterns
        if (i % 10 == 0) {  // Occasionally increase nesting depth
            args->nesting_depth = (args->nesting_depth % 5) + 1;
        }
        
        acquire_hierarchical_locks(args->lock_hierarchy, start_level, args->nesting_depth, random_state);
        do_work(args->work_amount * 2);  // Increased work
        local_ops++;
    }
    
    if (!args->is_warmup) {
        args->ops_completed[args->thread_id] = local_ops;
    }
    return NULL;
}

void run_warmup(int num_threads, int nesting_depth, int work_amount) {
    pthread_t* threads = malloc(sizeof(pthread_t) * num_threads);
    thread_args_t* thread_args = malloc(sizeof(thread_args_t) * num_threads);
    uint64_t base_seed = 12345;
    
    //printf("Starting warmup phase...\n");
    
    for(int i = 0; i < num_threads; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].num_threads = num_threads;
        thread_args[i].nesting_depth = nesting_depth;
        thread_args[i].work_amount = work_amount;
        thread_args[i].lock_hierarchy = lock_hierarchy;
        thread_args[i].is_warmup = 1;
        thread_args[i].thread_seed = base_seed + i;
        
        pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]);
    }
    
    for(int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    //printf("Warmup completed.\n");
    
    free(threads);
    free(thread_args);
}

void run_benchmark(int num_threads, int nesting_depth, int work_amount) {
    pthread_t* threads = malloc(sizeof(pthread_t) * num_threads);
    thread_args_t* thread_args = malloc(sizeof(thread_args_t) * num_threads);
    uint64_t* ops_completed = calloc(num_threads, sizeof(uint64_t));
    uint64_t base_seed = 67890;
    
    init_lock_hierarchy();
    run_warmup(num_threads, nesting_depth, work_amount);
    
    //printf("\nStarting main benchmark...\n");
    
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for(int i = 0; i < num_threads; i++) {
        thread_args[i].thread_id = i;
        thread_args[i].num_threads = num_threads;
        thread_args[i].nesting_depth = nesting_depth;
        thread_args[i].work_amount = work_amount;
        thread_args[i].lock_hierarchy = lock_hierarchy;
        thread_args[i].ops_completed = ops_completed;
        thread_args[i].is_warmup = 0;
        thread_args[i].thread_seed = base_seed + i;
        
        pthread_create(&threads[i], NULL, worker_thread, &thread_args[i]);
    }
    
    for(int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
        total_operations += ops_completed[i];
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    double duration = (end.tv_sec - start.tv_sec) + 
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("\nBenchmark Results:\n");
    printf("Configuration:\n");
    printf("- Total Locks: %d\n", TOTAL_LOCKS);
    printf("- Hierarchy Levels: %d\n", HIERARCHY_LEVELS);
    printf("- Locks per Level: %d\n", LOCKS_PER_GROUP);
    printf("- CPU Cores Used: %d\n", NUM_CPUS);
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
    
    printf("%d,%d,%d,%.2f,%.2f\n",num_threads, nesting_depth, work_amount,duration,total_operations / duration);
    cleanup_lock_hierarchy();
    free(threads);
    free(thread_args);
    free(ops_completed);
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
*/
