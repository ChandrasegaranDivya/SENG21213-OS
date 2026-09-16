#include "scheduler.h"

static int current_process = -1;

void scheduler_init(void)
{
    current_process = -1;
}

uint32_t *scheduler_tick(uint32_t *current_esp)
{
    /* Save the current process context */
    if (current_process >= 0) {
        pcb_t *current = get_process_by_index(current_process);

        if (current != 0 &&
            current->state == PROCESS_RUNNING) {
            current->stack_pointer = current_esp;
            current->state = PROCESS_READY;
        }
    }

    /* Find the next READY process */
    int start = current_process;

    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int index = (start + i) % MAX_PROCESSES;
        pcb_t *process = get_process_by_index(index);

        if (process != 0 &&
            process->state == PROCESS_READY) {

            current_process = index;
            process->state = PROCESS_RUNNING;

            return process->stack_pointer;
        }
    }

    /* No other process found */
    return current_esp;
}

uint32_t *scheduler_start(void)
{
    current_process = -1;

    return scheduler_tick(0);
}
