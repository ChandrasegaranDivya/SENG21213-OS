#ifndef RAMDISK_H
#define RAMDISK_H

#include "../include/types.h"

#define RAMDISK_SIZE (1024 * 1024)
#define RAMDISK_BLOCK_SIZE 4096
#define RAMDISK_BLOCK_COUNT (RAMDISK_SIZE / RAMDISK_BLOCK_SIZE)

void ramdisk_init(void);

bool_t ramdisk_read_block(uint32_t block, void *buffer);
bool_t ramdisk_write_block(uint32_t block, const void *buffer);

#endif
