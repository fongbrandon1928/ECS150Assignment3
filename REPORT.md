# ECS150-FS: Implementing a FAT-Based File System

## Overview

The ECS150-FS project involved creating a simple file system based on the File
Allocation Table (FAT) structure. This filesystem, designed for educational 
purposes, offered insights into how filesystems manage data at a low level, 
emphasizing aspects such as file creation, deletion, reading, and writing. The 
architecture of ECS150-FS included key components: a virtual disk, a superblock,
a FAT, and a root directory. The filesystem was mounted on a virtual disk - a 
binary file emulating a physical disk, logically partitioned into blocks.

## File Operations:

### Mounting and Unmounting
Implementing fs_mount and fs_umount, the system could transition between 
operational and non-operational states. This included reading the superblock and
FAT into memory during mounting and ensuring all data were written back to the 
disk on unmounting.

Fs_mount opened the virtual disk using block_disk_open and read the superblock 
to load filesystem metadata. It involved validating the filesystem signature, 
confirming the total number of blocks, and initializing the File Allocation 
Table (FAT) and root directory. In contrast, fs_umount was responsible for 
safely closing the virtual disk and ensuring all metadata and file data were 
correctly written back to disk.

### File Creation and Deletion
Functions like fs_create and fs_delete handled file lifecycle events. Creating a 
file meant finding an empty entry in the root directory and initializing it, 
while deletion involved removing the entry and freeing associated data blocks.

fs_create: involved locating an empty entry in the root directory and setting up
the initial metadata for a new file. The file size was initialized to zero, and 
the first data block index was set to the End of Chain (FAT_EOC) marker, 
indicating an empty file. In contrast, fs_delete removed a file's entry from the
root directory and freed up the data blocks used by the file in the FAT.

### Reading and Writing Files
The core of the filesystem, fs_read and fs_write, dealt with data transfer 
between the disk and user space. This involved calculating which blocks to 
access and handling edge cases like block boundaries and end of file conditions.

fs_read: This function calculated which block to read based on the file's offset 
and handled scenarios like reading partial blocks and spanning across multiple 
blocks. It ensured that the read operation respected the file's current offset 
and size limitations.

fs_write: Similar to fs_read, but it also extended the file if needed by 
allocating new blocks. It managed the complexities of writing data that spanned 
multiple blocks and handled block boundary cases.

## File Descriptor Management

The system used a table to manage open files, each represented by a file 
descriptor containing information like the current file offset. This allowed for 
operations like opening, closing, reading from, and writing to files via their 
descriptors. A maximum of 32 file descriptors could be open simultaneously. 
fs_open and fs_close managed the opening and closing of files, respectively. 
fs_open identified the file in the root directory and assigned a file 
descriptor, while fs_close marked the descriptor as no longer in use. fs_stat 
and fs_lseek: fs_stat provids the current size of a file, and fs_lseek allows 
for changing the file offset within a file.

## Helper Functions

### allocate_new_block

allocate_new_block searches the file allocation table (FAT) for a free block 
that can be allocated for file storage. Upon finding a free block, it marks this 
block as used (typically by setting its FAT entry to FAT_EOC to indicate it's 
the last block in a chain, or to the index of the next block in an ongoing 
chain) and returns the index of this newly allocated block. This function is 
vital for file writing operations, especially when a file needs to be extended 
beyond its current allocation. It ensures that additional storage is available 
for new data, facilitating file growth and efficient disk space utilization.

### get_offset_blk

get_offset_blk calculates which block within the FAT corresponds to a specific 
offset within a file. Given a file descriptor (fd) and an offset in bytes from 
the beginning of the file, it navigates through the file's chain of blocks in 
the FAT to find the block that contains the byte at the specified offset. This 
involves starting from the file's first data block and following the chain of 
blocks, counting the offset's progression through each block until reaching the 
block that encompasses the desired offset. This function is crucial for both 
reading from and writing to a file at specific positions, as it determines the 
exact block on the disk where the operation should begin or end.

### expand_file

expand_file is tasked with increasing a file's size to a specified 'new_size'. 
It is called when a write operation requires the file to be larger than its 
current size, necessitating the allocation of additional data blocks and 
updating of the file's metadata to reflect its new size. This function operates 
by determining how many new blocks are needed to reach the desired size, 
allocating each new block sequentially, and linking these blocks into the 
file's data block chain. It ensures that each newly allocated block is properly 
initialized and marked in the FAT, and it updates the file's directory entry to 
indicate its new size. This function is essential for supporting file write 
operations that extend beyond the current end of the file, allowing files to 
dynamically grow as data is added to them.

### link_new_block_to_file

link_new_block_to_file integrates a newly allocated block into a file's 
existing data block chain. It accepts the index of the file's entry in the root 
directory ('root_dir_index') and the FAT index of the newly allocated block 
('new_block'). The function first checks if the file currently has any data 
blocks associated with it; if not, it directly assigns the new block as the 
first data block of the file. If the file already has data blocks, the function 
traverses the FAT chain starting from the file's first block until it reaches 
the last block in the chain. Then, it updates the FAT entry of this last block 
to point to the newly allocated block, effectively linking the new block to the 
end of the file's chain of blocks. This operation is crucial for file 
expansion, enabling the file to grow and occupy additional blocks as needed.

## Limitations/CHallenges

The initial implementation of fs_read struggled with correctly calculating 
which block to start reading from based on the file descriptor's offset. The 
problem was compounded when the read operation needed to cross several blocks, 
especially when these blocks were not sequentially located due to fragmentation 
within the FAT. This fragmentation meant that simply incrementing block numbers 
was insufficient; instead, each block's index had to be individually resolved 
by traversing the FAT chain, a process that proved to be error-prone in early 
attempts.

Another challenge we encountered was related to the improper 
implementation of the expand_file function. This function was designed to 
dynamically increase the size of a file by allocating additional data blocks 
and updating the file's metadata accordingly, a crucial operation for 
supporting write operations that extend a file beyond its current size. 
However, the initial implementation of expand_file failed to correctly handle 
the allocation and linkage of new blocks within the file's block chain in the 
File Allocation Table (FAT), leading to several issues.

## Conclusion

The ECS150-FS project, aimed at designing and implementing a FAT-based 
filesystem, has been an insightful journey into the core mechanisms of file 
storage and management within a computer system. This educational endeavor not 
only highlighted the functional aspects of filesystem operations, including 
file creation, deletion, reading, and writing, but also delved into the 
complexities of managing file descriptors and navigating the File Allocation 
Table (FAT) for efficient data retrieval and storage.

The challenges we faced like ensuring the integrity of data during read and 
write operations and dynamically expanding files to accommodate new data, 
underscored the importance of meticulous planning, robust error handling, and 
the careful management of data structures.
