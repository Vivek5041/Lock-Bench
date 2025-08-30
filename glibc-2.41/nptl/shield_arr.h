#include <stdbool.h>
#include <stddef.h>
#define MAX_LOCKS 4

#define DEBUG_P 0
#if DEBUG_P
    #define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(...) ((void)0)
#endif

// Lock status
typedef enum {
    LS_ACQUIRE_NOW,
    LS_SKIP_ACQUISITION,
    LS_RELEASE_NOW,
    LS_SKIP_RELEASE,
    LS_UNBALANCED_LOCK,
    LS_UNBALANCED_UNLOCK
} LS_Status;

// Structure for each array entry
typedef struct {
    void* lock_ptr;
    int rec_count;
} LS_LockEntry;


// Thread-Local Storage
extern __thread LS_LockEntry lock_table[MAX_LOCKS];
extern __thread int lock_count;


// --- TLS Lookup ---
static inline LS_LockEntry* lookup(void* l) {
    DEBUG_PRINT("In lookup\n");
    if (lock_count <= MAX_LOCKS) {
        for (int i = 0; i < lock_count; i++) {
            if (lock_table[i].lock_ptr == l)
                return &lock_table[i];
        }
    }
    return NULL;
}

// --- TLS Increment ---
static inline void IncrementRef(void* l) {
    DEBUG_PRINT("In IncrementRef\n");
    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        if (lock_count <= MAX_LOCKS ) {
            lock_table[lock_count].lock_ptr = l;
            lock_table[lock_count].rec_count = 1;
            lock_count++;
        }
    } else {
        entry->rec_count++;
    }
}

// --- TLS Decrement ---
static inline int DecrementRef(void* l) {
    DEBUG_PRINT("In DecrementRef\n");
    LS_LockEntry* entry = lookup(l);
    if (!entry) return -1;

    if (lock_count < MAX_LOCKS) {
        int val = entry->rec_count;
        if (val > 1) {
            entry->rec_count--;
            return val - 1;
        } else {
            int idx = entry - lock_table;
            lock_table[idx] = lock_table[--lock_count];
            return 0;
        }
    }
    return -1;
}

// Typedef for locking/unlocking function pointer
typedef int (*LockFunc2)(void* l, void* me);
typedef void (*UnlockFunc2)(void* l, void* me);
typedef int (*LockFunc1)(void* l);
typedef void (*UnlockFunc1)(void* l);

// --- Shielding LS Layer ---

static LS_Status __attribute__((unused)) LS_ACQUIRE1(void* l , bool reentrant, LockFunc1 __lock_fn) {
    DEBUG_PRINT("In LS_ACQ_ENT\n");

    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        __lock_fn(l);
        IncrementRef(l);
        return LS_ACQUIRE_NOW;
    }
    if (reentrant){
        IncrementRef(l);
        return LS_SKIP_ACQUISITION;
    }
    return LS_UNBALANCED_LOCK;
}

static LS_Status __attribute__((unused)) LS_ACQUIRE2(void* l, void* me, bool reentrant, LockFunc2 __lock_fn) {
    DEBUG_PRINT("In LS_ACQ_ENT\n");

    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        __lock_fn(l, me);
        IncrementRef(l);
        return LS_ACQUIRE_NOW;
    }
    if (reentrant){
        IncrementRef(l);
        return LS_SKIP_ACQUISITION;
    }
    return LS_UNBALANCED_LOCK;
}


static LS_Status __attribute__((unused)) LS_RELEASE1(void* l,  bool reentrant, UnlockFunc1  __unlock_fn) {
    DEBUG_PRINT("In LS_REL_ENT\n");

    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        return LS_UNBALANCED_UNLOCK;
    }
    if (reentrant) {
        int val = DecrementRef(l);
        if(val == 0) {
            __unlock_fn(l);
            return LS_RELEASE_NOW;
        }
        return LS_SKIP_RELEASE;
    }
    __unlock_fn(l);
    DecrementRef(l);
    return LS_RELEASE_NOW; 
}


static LS_Status __attribute__((unused)) LS_RELEASE2(void* l, void* me, bool reentrant, UnlockFunc2  __unlock_fn) {
    DEBUG_PRINT("In LS_REL_ENT\n");

    LS_LockEntry* entry = lookup(l);
    if (!entry) {
        return LS_UNBALANCED_UNLOCK;
    }
    if (reentrant) {
        int val = DecrementRef(l);
        if(val == 0) {
            __unlock_fn(l,me);
            return LS_RELEASE_NOW;
        }
        return LS_SKIP_RELEASE;
    }
    __unlock_fn(l, me);
    DecrementRef(l);
    return LS_RELEASE_NOW;
}

