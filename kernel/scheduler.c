#include "scheduler.h"
#include "thread.h"

static int current_process = -1;
static scheduler_context_t current_context = SCHED_CONTEXT_PROCESS;

void scheduler_init(void)
{
    current_process = -1;
    current_context = SCHED_CONTEXT_PROCESS;
    thread_init();
}

uint32_t *scheduler_tick(uint32_t *current_esp)
{
    /*
     * If the current context is a thread, save the thread's
     * stack instead of treating it as a process stack.
     */
    if (current_context == SCHED_CONTEXT_THREAD) {
        int pid = scheduler_current_pid();

        if (thread_has_owner(pid)) {
            uint32_t *thread_esp =
                thread_scheduler_tick(current_esp, pid);

            /*
             * Give the process scheduler the next turn.
             * The returned thread stack will be used directly.
             */
            if (thread_esp != current_esp) {
                current_context = SCHED_CONTEXT_THREAD;
                return thread_esp;
            }
        }

        current_context = SCHED_CONTEXT_PROCESS;
    }

    /*
     * Save the current process context.
     */
    if (current_process >= 0) {
        pcb_t *current = get_process_by_index(current_process);

        if (current != 0 &&
            current->state == PROCESS_RUNNING) {
            current->stack_pointer = current_esp;
            current->state = PROCESS_READY;
        }
    }

    /*
     * Find the next READY process.
     */
    int start = current_process;

    for (int i = 1; i <= MAX_PROCESSES; i++) {
        int index = (start + i) % MAX_PROCESSES;
        pcb_t *process = get_process_by_index(index);

        if (process != 0 &&
            process->state == PROCESS_READY) {

            current_process = index;
            process->state = PROCESS_RUNNING;

            /*
             * If this process owns a thread, run its thread.
             */
            if (thread_has_owner((int)process->pid)) {
                uint32_t *thread_esp =
                    thread_scheduler_tick(process->stack_pointer,
                                           (int)process->pid);

                if (thread_esp != process->stack_pointer) {
                    current_context = SCHED_CONTEXT_THREAD;
                    return thread_esp;
                }
            }

            current_context = SCHED_CONTEXT_PROCESS;
            return process->stack_pointer;
        }
    }

    return current_esp;
}

uint32_t *scheduler_start(void)
{
    current_process = -1;
    current_context = SCHED_CONTEXT_PROCESS;

    return scheduler_tick(0);
}

int scheduler_current_pid(void)
{
    if (current_process < 0) {
        return -1;
    }

    pcb_t *process = get_process_by_index(current_process);

    if (process == 0) {
        return -1;
    }

    return (int)process->pid;
}

int scheduler_current_thread(void)
{
    return thread_current_index();
}

scheduler_context_t scheduler_current_context(void)
{
    return current_context;
}
