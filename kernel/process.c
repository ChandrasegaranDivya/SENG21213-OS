#include "process.h"

static pcb_t process_table[MAX_PROCESSES];
static uint8_t process_stacks[MAX_PROCESSES][PROCESS_STACK_SIZE];

static uint32_t next_pid = 1;

void process_init(void)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].pid = 0;
        process_table[i].state = PROCESS_UNUSED;
        process_table[i].stack_pointer = 0;
        process_table[i].entry_point = 0;
    }

    next_pid = 1;
}

int create_process(void (*entry_fn)(void))
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_UNUSED) {

            process_table[i].pid = next_pid++;
            process_table[i].state = PROCESS_READY;
            process_table[i].entry_point = entry_fn;

            /*
             * Start at the top of the 4 KB process stack.
             */
            uint32_t *sp =
                (uint32_t *)&process_stacks[i][PROCESS_STACK_SIZE];

            /*
             * Fake interrupt-return frame.
             *
             * iretd expects:
             *   EIP
             *   CS
             *   EFLAGS
             */
            *--sp = 0x202;              /* EFLAGS */
            *--sp = 0x08;               /* CS */
            *--sp = (uint32_t)entry_fn; /* EIP */

            /*
             * Fake PUSHAD frame.
             *
             * popa will restore these registers before iretd.
             */
            *--sp = 0; /* EAX */
            *--sp = 0; /* ECX */
            *--sp = 0; /* EDX */
            *--sp = 0; /* EBX */
            *--sp = 0; /* Original ESP */
            *--sp = 0; /* EBP */
            *--sp = 0; /* ESI */
            *--sp = 0; /* EDI */

            process_table[i].stack_pointer = sp;

            return (int)process_table[i].pid;
        }
    }

    return -1;
}

pcb_t *get_process(int pid)
{
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == (uint32_t)pid &&
            process_table[i].state != PROCESS_UNUSED) {
            return &process_table[i];
        }
    }

    return 0;
}

pcb_t *get_process_by_index(int index)
{
    if (index < 0 || index >= MAX_PROCESSES) {
        return 0;
    }

    return &process_table[index];
}
