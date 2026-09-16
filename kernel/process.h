#ifndef PROCESS_H
#define PROCESS_H

#include "../include/types.h"

#define MAX_PROCESSES 8
#define PROCESS_STACK_SIZE 4096

typedef enum {
    PROCESS_UNUSED = 0,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_TERMINATED
} process_state_t;

typedef struct {
    uint32_t pid;
    process_state_t state;
    uint32_t *stack_pointer;
    void (*entry_point)(void);
} pcb_t;

void process_init(void);
int create_process(void (*entry_fn)(void));
pcb_t *get_process(int pid);
pcb_t *get_process_by_index(int index);
#endif
