#include "thread.h"
#include "scheduler.h"

static thread_t thread_table[MAX_THREADS];
static uint8_t thread_stacks[MAX_THREADS][THREAD_STACK_SIZE];

static uint32_t next_tid = 1;
static int current_thread = -1;

void thread_entry_wrapper(void)
{
    if (current_thread < 0 ||
        current_thread >= MAX_THREADS) {
        return;
    }

    thread_t *thread = &thread_table[current_thread];

    thread->state = THREAD_RUNNING;

    if (thread->entry_point != 0) {
        thread->entry_point(thread->arg);
    }

    thread->state = THREAD_TERMINATED;

    while (true) {
        __asm__ __volatile__("hlt");
    }
}
void thread_init(void)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_table[i].tid = 0;
        thread_table[i].state = THREAD_UNUSED;
        thread_table[i].stack_pointer = 0;
        thread_table[i].entry_point = 0;
        thread_table[i].arg = 0;
        thread_table[i].owner_pid = -1;
    }

    next_tid = 1;
    current_thread = -1;
}
uint32_t *thread_scheduler_tick(uint32_t *current_esp, int owner_pid)
{
    if (current_thread >= 0) {
        thread_t *current = get_thread_by_index(current_thread);

        if (current != 0) {
            current->stack_pointer = current_esp;

            if (current->state == THREAD_RUNNING) {
                current->state = THREAD_READY;
            }
        }
    }

    int start = current_thread;

    for (int i = 1; i <= MAX_THREADS; i++) {
        int index = (start + i) % MAX_THREADS;
        thread_t *thread = get_thread_by_index(index);

        if (thread != 0 &&
            thread->state == THREAD_READY &&
            thread->owner_pid == owner_pid) {

            current_thread = index;
            thread->state = THREAD_RUNNING;

            return thread->stack_pointer;
        }
    }

    return current_esp;
}

int thread_create(void (*entry_fn)(void *), void *arg, int owner_pid)
{
    if (entry_fn == 0) {
        return -1;
    }

    for (int i = 0; i < MAX_THREADS; i++) {

        if (thread_table[i].state == THREAD_UNUSED) {

            thread_table[i].tid = next_tid++;
            thread_table[i].state = THREAD_READY;
            thread_table[i].entry_point = entry_fn;
            thread_table[i].arg = arg;
            thread_table[i].owner_pid = owner_pid;

            /*
             * Start at the top of the 4 KB thread stack.
             */
            uint32_t *sp =
                (uint32_t *)&thread_stacks[i][THREAD_STACK_SIZE];

            /*
             * Fake interrupt-return frame.
             *
             * iretd expects:
             *   EIP
             *   CS
             *   EFLAGS
             */
            *--sp = 0x202; /* EFLAGS */
            *--sp = 0x08;  /* CS */

            /*
             * The context-switch mechanism will eventually
             * enter the thread through a wrapper.
             *
             * For now, use the thread entry point directly.
             */
            *--sp = (uint32_t)thread_entry_wrapper; /* EIP */

            /*
             * Fake PUSHAD frame.
             */
            *--sp = 0; /* EAX */
            *--sp = 0; /* ECX */
            *--sp = 0; /* EDX */
            *--sp = 0; /* EBX */
            *--sp = 0; /* Original ESP */
            *--sp = 0; /* EBP */
            *--sp = 0; /* ESI */
            *--sp = 0; /* EDI */

            thread_table[i].stack_pointer = sp;

            return (int)thread_table[i].tid;
        }
    }

    return -1;
}

thread_t *get_thread(int tid)
{
    for (int i = 0; i < MAX_THREADS; i++) {

        if (thread_table[i].tid == (uint32_t)tid &&
            thread_table[i].state != THREAD_UNUSED) {

            return &thread_table[i];
        }
    }

    return 0;
}

thread_t *get_thread_by_index(int index)
{
    if (index < 0 || index >= MAX_THREADS) {
        return 0;
    }

    return &thread_table[index];
}

int thread_current_index(void)
{
    return current_thread;
}

int thread_has_owner(int owner_pid)
{
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].state != THREAD_UNUSED &&
            thread_table[i].owner_pid == owner_pid) {
            return 1;
        }
    }

    return 0;
}

