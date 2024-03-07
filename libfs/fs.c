#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC 0xFFFF 

__attribute__((packed))
struct SuperBlock {
    char signature[8];         // File system signature "ECS150FS"
    uint16_t total_blocks;     // Total number of blocks in the virtual disk
    uint16_t root_dir_index;   // Block index of the root directory
    uint16_t data_start_index; // Block index where data blocks start
    uint16_t data_blocks;      // Total number of data blocks
    uint8_t fat_blocks;        // Number of blocks for the FAT
    uint8_t unused[4079];      // Padding to make the superblock size 4096 bytes
};

typedef struct RootDirectory {
    char filename[FS_FILENAME_LEN]; // Filename (including NULL character)
    uint32_t file_size;             // Size of the file in bytes
    uint16_t first_data_block;      // Index of the first data block
    uint8_t unused[10];             // Unused/Padding to align to 32 bytes
} __attribute__((packed));


struct FileDescriptor {
    int used;           // A flag to indicate if this file descriptor is in use
    int root_dir_index; // Index of the file in the root directory
    size_t offset;      // Current offset within the file
};
struct FileDescriptor fd_table[FS_OPEN_MAX_COUNT] = {0};

static struct SuperBlock superblock; 
static struct RootDirectory *root_directory = NULL; 
static uint16_t *fat16 = NULL;       

/* TODO: Phase 1 */

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
}

int fs_umount(void) 
{

    if (!fat16 || !root_directory) {
        return -1;
    }
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++) {
        if (fd_table[i].used) {
            return -1;
        }
    }
    for (int i = 0; i < superblock.fat_blocks; ++i) {
        if (block_write(1 + i, fat16 + (i * BLOCK_SIZE / sizeof(uint16_t))) == -1) {
            return -1;
        }
    }

   if (block_write(superblock.root_dir_index, root_directory) == -1) {
        return -1;
    }    

    free(fat16);
    free(root_directory);
    fat16 = NULL;
    root_directory = NULL;

    if (block_disk_close() == -1) {
        return -1;
    }
    
    return 0;

}

int fs_info(void) 
{
    if (!fat16 || !root_directory) {
        return -1;
    }
    
    printf("FS Info:\n");
    printf("Total number of blocks: %d\n", superblock.total_blocks);
    printf("Data block count: %d\n", superblock.data_blocks);
    printf("FAT block count: %d\n", superblock.fat_blocks);
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
	/* TODO: Phase 2 */
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
	/* TODO: Phase 4 */
}


int get_block_index(const struct RootDirectory *file, size_t offset) {
    uint16_t block = file->first_data_block;
    size_t block_number = offset / BLOCK_SIZE;

    for (size_t i = 0; i < block_number && block != FAT_EOC; ++i) {
        block = fat16[block];
    }

    if (block == FAT_EOC) {
        return -1;  
    } else {
        return block;  
    }
}

size_t min(size_t a, size_t b) {
    if (a < b) {
        return a;
    } else {
        return b;
    }
}

int fs_read(int fd, void *buf, size_t count)
{
    if (!fat16 || !root_directory || !buf) {
        return -1;
    }
    if (fd < 0 || fd >= FS_OPEN_MAX_COUNT || !fd_table[fd].used) {
        return -1;
    }
    int rootDirIndex = fd_table[fd].root_dir_index;
    size_t offset = fd_table[fd].offset;
    size_t file_size = root_directory[rootDirIndex].file_size;

    if (offset >= file_size) {
        return 0; 
    }

    if (offset + count > file_size) {
        count = file_size - offset;
    }

    size_t bytes_read = 0;
    char block_buffer[BLOCK_SIZE];


     while (bytes_read < count) {
        int block_index = get_block_index(&root_directory[rootDirIndex], offset + bytes_read);
        if (block_index == -1) {
            break;  // End of file reached or invalid block
        }

        if (block_read(block_index, block_buffer) == -1) {
            return -1;  // Error reading the block
        }

        size_t block_offset = (offset + bytes_read) % BLOCK_SIZE;
        size_t remaining_in_block = BLOCK_SIZE - block_offset;
        size_t remaining_to_read = count - bytes_read;
        size_t bytes_to_copy = min(remaining_in_block, remaining_to_read);

        memcpy((char *)buf + bytes_read, block_buffer + block_offset, bytes_to_copy);
        bytes_read += bytes_to_copy;
    }

    fd_table[fd].offset += bytes_read;
    return bytes_read;
}

	/* TODO: Phase 4 */
}

