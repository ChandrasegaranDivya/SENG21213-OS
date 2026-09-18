#ifndef FS_H
#define FS_H

#include "../include/types.h"
#include "ramdisk.h"

#define FS_MAGIC 0x53454E47
#define FS_BLOCK_SIZE RAMDISK_BLOCK_SIZE
#define FS_TOTAL_BLOCKS RAMDISK_BLOCK_COUNT

#define FS_MAX_INODES 64
#define FS_MAX_FILENAME 28
#define FS_DIRECT_BLOCKS 8

#define FS_SUPERBLOCK_BLOCK 0
#define FS_DIRECTORY_BLOCK 1
#define FS_BLOCK_BITMAP_BLOCK 2
#define FS_INODE_BITMAP_BLOCK 3
#define FS_INODE_TABLE_BLOCK 4
#define FS_DATA_START_BLOCK 5

typedef struct {
    uint32_t magic;
    uint32_t block_count;
    uint32_t inode_count;
    uint32_t block_bitmap_block;
    uint32_t inode_bitmap_block;
    uint32_t inode_table_block;
    uint32_t directory_block;
    uint32_t data_start_block;
} superblock_t;

typedef struct {
    uint32_t size;
    uint32_t direct[FS_DIRECT_BLOCKS];
} inode_t;

typedef struct {
    char name[FS_MAX_FILENAME];
    uint32_t inode;
} dir_entry_t;

void fs_init(void);

int fs_open(const char *name);
bool_t fs_close(int fd);

int fs_read(int fd, void *buffer, uint32_t size, uint32_t offset);
int fs_write(int fd, const void *data, uint32_t size, uint32_t offset);
uint32_t fs_size(int fd);

bool_t fs_unlink(const char *name);

bool_t fs_create(const char *name);
bool_t fs_exists(const char *name);

void fs_list(void);

#endif
