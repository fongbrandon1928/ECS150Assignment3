#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

/* TODO: Phase 1 */

uint16_t allocate_new_block(void);
size_t minimum(size_t a, size_t b);
uint16_t get_offset_blk(int fd, size_t offset);
int file_blk_count(uint32_t sz);
static inline int32_t clamp(int32_t val, int32_t min, int32_t max);
void expand_file(int fd, size_t new_size);
void link_new_block_to_file(int root_dir_index, uint16_t new_block);

#define FAT_EOC 0xFFFF

struct SuperBlock
{
    char signature[8];         // File system signature "ECS150FS"
    uint16_t total_blocks;     // Total number of blocks in the virtual disk
    uint16_t root_dir_index;   // Block index of the root directory
    uint16_t data_start_index; // Block index where data blocks start
    uint16_t data_blocks;      // Total number of data blocks
    uint8_t fat_blocks;        // Number of blocks for the FAT
    uint8_t unused[4079];      // Padding to make the superblock size 4096 bytes
} __attribute__((packed));

struct RootDirectory
{
    char filename[FS_FILENAME_LEN]; // Filename (including NULL character)
    uint32_t file_size;             // Size of the file in bytes
    uint16_t first_data_block;      // Index of the first data block
    uint8_t unused[10];             // Unused/Padding to align to 32 bytes
} __attribute__((packed));

struct FileDescriptor
{
    int used;           // A flag to indicate if this file descriptor is in use
    int root_dir_index; // Index of the file in the root directory
    size_t offset;      // Current offset within the file
};

static struct SuperBlock superblock;
static struct RootDirectory *root_directory = NULL;
uint16_t *fat16 = NULL;
struct FileDescriptor fd_table[FS_OPEN_MAX_COUNT] = {0};

int fs_mount(const char *diskname) 
{
    if (block_disk_open(diskname) == -1) {
        return -1;
    }

    if (block_read(0, &superblock) == -1) {
        block_disk_close();
        return -1;
    }

    if (strncmp(superblock.signature, "ECS150FS", 8) != 0) {
        block_disk_close();
        return -1;
    }

    int total_blocks = block_disk_count();
    if (total_blocks == -1 || total_blocks != superblock.total_blocks) {
        block_disk_close();
        return -1;
    }

    fat16 = malloc(superblock.fat_blocks * BLOCK_SIZE);
    if (fat16 == NULL) {
        block_disk_close();
        return -1;
    }

    for (int i = 0; i < superblock.fat_blocks; ++i) {
        if (block_read(1 + i, fat16 + (i * BLOCK_SIZE / sizeof(uint16_t))) == -1) {
            free(fat16);
            block_disk_close();
            return -1;
        }
    }

    root_directory = malloc(sizeof(struct RootDirectory) * FS_FILE_MAX_COUNT);
    if (root_directory == NULL) {
        free(fat16);
        block_disk_close();
        return -1;
    }

    if (block_read(superblock.root_dir_index, root_directory) == -1) {
        free(root_directory);
        free(fat16);
        block_disk_close();
        return -1;
    }
    return 0;
}

int fs_umount(void)
{
    if (!fat16 || !root_directory)
    {
        return -1;
    }

    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (fd_table[i].used == 1) {
            return -1;
        }
    }

    for (int i = 0; i < superblock.fat_blocks; ++i)
    {
        if (block_write(1 + i, fat16 + (i * BLOCK_SIZE / sizeof(uint16_t))) == -1)
        {
            return -1;
        }
    }

    if (block_write(superblock.root_dir_index, root_directory) == -1)
    {
        return -1;
    }

    free(fat16);
    free(root_directory);
    fat16 = NULL;
    root_directory = NULL;

    if (block_disk_close() == -1)
    {
        return -1;
    }

    // If we reach here, the file system has been successfully unmounted
    return 0;
}

