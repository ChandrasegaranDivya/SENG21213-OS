#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"
#include "../include/types.h"

typedef enum {
    SCHED_CONTEXT_PROCESS = 0,
    SCHED_CONTEXT_THREAD
} scheduler_context_t;

void scheduler_init(void);
uint32_t *scheduler_tick(uint32_t *current_esp);
uint32_t *scheduler_start(void);
int scheduler_current_pid(void);
int scheduler_current_thread(void);
scheduler_context_t scheduler_current_context(void);

#endif
