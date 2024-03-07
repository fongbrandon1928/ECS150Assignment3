#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

/* TODO: Phase 1 */

size_t calculate_block_index(size_t offset, uint16_t first_data_block);
uint16_t allocate_new_block(void);
void update_fat_chain(uint16_t first_block, uint16_t new_block);
size_t minimum(size_t a, size_t b);
int find_block_index(int fd, size_t offset);

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

int fs_write(int fd, void *buf, size_t count)
{
    if (!fat16 || !root_directory || !buf) {
        return -1;
    }
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT || fd_table[fd].used == 0) {
        return -1;
    }

    struct RootDirectory *file = &root_directory[fd_table[fd].root_dir_index];
    size_t bytesWritten = 0;
    size_t fileOffset = fd_table[fd].offset;
    char bounceBuffer[BLOCK_SIZE];

    while (bytesWritten < count) {
        size_t blockIndex = calculate_block_index(fileOffset + bytesWritten, file->first_data_block);
        if (blockIndex == FAT_EOC) {
            // Need to allocate a new block if blockIndex is FAT_EOC indicating end of file or no allocation
            blockIndex = allocate_new_block(); 
            if (blockIndex == FAT_EOC) break; // No space left on disk
            // Link the newly allocated block to the file's block chain in the FAT
            update_fat_chain(file->first_data_block, blockIndex);
        }

        size_t blockOffset = (fileOffset + bytesWritten) % BLOCK_SIZE;
        size_t bytesToEndOfBlock = BLOCK_SIZE - blockOffset;
        size_t bytesToWrite = minimum(count - bytesWritten, bytesToEndOfBlock);

    // If writing within a block (not from the start), read the block first to preserve existing data
        if (blockOffset != 0 || bytesToWrite < BLOCK_SIZE) {
            block_read(blockIndex, bounceBuffer);
        }

        // Copy data from buf to bounceBuffer for the current block segment
        memcpy(bounceBuffer + blockOffset, (char*)buf + bytesWritten, bytesToWrite);

        // Write the updated bounce buffer back to the block
        block_write(blockIndex, bounceBuffer);

        bytesWritten += bytesToWrite;
        fd_table[fd].offset += bytesToWrite; // Update the file descriptor's offset

        // Update file size if necessary
        if (fd_table[fd].offset > file->file_size) {
            file->file_size = fd_table[fd].offset;
            // Optionally, write back the updated root directory entry to disk here
        }
    }

    return bytesWritten;
}

int fs_read(int fd, void *buf, size_t count)
{
    if (!fat16 || !root_directory || !buf) {
        return -1;
    }
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT || fd_table[fd].used == 0) {
        return -1;
    }

    int root_dir_index = fd_table[fd].root_dir_index;
    size_t file_size = root_directory[root_dir_index].file_size;
    size_t offset = fd_table[fd].offset;

    // Adjust count if read request exceeds file size from current offset
    if (offset + count > file_size) {
        count = file_size - offset;
    }

    size_t bytes_read = 0;
    char bounce_buffer[BLOCK_SIZE];
    int block_number;

    while (bytes_read < count) {
        block_number = find_block_index(fd, offset + bytes_read);
        if (block_number == FAT_EOC) break; // End of file reached

        size_t block_offset = (offset + bytes_read) % BLOCK_SIZE;
        size_t bytes_to_read = min(BLOCK_SIZE - block_offset, count - bytes_read);

        // Read the entire block into bounce_buffer
        block_read(block_number, bounce_buffer);
        // Copy the needed part into the user buffer
        memcpy((char*)buf + bytes_read, bounce_buffer + block_offset, bytes_to_read);

        bytes_read += bytes_to_read;
    }

    // Update the file descriptor's offset
    fd_table[fd].offset += bytes_read;

    return bytes_read;
}

size_t calculate_block_index(size_t offset, uint16_t first_data_block) {
    size_t blocks_to_skip = offset / BLOCK_SIZE;
    uint16_t current_block = first_data_block;

    for (size_t i = 0; i < blocks_to_skip; ++i) {
        if (current_block == FAT_EOC || fat16[current_block] == 0) {
            return FAT_EOC;
        }
        current_block = fat16[current_block];
    }

    return current_block;
}

int find_block_index(int fd, size_t offset) {
    int block_index = fd_table[fd].root_dir_index;
    size_t current_offset = 0;
    size_t block_number = root_directory[block_index].first_data_block;

    while (current_offset + BLOCK_SIZE <= offset && block_number != FAT_EOC) {
        block_number = fat16[block_number]; // Move to the next block in the chain
        current_offset += BLOCK_SIZE;
    }

    return block_number;
}

uint16_t allocate_new_block() {
    for (uint16_t i = 0; i < superblock.data_blocks; i++) {
        if (fat16[i] == 0) {
            fat16[i] = FAT_EOC;
            return i;
        }
    }
    return FAT_EOC;
}

void update_fat_chain(uint16_t first_block, uint16_t new_block) {
    uint16_t current_block = first_block;

    // Follow the chain until the end
    while (fat16[current_block] != FAT_EOC) {
        current_block = fat16[current_block];
    }

    // Link the new block at the end of the chain
    fat16[current_block] = new_block;
}

size_t minimum(size_t a, size_t b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}