int fs_info(void) 
{
    if (!fat16 || !root_directory) {
        return -1;
    }
    
    int fat_free_count = 0;
    for (int i = 0; i < superblock.data_blocks; i++) {
        if (fat16[i] == 0) {
            fat_free_count++;
        }
    }
    
    int free_rdir_entries = 0;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (root_directory[i].filename[0] == '\0') {
            free_rdir_entries++;
        }
    }
    
    printf("FS Info:\n");
    printf("total_blk_count=%d\n", superblock.total_blocks);
    printf("fat_blk_count=%d\n", superblock.fat_blocks);
    printf("rdir_blk=%d\n", superblock.root_dir_index);
    printf("data_blk=%d\n", superblock.data_start_index);
    printf("data_blk_count=%d\n", superblock.data_blocks);
    printf("fat_free_ratio=%d/%d\n", fat_free_count, superblock.data_blocks);
    printf("rdir_free_ratio=%d/%d\n", free_rdir_entries, FS_FILE_MAX_COUNT);
    return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
    
    if (!fat16 || !root_directory) {
        return -1;
    }
    
    if (filename == NULL || strlen(filename) == 0 || strlen(filename) > FS_FILENAME_LEN) {
        return -1;
    }

    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (strcmp(root_directory[i].filename, filename) == 0) {
            return -1;
        }
    }   

    int index = -1;
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i) 
    {
        if (root_directory[i].filename[0] == '\0') 
        {
            index = i;
            break;
        }
    }

    // Check if the root directory is full
    if (index == -1) {
        return -1;
    }

    strncpy(root_directory[index].filename, filename, FS_FILENAME_LEN);
    root_directory[index].file_size = 0;
    root_directory[index].first_data_block = FAT_EOC;

    return 0;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
    if (!fat16 || !root_directory) {
        return -1;
    }

    if (filename == NULL || strlen(filename) == 0 || strlen(filename) > FS_FILENAME_LEN) {
        return -1;
    }
    int index = -1;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (strcmp(root_directory[i].filename, filename) == 0) {
            index = i;
            break;
        }
    }

    if (index == -1){
        return -1;
    }

// cccchhh
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
    if (fd_table[i].used && fd_table[i].root_dir_index == index) {
        return -1;  // File is currently open
    }
    }

    uint16_t block_index = root_directory[index].first_data_block;
    while (block_index != FAT_EOC) {
        uint16_t next_block_index = fat16[block_index];
        fat16[block_index] = 0; 
        block_index = next_block_index;
    }

    memset(&root_directory[index], 0, sizeof(struct RootDirectory));

    return 0;
}

int fs_ls(void)
{
    if (!fat16 || !root_directory)
    {
        return -1;
    }

    // Iterate through root directory entries and print information about each file
    for (int i = 0; i < FS_FILE_MAX_COUNT; ++i)
    {
        if (root_directory[i].filename[0] != '\0')
        { // Check if entry is not empty
            printf("file: %s, size: %d, data_blk: %d\n",
                   root_directory[i].filename,
                   root_directory[i].file_size,
                   root_directory[i].first_data_block);
        }
    }
    return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
    if (!fat16 || !root_directory) {
        return -1;
    }
    if (filename == NULL || strlen(filename) == 0 || strlen(filename) > FS_FILENAME_LEN) {
        return -1;
    }
    int index = -1;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++) {
        if (strcmp(root_directory[i].filename, filename) == 0) {
            index = i;
            break;
        }
    }
    if (index == -1){
        return -1;
    }
    int fd = -1;
    for (int i = 0; i < FS_OPEN_MAX_COUNT; ++i) {
        if (fd_table[i].used == 0) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
            return -1;
    }
fd_table[fd].used = 1;
fd_table[fd].root_dir_index = fd;
fd_table[fd].offset = 0;

return fd;


}

