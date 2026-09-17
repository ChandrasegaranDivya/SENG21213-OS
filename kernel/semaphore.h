#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "../include/types.h"

#define SEM_MAX_WAITERS 16

typedef struct {
    int count;
    int waiters[SEM_MAX_WAITERS];
    int wait_head;
    int wait_tail;
} semaphore_t;

void sem_init(semaphore_t *sem, int value);
void sem_wait(semaphore_t *sem);
void sem_signal(semaphore_t *sem);

#endif
