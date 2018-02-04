/*
MIT License

Copyright (c) 2018 Andre Seidelt <superilu@yahoo.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
#ifndef __Z80FS_H__
#define __Z80FS_H__

// all data in the structs is little endian like the z80 CPU

#include <stdbool.h>
#include <stdint.h>

#define Z80FS_VER1 0x01  //!< Z80FS version number

#define Z80FS_BS32 0x01    //!< block size 32byte
#define Z80FS_BS64 0x02    //!< block size 64byte
#define Z80FS_BS128 0x03   //!< block size 128byte
#define Z80FS_BS256 0x04   //!< block size 256byte
#define Z80FS_BS512 0x05   //!< block size 512byte
#define Z80FS_BS1024 0x06  //!< block size 1024byte

#define Z80FS_GET_BLOCKSIZE(x) (1 << (x + 4))  //!< convert block size from superblock into number of bytes

#define Z80FS_FNODE_EMPTY 0xFF      //!< block is empty
#define Z80FS_FNODE_FILE 0x01       //!< block is a file
#define Z80FS_FNODE_FEXTEND 0x02    //!< block is a file extend
#define Z80FS_FNODE_DIRECTORY 0x03  //!< block is a directory
#define Z80FS_FNODE_DIREXTEND 0x04  //!< block is a directory extend

#define Z80FS_SUPERBLOCK 0  //!< block number of the superblock
#define Z80FS_ROOT_DIR 1    //!< block number of the root directory

#define Z80FS_NAME_SIZE 12  //!< max file/directory name length

#define Z80FS_PATH_SEPARATOR '/'   //!< path separator (unused for now)
#define Z80FS_DRIVE_SEPARATOR ':'  //!< drive separator (unused for now)
#define Z80FS_PARENT ".."          //!< name of parent directory entry
#define Z80FS_CURRENT "."          //!< name of current directory entry

#define Z80FS_EMPTY 0  //!< value for 'emoty' or 'unsued' or 'unavailable'

#define Z80FS_ATTR_RO (1 << 0)   //!< file is read-only
#define Z80FS_ATTR_SYS (1 << 1)  //!< file is system file

//! get address of first dir-entry in a directory
#define Z80FS_FIRST_DIR_ENTRY(dir) ((block_t *)(((char *)dir) + sizeof(z80fs_dir_t)))

//! get address of first dir-entry in a directory extend
#define Z80FS_FIRST_EXT_ENTRY(ext) ((block_t *)(((char *)ext) + sizeof(z80fs_extend_t)))

//! get address of first byte in a file
#define Z80FS_FIRST_FILE_BYTE(f) ((uint8_t *)(((char *)f) + sizeof(z80fs_file_t)))

//! get address of first byte in a file-extend
#define Z80FS_FIRST_EXT_BYTE(f) ((uint8_t *)(((char *)f) + sizeof(z80fs_extend_t)))

//!< data type used for block numbers.
typedef uint16_t block_t;

/**
 * this struct describes a filesystem. it is partition table and superblock.
 * it always resides in block 0
 */
typedef struct {
    uint8_t fs_type;             //!< [0x00] file system type, 0x01 for now
    uint8_t blk_size;            //!< [0x01] block size of the medium
    block_t first_block;         //!< [0x02] the first usable block used by the filesystem
    block_t num_blocks;          //!< [0x04] size of the medium in blocks
    char name[Z80FS_NAME_SIZE];  //!< [0x06] name of the filesystem, not NULL terminated if fully used
} z80fs_super_t;

//! a file node
typedef struct {
    uint8_t type;                //!< [0x00] node type, 0x01 for file
    block_t extend;              //!< [0x01] block of the next entry
    char name[Z80FS_NAME_SIZE];  //!< [0x03] name of the file, not NULL terminated if fully used
    uint8_t attributes;          //!< [0x0f] file flags, must be 0x00
    uint16_t size;               //!< [0x10] file size
} z80fs_file_t;                  //!< [0x12] file data starts here

//! a dir node
typedef struct {
    uint8_t type;                //!< [0x00] node type, 0x03 for dir
    block_t extend;              //!< [0x01] block of the next entry
    char name[Z80FS_NAME_SIZE];  //!< [0x03] name of the file, not NULL terminated if fully used
    uint8_t attributes;          //!< [0x0f] directory flags, must be 0x00
    block_t parent;              //!< [0x10] parent directory entry, 0x0000 for root directory
} z80fs_dir_t;                   //!< [0x12] nodes for files in this dir start here

//! a file or directory extend
typedef struct {
    uint8_t type;       //!< [0x00] node type, 0x02 for file-extend, 0x04 for dir-extend
    block_t extend;     //!< [0x01] block of the next entry
    uint8_t reserved1;  //!< [0x02] unused for now, must be 0x00
} z80fs_extend_t;

#endif /* __Z80FS_H__ */
