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

Fs_mount opened the virtual disk using block_disk_open and read the superblock to
load filesystem metadata. It involved validating the filesystem signature, 
confirming the total number of blocks, and initializing the File Allocation Table
(FAT) and root directory. In contrast, fs_umount was responsible for safely 
closing the virtual disk and ensuring all metadata and file data were correctly 
written back to disk.

### File Creation and Deletion
Functions like fs_create and fs_delete handled file lifecycle events. Creating a 
file meant finding an empty entry in the root directory and initializing it, 
while deletion involved removing the entry and freeing associated data blocks.

fs_create: involved locating an empty entry in the root directory and setting up
the initial metadata for a new file. The file size was initialized to zero, and 
the first data block index was set to the End-of-Chain (FAT_EOC) marker, 
indicating an empty file. In contrast, fs_delete removed a file's entry from the
root directory and freed up the data blocks used by the file in the FAT.

### Reading and Writing Files
The core of the filesystem, fs_read and fs_write, dealt with data transfer 
between the disk and user space. This involved calculating which blocks to access
and handling edge cases like block boundaries and end of file conditions.

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
fs_open identified the file in the root directory and assigned a file descriptor,
while fs_close marked the descriptor as no longer in use. fs_stat and fs_lseek: 
fs_stat provided the current size of a file, and fs_lseek allowed for changing 
the file offset within a file.
