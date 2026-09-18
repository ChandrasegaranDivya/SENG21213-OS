#include "fs.h"
#include "ramdisk.h"
#include "vga.h"

#define FS_MAX_FD 16
#define FS_INVALID_INODE 0xFFFFFFFF

typedef struct {
    bool_t used;
    uint32_t inode;
} fd_entry_t;

static superblock_t superblock;
static uint8_t block_bitmap[FS_TOTAL_BLOCKS / 8];
static uint8_t inode_bitmap[FS_MAX_INODES / 8];
static inode_t inode_table[FS_MAX_INODES];
static dir_entry_t directory[FS_MAX_INODES];
static fd_entry_t fd_table[FS_MAX_FD];

static int k_strcmp_fs(const char *a, const char *b)
{
    uint32_t i = 0;

    if (a == 0 || b == 0) {
        return -1;
    }

    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return (a[i] < b[i]) ? -1 : 1;
        }
        i++;
    }

    if (a[i] == b[i]) {
        return 0;
    }

    return (a[i] < b[i]) ? -1 : 1;
}

static uint32_t fs_strlen(const char *s)
{
    uint32_t n = 0;

    if (s == 0) {
        return 0;
    }

    while (s[n] != '\0') {
        n++;
    }

    return n;
}

static void bitmap_set(uint8_t *bitmap, uint32_t index)
{
    bitmap[index / 8] |= (uint8_t)(1U << (index % 8));
}

static void bitmap_clear(uint8_t *bitmap, uint32_t index)
{
    bitmap[index / 8] &= (uint8_t)~(1U << (index % 8));
}

static bool_t bitmap_test(const uint8_t *bitmap, uint32_t index)
{
    return (bitmap[index / 8] & (uint8_t)(1U << (index % 8))) != 0;
}

static void fs_sync_metadata(void)
{
    uint8_t block[FS_BLOCK_SIZE];
    uint32_t i;

    for (i = 0; i < FS_BLOCK_SIZE; i++) {
        block[i] = 0;
    }

    for (i = 0; i < sizeof(superblock); i++) {
        block[i] = ((uint8_t *)&superblock)[i];
    }
    ramdisk_write_block(FS_SUPERBLOCK_BLOCK, block);

    for (i = 0; i < FS_BLOCK_SIZE; i++) {
        block[i] = 0;
    }

    for (i = 0; i < sizeof(block_bitmap); i++) {
        block[i] = block_bitmap[i];
    }
    ramdisk_write_block(FS_BLOCK_BITMAP_BLOCK, block);

    for (i = 0; i < FS_BLOCK_SIZE; i++) {
        block[i] = 0;
    }

    for (i = 0; i < sizeof(inode_bitmap); i++) {
        block[i] = inode_bitmap[i];
    }
    ramdisk_write_block(FS_INODE_BITMAP_BLOCK, block);

    for (i = 0; i < FS_BLOCK_SIZE; i++) {
        block[i] = 0;
    }

    for (i = 0; i < sizeof(inode_table); i++) {
        block[i] = ((uint8_t *)inode_table)[i];
        if (i + 1 >= FS_BLOCK_SIZE) {
            break;
        }
    }
    ramdisk_write_block(FS_INODE_TABLE_BLOCK, block);

    for (i = 0; i < FS_BLOCK_SIZE; i++) {
        block[i] = 0;
    }

    for (i = 0; i < sizeof(directory); i++) {
        block[i] = ((uint8_t *)directory)[i];
        if (i + 1 >= FS_BLOCK_SIZE) {
            break;
        }
    }
    ramdisk_write_block(FS_DIRECTORY_BLOCK, block);
}

static int fs_find_inode(const char *name)
{
    uint32_t i;

    if (name == 0) {
        return -1;
    }

    for (i = 0; i < FS_MAX_INODES; i++) {
        if (directory[i].inode != FS_INVALID_INODE &&
            k_strcmp_fs(directory[i].name, name) == 0) {
            return (int)directory[i].inode;
        }
    }

    return -1;
}

static int fs_find_free_inode(void)
{
    uint32_t i;

    for (i = 0; i < FS_MAX_INODES; i++) {
        if (!bitmap_test(inode_bitmap, i)) {
            return (int)i;
        }
    }

    return -1;
}

static int fs_find_free_block(void)
{
    uint32_t i;

    for (i = FS_DATA_START_BLOCK; i < FS_TOTAL_BLOCKS; i++) {
        if (!bitmap_test(block_bitmap, i)) {
            return (int)i;
        }
    }

    return -1;
}

