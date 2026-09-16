#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "process.h"
#include "../include/types.h"

void scheduler_init(void);
uint32_t *scheduler_tick(uint32_t *current_esp);
uint32_t *scheduler_start(void);

#endif
