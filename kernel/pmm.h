#ifndef PMM_H
#define PMM_H

#include "../include/types.h"

#define PMM_PAGE_SIZE 4096

void pmm_init(void);
uint32_t pmm_alloc_frame(void);
void pmm_free_frame(uint32_t paddr);

uint32_t pmm_get_total_frames(void);
uint32_t pmm_get_used_frames(void);
uint32_t pmm_get_free_frames(void);

#endif
