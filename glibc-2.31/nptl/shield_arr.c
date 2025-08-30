#include "shield_arr.h"
__thread LS_LockEntry lock_table[MAX_LOCKS];
__thread int lock_count = 0;