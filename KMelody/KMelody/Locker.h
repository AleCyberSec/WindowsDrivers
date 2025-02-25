#pragma once

typedef struct _Locker {
    PFAST_MUTEX Lock;
} Locker, *PLocker;

// Function to initialize and acquire the lock
void Locker_Init(Locker* locker, PFAST_MUTEX lock);
// Function to release the lock
void Locker_Release(Locker* locker);