static int fs_find_free_fd(void)
{
    uint32_t i;

    for (i = 0; i < FS_MAX_FD; i++) {
        if (!fd_table[i].used) {
            return (int)i;
        }
    }

    return -1;
}

void fs_init(void)
{
    uint32_t i;
    uint32_t j;

    ramdisk_init();

    superblock.magic = FS_MAGIC;
    superblock.block_count = FS_TOTAL_BLOCKS;
    superblock.inode_count = FS_MAX_INODES;
    superblock.block_bitmap_block = FS_BLOCK_BITMAP_BLOCK;
    superblock.inode_bitmap_block = FS_INODE_BITMAP_BLOCK;
    superblock.inode_table_block = FS_INODE_TABLE_BLOCK;
    superblock.directory_block = FS_DIRECTORY_BLOCK;
    superblock.data_start_block = FS_DATA_START_BLOCK;

    for (i = 0; i < sizeof(block_bitmap); i++) {
        block_bitmap[i] = 0;
    }

    for (i = 0; i < sizeof(inode_bitmap); i++) {
        inode_bitmap[i] = 0;
    }

    for (i = 0; i < FS_MAX_INODES; i++) {
        inode_table[i].size = 0;

        for (j = 0; j < FS_DIRECT_BLOCKS; j++) {
            inode_table[i].direct[j] = 0;
        }

        directory[i].inode = FS_INVALID_INODE;
        directory[i].name[0] = '\0';
    }

    for (i = 0; i < FS_MAX_FD; i++) {
        fd_table[i].used = false;
        fd_table[i].inode = 0;
    }

    bitmap_set(block_bitmap, FS_SUPERBLOCK_BLOCK);
    bitmap_set(block_bitmap, FS_DIRECTORY_BLOCK);
    bitmap_set(block_bitmap, FS_BLOCK_BITMAP_BLOCK);
    bitmap_set(block_bitmap, FS_INODE_BITMAP_BLOCK);
    bitmap_set(block_bitmap, FS_INODE_TABLE_BLOCK);

    fs_sync_metadata();
}

bool_t fs_exists(const char *name)
{
    return fs_find_inode(name) >= 0;
}

bool_t fs_create(const char *name)
{
    int inode;
    uint32_t i;

    if (name == 0 || name[0] == '\0') {
        return false;
    }

    if (fs_strlen(name) >= FS_MAX_FILENAME) {
        return false;
    }

    if (fs_exists(name)) {
        return false;
    }

    inode = fs_find_free_inode();

    if (inode < 0) {
        return false;
    }

    bitmap_set(inode_bitmap, (uint32_t)inode);

    inode_table[inode].size = 0;

    for (i = 0; i < FS_DIRECT_BLOCKS; i++) {
        inode_table[inode].direct[i] = 0;
    }

    directory[inode].inode = (uint32_t)inode;

    for (i = 0; i < FS_MAX_FILENAME; i++) {
        directory[inode].name[i] = '\0';
    }

    for (i = 0; i < FS_MAX_FILENAME - 1 && name[i] != '\0'; i++) {
        directory[inode].name[i] = name[i];
    }

    fs_sync_metadata();

    return true;
}

int fs_open(const char *name)
{
    int inode;
    int fd;

    inode = fs_find_inode(name);

    if (inode < 0) {
        return -1;
    }

    fd = fs_find_free_fd();

    if (fd < 0) {
        return -1;
    }

    fd_table[fd].used = true;
    fd_table[fd].inode = (uint32_t)inode;

    return fd;
}

bool_t fs_close(int fd)
{
    if (fd < 0 || fd >= FS_MAX_FD || !fd_table[fd].used) {
        return false;
    }

    fd_table[fd].used = false;
    fd_table[fd].inode = 0;

    return true;
}

uint32_t fs_size(int fd)
{
    if (fd < 0 || fd >= FS_MAX_FD || !fd_table[fd].used) {
        return 0;
    }

    return inode_table[fd_table[fd].inode].size;
}

