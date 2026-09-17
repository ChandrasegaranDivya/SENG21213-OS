#include "semaphore.h"
#include "thread.h"

static void wake_thread(int index)
{
    thread_t *thread = get_thread_by_index(index);

    if (thread != 0 && thread->state == THREAD_BLOCKED) {
        thread->state = THREAD_READY;
    }
}

void sem_init(semaphore_t *sem, int value)
{
    sem->count = value;
    sem->wait_head = 0;
    sem->wait_tail = 0;

    for (int i = 0; i < SEM_MAX_WAITERS; i++) {
        sem->waiters[i] = -1;
    }
}

void sem_wait(semaphore_t *sem)
{
    int tid = thread_current_index();

    while (sem->count <= 0) {

        int already_waiting = 0;

        for (int i = 0; i < SEM_MAX_WAITERS; i++) {
            if (sem->waiters[i] == tid) {
                already_waiting = 1;
                break;
            }
        }

        if (!already_waiting &&
            sem->wait_tail - sem->wait_head < SEM_MAX_WAITERS) {

            int index = sem->wait_tail % SEM_MAX_WAITERS;
            sem->waiters[index] = tid;
            sem->wait_tail++;
        }

        thread_t *current = get_thread_by_index(tid);

        if (current != 0) {
            current->state = THREAD_BLOCKED;
        }

        /*
         * Wait for the timer interrupt.
         * IRQ0 will call the scheduler and select
         * another READY thread.
         */
        __asm__ __volatile__("hlt");
    }

    sem->count--;
}

void sem_signal(semaphore_t *sem)
{
    /*
     * If a thread is waiting, wake one waiter.
     * Otherwise simply increase the semaphore count.
     */
    if (sem->wait_head < sem->wait_tail) {

        int index = sem->wait_head % SEM_MAX_WAITERS;
        int tid = sem->waiters[index];

        sem->waiters[index] = -1;
        sem->wait_head++;

        if (tid >= 0) {
            wake_thread(tid);
            return;
        }
    }

    sem->count++;
}
