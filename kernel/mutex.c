#include "mutex.h"
#include "thread.h"

void mutex_init(mutex_t *mutex)
{
    mutex->locked = 0;
    mutex->owner_tid = -1;
}

void mutex_lock(mutex_t *mutex)
{
    int tid = thread_current_index();

    while (mutex->locked && mutex->owner_tid != tid) {
        __asm__ __volatile__("hlt");
    }

    if (!mutex->locked) {
        mutex->locked = 1;
        mutex->owner_tid = tid;
    }
}

void mutex_unlock(mutex_t *mutex)
{
    int tid = thread_current_index();

    if (mutex->locked && mutex->owner_tid == tid) {
        mutex->locked = 0;
        mutex->owner_tid = -1;
    }
}
