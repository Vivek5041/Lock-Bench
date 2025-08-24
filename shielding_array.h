#ifndef SHIELDING_ARRAY_H
#define SHIELDING_ARRAY_H

#include <cstdio>
#include <utility>


#define MAX_LOCKS 4
#define MAX_HASH_ENTRIES 10

#define DEBUG_P 0
#if DEBUG_P
    #define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(...) ((void)0)
#endif

// Lock status enum
enum class LS_Status {
    LS_ACQUIRE_NOW = 1,
    LS_SKIP_ACQUISITION,
    LS_UNBALANCED_LOCK,
    LS_RELEASE_NOW,
    LS_SKIP_RELEASE,
    LS_UNBALANCED_UNLOCK,
};

// TLS Entry for array mode
struct LS_LockEntry {
    void* lock_ptr;
    long rec_count;
};

struct LS_LockHashEntry {
    void* lock_ptr;
    int rec_count;
    // UT_hash_handle hh;
    int * dummy[8];
    struct LS_LockHashEntry* next;
    bool dynamically_allocated;
};

thread_local LS_LockEntry lock_table[MAX_LOCKS];
thread_local int lock_count = 0;
thread_local LS_LockHashEntry* lock_hash = NULL;
thread_local LS_LockHashEntry freelist_pool[MAX_HASH_ENTRIES];
thread_local LS_LockHashEntry* freelist_head = NULL;
thread_local bool freelist_initialized = false;

LS_LockEntry* lookup(void* l) {
    if (lock_count <= MAX_LOCKS) {
        for (int i = 0; i < lock_count; ++i) {
            if (lock_table[i].lock_ptr == l)
                return &lock_table[i];
        }
    }
    return nullptr;
}

void IncrementRef(void* l) {
    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        if (lock_count < MAX_LOCKS) {
            lock_table[lock_count].lock_ptr = l;
            lock_table[lock_count].rec_count = 1;
            ++lock_count;
        }
    } else {
        entry->rec_count++;
    }
}

int DecrementRef(void* l) {
    LS_LockEntry* entry = lookup(l);
    if (!entry) return -1;

    if (entry->rec_count > 1) {
        entry->rec_count--;
    } else {
        int idx = static_cast<int>(entry - lock_table);
        lock_table[idx] = lock_table[--lock_count];
        return 0;
    }
    return entry->rec_count;
}


template <typename LockFunc, typename... Args>
LS_Status LS_ACQUIRE(void* l, bool reentrant, LockFunc lock_fn, Args&&... args) { //__attribute__((always_inline))
    DEBUG_PRINT("In LS_ACQUIRE\n");
    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        lock_fn(static_cast<pthread_mutex_t*>(l), std::forward<Args>(args)...);
        IncrementRef(l);
        return LS_Status::LS_ACQUIRE_NOW;
    }
    if (reentrant) {
        IncrementRef(l);
        return LS_Status::LS_SKIP_ACQUISITION;
    }
    return LS_Status::LS_UNBALANCED_LOCK;
}

template <typename UnlockFunc, typename... Args>
LS_Status  LS_RELEASE(void* l, bool reentrant, UnlockFunc unlock_fn, Args&&... args) {
    DEBUG_PRINT("In LS_RELEASE\n");
    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        return LS_Status::LS_UNBALANCED_UNLOCK;
    }
    if (reentrant) {
        int val = DecrementRef(l);
        if (val == 0) {
            unlock_fn(static_cast<pthread_mutex_t*>(l), std::forward<Args>(args)...);
            return LS_Status::LS_RELEASE_NOW;
        }
        return LS_Status::LS_SKIP_RELEASE;
    }
    unlock_fn(static_cast<pthread_mutex_t*>(l), std::forward<Args>(args)...);
    DecrementRef(l);
    return LS_Status::LS_RELEASE_NOW;
}

#endif // SHIELDING_ARRAY_H