int fs_close(int fd)
{
	/* TODO: Phase 3 */

    if (!fat16 || !root_directory) {
        return -1;
    }



    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT || !fd_table[fd].used) {
        return -1;
    }


    fd_table[fd].used = 0;

    return 0;

}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
    if (!fat16 || !root_directory) {
        return -1;
    }
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT || !fd_table[fd].used) {
        return -1;
    }

    int rootDirIndex = fd_table[fd].root_dir_index;

    return root_directory[rootDirIndex].file_size;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */

    if (!fat16 || !root_directory) {
        return -1;
    }
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT || !fd_table[fd].used) {
        return -1;
    }
    int rootDirIndex = fd_table[fd].root_dir_index;

     if (offset > root_directory[rootDirIndex].file_size) {
            return -1;
     }

    fd_table[fd].offset = offset;

    return 0;

}

int fs_write(int fd, void *buf, size_t count) {
    if (!fat16 || !root_directory || !buf) {
        return -1;
    }
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT || fd_table[fd].used == 0) {
        return -1;
    }

    struct RootDirectory *dir_entry = &root_directory[fd_table[fd].root_dir_index];
    if (count == 0) return 0; // Nothing to write

    size_t offset = fd_table[fd].offset;
    size_t bytes_written = 0;
    void *bounce_buffer = malloc(BLOCK_SIZE);
    if (!bounce_buffer) return -1; // Failed to allocate memory

    while (bytes_written < count) {
        // Calculate the block to write to, based on the current offset
        uint16_t current_block = get_offset_blk(fd, offset + bytes_written);
        if (current_block == FAT_EOC || current_block == 0) {
            // Allocate a new block if necessary
            current_block = allocate_new_block();
            if (current_block == 0) break; // No more space on disk

            // Update FAT to link this new block to the file
            if (offset == dir_entry->file_size) {
                // Link new block at the end of the file
                link_new_block_to_file(fd_table[fd].root_dir_index, current_block);
            }
        }

        size_t block_offset = (offset + bytes_written) % BLOCK_SIZE;
        size_t bytes_to_write = minimum(BLOCK_SIZE - block_offset, count - bytes_written);

        // For partial block writes, read the block first, then modify the necessary parts
        if (block_offset != 0 || bytes_to_write != BLOCK_SIZE) {
            block_read(current_block + superblock.data_start_index, bounce_buffer);
        }
        memcpy(bounce_buffer + block_offset, buf + bytes_written, bytes_to_write);
        block_write(current_block + superblock.data_start_index, bounce_buffer);

        bytes_written += bytes_to_write;
    }

    // Update file size if we've written beyond the current file size
    if (offset + bytes_written > dir_entry->file_size) {
        dir_entry->file_size = offset + bytes_written;
        // This assumes you have a way to write back the updated root directory entry to disk
    }

    // Update the file descriptor's offset
    fd_table[fd].offset += bytes_written;

    free(bounce_buffer);
    return bytes_written;
}

int fs_read(int fd, void *buf, size_t count) {
    if (!fat16 || !root_directory || !buf) {
        return -1;
    }
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT || fd_table[fd].used == 0) {
        return -1;
    }


    struct RootDirectory *dir_entry = &root_directory[fd_table[fd].root_dir_index];
    size_t offset = fd_table[fd].offset;
    int32_t real_count = clamp(dir_entry->file_size - offset, 0, count); // Assuming clamp function limits between 0 and count
    if (real_count == 0) return 0;

    uint16_t read_blk = get_offset_blk(fd, offset);
    if (read_blk == 0) return -1; // Nothing can be read

    void *bounce_buffer = malloc(BLOCK_SIZE);
    memset(bounce_buffer, 0, BLOCK_SIZE);
    int buf_idx = 0;
    block_read(read_blk + superblock.data_start_index, bounce_buffer); // Assuming superblock has data_start_index field
    memcpy(buf + buf_idx, bounce_buffer, clamp(real_count, 0, BLOCK_SIZE));

    buf_idx += clamp(real_count, 0, BLOCK_SIZE);
    real_count -= BLOCK_SIZE; // Remaining bytes to read
    read_blk = fat16[read_blk]; // Get the next block in the file's data block chain

    while (real_count > 0 && read_blk != FAT_EOC) {
        if (real_count >= BLOCK_SIZE) {
            block_read(read_blk + superblock.data_start_index, buf + buf_idx);
            buf_idx += BLOCK_SIZE;
        } else {
            block_read(read_blk + superblock.data_start_index, bounce_buffer);
            memcpy(buf + buf_idx, bounce_buffer, real_count);
            buf_idx += real_count; // This increment is actually unnecessary
        }
        real_count -= BLOCK_SIZE;
        read_blk = fat16[read_blk]; // Advance to the next block
    }

    free(bounce_buffer);
    fd_table[fd].offset += (count - real_count); // Update file offset

    return (count - real_count); // Return the number of bytes actually read
}

