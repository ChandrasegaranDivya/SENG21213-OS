#include "ramdisk.h"

static uint8_t ramdisk[RAMDISK_SIZE];

void ramdisk_init(void)
{
    uint32_t i;

    for (i = 0; i < RAMDISK_SIZE; i++) {
        ramdisk[i] = 0;
    }
}

bool_t ramdisk_read_block(uint32_t block, void *buffer)
{
    uint32_t i;
    uint8_t *dest = (uint8_t *)buffer;

    if (block >= RAMDISK_BLOCK_COUNT || buffer == 0) {
        return false;
    }

    for (i = 0; i < RAMDISK_BLOCK_SIZE; i++) {
        dest[i] = ramdisk[block * RAMDISK_BLOCK_SIZE + i];
    }

    return true;
}

bool_t ramdisk_write_block(uint32_t block, const void *buffer)
{
    uint32_t i;
    const uint8_t *src = (const uint8_t *)buffer;

    if (block >= RAMDISK_BLOCK_COUNT || buffer == 0) {
        return false;
    }

    for (i = 0; i < RAMDISK_BLOCK_SIZE; i++) {
        ramdisk[block * RAMDISK_BLOCK_SIZE + i] = src[i];
    }

    return true;
}
