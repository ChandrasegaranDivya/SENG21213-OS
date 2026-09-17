#ifndef MUTEX_H
#define MUTEX_H

typedef struct {
    int locked;
    int owner_tid;
} mutex_t;

void mutex_init(mutex_t *mutex);
void mutex_lock(mutex_t *mutex);
void mutex_unlock(mutex_t *mutex);

#endif
