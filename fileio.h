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
#ifndef __FILEIO_H__
#define __FILEIO_H__

#include "config.h"
#include "z80fs.h"

#define NAME_SIZE (Z80FS_NAME_SIZE + 1)

//! describes a directory while iterating
typedef struct {
    block_t dir_block;                    //!< block number of the directory entry
    block_t current_block;                //!< block number of the data data in block_data
    uint16_t *current_entry;              //!< current iteration pointer
    uint8_t block_data[BYTE_BLOCK_SIZE];  //!< current block data
} dir_iterator_t;

#define MODE_READ 0x00   //!< file is opened for reading
#define MODE_WRITE 0x01  //!< file is opened for writing

//! describes an open file
typedef struct {
    block_t file_block;                   //!< block number of the file entry
    block_t current_block;                //!< block number of the data in block_data
    uint16_t size;                        //!< remaining bytes when reading, written bytes when writing
    uint8_t *data_pointer;                //!< current read/write pointer
    uint8_t block_data[BYTE_BLOCK_SIZE];  //!< current block data
    uint8_t mode;                         //!< file access mode
} file_t;

#define ENTRY_FILE Z80FS_FNODE_FILE      //!< entry type file
#define ENTRY_DIR Z80FS_FNODE_DIRECTORY  //!< entry type directory

//! returned when iterating over a directory
typedef struct {
    uint8_t type;          // type of the entry
    char name[NAME_SIZE];  // NULL terminated name of the entry
    uint16_t size;         // the size of the file if type==ENTRY_FILE
} dir_entry_t;

//! error return values for the functions below
typedef enum fserror {
    Z80FS_OK = 0x00,           // all OK
    Z80FS_E_IO = -0x01,        // IO error
    Z80FS_E_NODIR = -0x02,     // not a directory
    Z80FS_E_NOTFOUND = -0x03,  // name not found
    Z80FS_E_FSBROKEN = -0x04,  // structural error in FS
    Z80FS_E_EXISTS = -0x05,    // name already exists
    Z80FS_E_FULL = -0x06,      // no free blocks
    Z80FS_E_INVALID = -0x07,   // invalid function
    Z80FS_E_EOF = -0x08,       // end of file
    Z80FS_E_NAME = -0x09,      // invalid file name
    Z80FS_E_NOFILE = -0x0A,    // not a file
    Z80FS_E_NOTEMPTY = -0x0B   // dir is not empty
} error_t;

extern error_t fs_change_dir(char *name);

extern error_t fs_iterate_dir(dir_iterator_t *iterator);
extern error_t fs_next_entry(dir_iterator_t *iterator, dir_entry_t *entry);

extern error_t fs_create_dir(char *name);

extern error_t fs_rename(char *old_name, char *new_name);

extern error_t fs_delete(char *name);

extern error_t fs_open(file_t *, char *name);
extern error_t fs_read_byte(file_t *f, uint8_t *buf);
extern int16_t fs_read(file_t *f, uint8_t *buf, int16_t size);

extern error_t fs_create(file_t *, char *name);
extern error_t fs_write_byte(file_t *f, uint8_t b);
extern error_t fs_write(file_t *f, uint8_t *buf, uint16_t size);
extern error_t fs_close(file_t *f);

extern error_t fs_format(char *name);

#endif /* __FILEIO_H__ */
