#ifndef THREAD_H
#define THREAD_H

#include "../include/types.h"

#define MAX_THREADS 16
#define THREAD_STACK_SIZE 4096

typedef enum {
    THREAD_UNUSED = 0,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_TERMINATED
} thread_state_t;

typedef struct {
    uint32_t tid;
    thread_state_t state;
    uint32_t *stack_pointer;
    void (*entry_point)(void *);
    void *arg;
    int owner_pid;
} thread_t;

void thread_init(void);
void thread_entry_wrapper(void);

uint32_t *thread_scheduler_tick(uint32_t *current_esp, int owner_pid);

int thread_create(void (*entry_fn)(void *), void *arg, int owner_pid);

thread_t *get_thread(int tid);
thread_t *get_thread_by_index(int index);
int thread_current_index(void);
int thread_has_owner(int owner_pid);

#endif