uint16_t allocate_new_block(void) {
    // Implementation for finding a free block in the FAT and marking it as used
    for (uint16_t i = 0; i < superblock.data_blocks; i++) {
        if (fat16[i] == 0) { // 0 indicates a free block
            fat16[i] = FAT_EOC; // Mark as end of chain (or use another value to continue the chain)
            return i + superblock.data_start_index; // Return the actual block index on disk
        }
    }
    return 0; // Indicate no free blocks are available
}

size_t minimum(size_t a, size_t b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

uint16_t get_offset_blk(int fd, size_t offset) {
    if (!fat16 || !root_directory) {
        return -1;
    }
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT || fd_table[fd].used == 0) {
        return -1;
    }


    struct RootDirectory *dir_entry = &root_directory[fd_table[fd].root_dir_index];
    if (offset >= dir_entry->file_size) {
        return 0; // Offset is larger than file size
    }

    int block_count = file_blk_count(offset);
    uint16_t current_block = dir_entry->first_data_block;
    for (int i = 1; i < block_count; ++i) { // Navigate to the correct block
        current_block = fat16[current_block];
    }

    return current_block;
}

int file_blk_count(uint32_t sz) {
    if (sz == 0) return 1;
    
    uint32_t blocks = sz / BLOCK_SIZE;
    return (blocks * BLOCK_SIZE < sz) ? (blocks + 1) : blocks;
}

static inline int32_t clamp(int32_t val, int32_t min, int32_t max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

void expand_file(int fd, size_t new_size) {
    struct RootDirectory *dir_entry = &root_directory[fd_table[fd].root_dir_index];
    while (dir_entry->file_size < new_size) {
        uint16_t new_block = allocate_new_block();
        if (new_block == FAT_EOC) break; // No space left, stop expanding
        
        // If the file has no blocks yet, initialize first_data_block
        if (dir_entry->first_data_block == FAT_EOC) {
            dir_entry->first_data_block = new_block;
        } else {
            // Find the last block in the file's chain and link the new block
            uint16_t last_block = dir_entry->first_data_block;
            while (fat16[last_block] != FAT_EOC) {
                last_block = fat16[last_block];
            }
            fat16[last_block] = new_block; // Link the new block
        }
        
        dir_entry->file_size = new_size; // Update the file size
    }
}

void link_new_block_to_file(int root_dir_index, uint16_t new_block) {
    // Find the last block in the file's chain
    uint16_t last_block = root_directory[root_dir_index].first_data_block;
    if (last_block == FAT_EOC) {
        // If the file has no blocks, this new block is the first block
        root_directory[root_dir_index].first_data_block = new_block;
    } else {
        // Otherwise, find the end of the chain and link the new block
        while (fat16[last_block] != FAT_EOC) {
            last_block = fat16[last_block];
        }
        fat16[last_block] = new_block; // Link the new block
    }
    // Mark the new block as the end of the chain
    fat16[new_block] = FAT_EOC;
}
