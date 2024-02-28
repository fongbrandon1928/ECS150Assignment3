#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF 

struct SuperBlock {
    char signature[8];         // File system signature "ECS150FS"
    uint16_t total_blocks;     // Total number of blocks in the virtual disk
    uint16_t root_dir_index;   // Block index of the root directory
    uint16_t data_start_index; // Block index where data blocks start
    uint16_t data_blocks;      // Total number of data blocks
    uint8_t fat_blocks;        // Number of blocks for the FAT
    uint8_t unused[4079];      // Padding to make the superblock size 4096 bytes
} __attribute__((packed));

typedef struct RootDirectory {
    char filename[FS_FILENAME_LEN]; // Filename (including NULL character)
    uint32_t file_size;             // Size of the file in bytes
    uint16_t first_data_block;      // Index of the first data block
    uint8_t unused[10];             // Unused/Padding to align to 32 bytes
} __attribute__((packed));

SuperBlock superblock = NULL;
RootDirectory *root_directory = NULL;
uint16_t *fat16 = NULL;

int fs_mount(const char *diskname) {
    // Attempt to open the disk
    if (block_disk_open(diskname) == -1) {
        return -1;
    }

    // Read the superblock
    if (block_read(0, &superblock) == -1) {
        block_disk_close();
        return -1;
    }

    // Check the file system signature
    if (strncmp(superblock.signature, "ECS150FS", 8) != 0) {
        block_disk_close();
        return -1;
    }
}

int fs_umount(void) 
{
    /* TODO: Phase 1 */
}

int fs_info(void) 
{
    /* TODO: Phase 1 */
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
}