int fs_read(int fd, void *buffer, uint32_t size, uint32_t offset)
{
    uint32_t inode_no;
    inode_t *inode;
    uint8_t *dest;
    uint32_t total;
    uint32_t block_index;
    uint32_t block_offset;
    uint8_t block[FS_BLOCK_SIZE];

    if (fd < 0 || fd >= FS_MAX_FD || !fd_table[fd].used ||
        buffer == 0) {
        return -1;
    }

    inode_no = fd_table[fd].inode;
    inode = &inode_table[inode_no];

    if (offset >= inode->size) {
        return 0;
    }

    if (size > inode->size - offset) {
        size = inode->size - offset;
    }

    dest = (uint8_t *)buffer;
    total = 0;

    while (total < size) {
        uint32_t copy_size;
        uint32_t i;

        block_index = (offset + total) / FS_BLOCK_SIZE;
        block_offset = (offset + total) % FS_BLOCK_SIZE;

        if (block_index >= FS_DIRECT_BLOCKS ||
            inode->direct[block_index] == 0) {
            return -1;
        }

        if (!ramdisk_read_block(inode->direct[block_index], block)) {
            return -1;
        }

        copy_size = FS_BLOCK_SIZE - block_offset;

        if (copy_size > size - total) {
            copy_size = size - total;
        }

        for (i = 0; i < copy_size; i++) {
            dest[total + i] = block[block_offset + i];
        }

        total += copy_size;
    }

    return (int)total;
}

int fs_write(int fd, const void *data, uint32_t size, uint32_t offset)
{
    uint32_t inode_no;
    inode_t *inode;
    const uint8_t *src;
    uint32_t total;

    if (fd < 0 || fd >= FS_MAX_FD || !fd_table[fd].used ||
        data == 0) {
        return -1;
    }

    inode_no = fd_table[fd].inode;
    inode = &inode_table[inode_no];

    if (offset > FS_DIRECT_BLOCKS * FS_BLOCK_SIZE) {
        return -1;
    }

    if (size > FS_DIRECT_BLOCKS * FS_BLOCK_SIZE - offset) {
        return -1;
    }

    src = (const uint8_t *)data;
    total = 0;

    while (total < size) {
        uint32_t block_index;
        uint32_t block_offset;
        uint32_t copy_size;
        int new_block;
        uint8_t block[FS_BLOCK_SIZE];
        uint32_t i;

        block_index = (offset + total) / FS_BLOCK_SIZE;
        block_offset = (offset + total) % FS_BLOCK_SIZE;

        if (block_index >= FS_DIRECT_BLOCKS) {
            return -1;
        }

        if (inode->direct[block_index] == 0) {
            new_block = fs_find_free_block();

            if (new_block < 0) {
                return -1;
            }

            inode->direct[block_index] = (uint32_t)new_block;
            bitmap_set(block_bitmap, (uint32_t)new_block);

            for (i = 0; i < FS_BLOCK_SIZE; i++) {
                block[i] = 0;
            }
        } else {
            if (!ramdisk_read_block(inode->direct[block_index], block)) {
                return -1;
            }
        }

        copy_size = FS_BLOCK_SIZE - block_offset;

        if (copy_size > size - total) {
            copy_size = size - total;
        }

        for (i = 0; i < copy_size; i++) {
            block[block_offset + i] = src[total + i];
        }

        if (!ramdisk_write_block(inode->direct[block_index], block)) {
            return -1;
        }

        total += copy_size;
    }

    if (offset + size > inode->size) {
        inode->size = offset + size;
    }

    fs_sync_metadata();

    return (int)total;
}

bool_t fs_unlink(const char *name)
{
    int inode_no;
    uint32_t i;
    uint32_t j;

    inode_no = fs_find_inode(name);

    if (inode_no < 0) {
        return false;
    }

    for (i = 0; i < FS_MAX_FD; i++) {
        if (fd_table[i].used &&
            fd_table[i].inode == (uint32_t)inode_no) {
            fd_table[i].used = false;
            fd_table[i].inode = 0;
        }
    }

    for (j = 0; j < FS_DIRECT_BLOCKS; j++) {
        if (inode_table[inode_no].direct[j] != 0) {
            bitmap_clear(block_bitmap, inode_table[inode_no].direct[j]);
            inode_table[inode_no].direct[j] = 0;
        }
    }

    inode_table[inode_no].size = 0;

    bitmap_clear(inode_bitmap, (uint32_t)inode_no);

    directory[inode_no].inode = FS_INVALID_INODE;
    directory[inode_no].name[0] = '\0';

    fs_sync_metadata();

    return true;
}

void fs_list(void)
{
    uint32_t i;

    vga_printf("Files:\n");

    for (i = 0; i < FS_MAX_INODES; i++) {
        if (directory[i].inode != FS_INVALID_INODE) {
            vga_printf("%s  %u bytes\n",
                       directory[i].name,
                       inode_table[directory[i].inode].size);
        }
    }
}
