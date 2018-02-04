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

#include <string.h>
#include "blockio.h"
#include "config.h"
#include "fileio.h"
#include "z80fs.h"

static block_t fs_last_free_checked = FIRST_BLOCK;
static block_t fs_current_dir = Z80FS_ROOT_DIR;

static error_t fs_fill_dir_entry(dir_entry_t *entry, block_t blockNum);
static block_t fs_find_free_block();
static error_t fs_find_in_dir(block_t blockNum, char *name, block_t *block, block_t *dir_block);
static error_t fs_check_name(block_t block_num, char *name);
static bool fs_name_compare(char *node_name, char *name);
static error_t fs_add_dir_entry(block_t new_entry);
static error_t fs_check_filename(char *name);
static error_t fs_iterate(dir_iterator_t *iterator, block_t dir);

/**
 * change current directory.
 *
 * @return Z80FS_OK if all went well, one of Z80FS_E_xxx for errors.
 */
error_t fs_change_dir(char *name) {
    block_t block;
    uint8_t block_data[BYTE_BLOCK_SIZE];
    z80fs_dir_t *dir = (z80fs_dir_t *)block_data;

    if ((name == NULL) || (strcmp(name, "/") == 0)) {
        // root-directory requested
        fs_current_dir = Z80FS_ROOT_DIR;
        return Z80FS_OK;
    } else if (strcmp(name, Z80FS_CURRENT) == 0) {
        return Z80FS_OK;
    } else if (strcmp(name, Z80FS_PARENT) == 0) {
        // parent directory requested
        if (!readBlock(fs_current_dir, block_data)) {
            return Z80FS_E_IO;
        }

        if (dir->type != Z80FS_FNODE_DIRECTORY) {
            return Z80FS_E_INVALID;
        }
        block = dir->parent;
    } else {
        // named directory requested
        error_t error = fs_find_in_dir(fs_current_dir, name, &block, NULL);

        if (error != Z80FS_OK) {
            return error;
        }
    }

    if (!readBlock(block, block_data)) {
        return Z80FS_E_IO;
    }
    if (dir->type == Z80FS_FNODE_DIRECTORY) {
        fs_current_dir = block;
        return Z80FS_OK;
    } else {
        return Z80FS_E_NODIR;
    }
}

/**
 * init dir_iterator_t to iterate over the current directory.
 *
 * @param iterator pointer to a dir_iterator_t which stores the intermediate information while iterating.
 *
 * @return Z80FS_OK if all went well, one of Z80FS_E_xxx for errors.
 */
error_t fs_iterate_dir(dir_iterator_t *iterator) { return fs_iterate(iterator, fs_current_dir); }

/**
 * init dir_iterator_t to iterate over the given directory.
 *
 * @param iterator pointer to a dir_iterator_t which stores the intermediate information while iterating.
 * @param dir_block the block of the directory to iterate.
 *
 * @return Z80FS_OK if all went well, one of Z80FS_E_xxx for errors.
 */
static error_t fs_iterate(dir_iterator_t *iterator, block_t dir_block) {
    z80fs_dir_t *dir = (z80fs_dir_t *)iterator->block_data;

    memset(iterator, 0, sizeof(dir_iterator_t));

    if (!readBlock(dir_block, iterator->block_data)) {
        return Z80FS_E_IO;
    }

    // sanity check
    if (iterator->block_data[0] != Z80FS_FNODE_DIRECTORY) {
        return Z80FS_E_NODIR;
    }

    iterator->dir_block = dir_block;
    iterator->current_block = dir_block;
    iterator->current_entry = Z80FS_FIRST_DIR_ENTRY(dir);

    return Z80FS_OK;
}

/**
 * get the next entry from the directory in iterator.
 *
 * @param iterator structure initialized by iterate_dir().
 * @param entry space for the entry data.
 *
 * @return Z80FS_OK if all went well, Z80FS_E_NOTFOUND if no more entries or Z80FS_E_xxx for an error.
 */
error_t fs_next_entry(dir_iterator_t *iterator, dir_entry_t *entry) {
    block_t extend;
    block_t *blockEnd = (uint16_t *)&iterator->block_data[BYTE_BLOCK_SIZE];
    z80fs_extend_t *ext = (z80fs_extend_t *)iterator->block_data;

    // sanity check
    if (!iterator->dir_block || ((iterator->block_data[0] != Z80FS_FNODE_DIRECTORY) && (iterator->block_data[0] != Z80FS_FNODE_DIREXTEND))) {
        return Z80FS_E_INVALID;
    }

    while (true) {
        while (iterator->current_entry < blockEnd) {
            if (*iterator->current_entry != Z80FS_EMPTY) {
                error_t error = fs_fill_dir_entry(entry, *iterator->current_entry);
                if (error == Z80FS_OK) {
                    iterator->current_entry++;
                    return Z80FS_OK;
                } else {
                    return error;
                }
            }
            iterator->current_entry++;
        }

        extend = ext->extend;

        if (extend != Z80FS_EMPTY) {
            iterator->current_block = extend;
            if (!readBlock(extend, iterator->block_data)) {
                return Z80FS_E_IO;
            }
            iterator->current_entry = Z80FS_FIRST_EXT_ENTRY(ext);
        } else {
            return Z80FS_E_NOTFOUND;
        }
    }
}

/**
 * create a new directory in the current directory.
 *
 * @param name name of the new directory.
 *
 * @return Z80FS_OK if all went well or Z80FS_E_xxx for an error.
 */
error_t fs_create_dir(char *name) {
    error_t error;
    block_t block;
    uint8_t block_data[BYTE_BLOCK_SIZE];
    z80fs_dir_t *dir = (z80fs_dir_t *)block_data;

    // sanity checks
    if (fs_check_filename(name) != Z80FS_OK) {
        return Z80FS_E_NAME;
    }
    if (!readBlock(fs_current_dir, block_data)) {
        return Z80FS_E_IO;
    }
    if (dir->type != Z80FS_FNODE_DIRECTORY) {
        return Z80FS_E_FSBROKEN;
    }
    error = fs_find_in_dir(fs_current_dir, name, &block, NULL);
    if (error == Z80FS_OK) {
        return Z80FS_E_EXISTS;
    } else if (error != Z80FS_E_NOTFOUND) {
        return error;
    }

    // get space for the first block of the new dir
    uint16_t new_dir = fs_find_free_block();
    if (!new_dir) {
        return Z80FS_E_FULL;
    }

    // create new dir and store it to disk
    memset(block_data, 0, BYTE_BLOCK_SIZE);
    dir->type = Z80FS_FNODE_DIRECTORY;
    dir->extend = Z80FS_EMPTY;
    strncpy(dir->name, name, Z80FS_NAME_SIZE);
    dir->attributes = Z80FS_EMPTY;
    dir->parent = fs_current_dir;
    if (!writeBlock(new_dir, block_data)) {
        clearBlock(new_dir);
        return Z80FS_E_IO;
    }

    // add the new dir-entry to the current directory
    error = fs_add_dir_entry(new_dir);
    if (error != Z80FS_OK) {
        clearBlock(new_dir);
        return error;
    }

    return Z80FS_OK;
}

/**
 * rename file or directory.
 *
 * @param old the old name.
 * @param new_name the new name.
 *
 * @return Z80FS_OK if all went well or Z80FS_E_xxx for an error.
 */
error_t fs_rename(char *old_name, char *new_name) {
    block_t block;
    uint8_t block_data[BYTE_BLOCK_SIZE];
    z80fs_dir_t *dir = (z80fs_dir_t *)block_data;

    // sanity checks
    if (fs_check_filename(old_name) != Z80FS_OK) {
        return Z80FS_E_NAME;
    }
    if (fs_check_filename(new_name) != Z80FS_OK) {
        return Z80FS_E_NAME;
    }

    error_t error = fs_find_in_dir(fs_current_dir, old_name, &block, NULL);
    if (error != Z80FS_OK) {
        return error;
    }
    if (!readBlock(block, block_data)) {
        return Z80FS_E_IO;
    }
    memset(dir->name, 0, Z80FS_NAME_SIZE);
    strncpy(dir->name, new_name, Z80FS_NAME_SIZE);
    if (!writeBlock(block, block_data)) {
        return Z80FS_E_IO;
    }
    return Z80FS_OK;
}

/**
 * delete a file or an empty directory.
 *
 * @param name the name of the file/directory to delete.
 *
 * @return Z80FS_OK if all went well or Z80FS_E_xxx for an error.
 */
error_t fs_delete(char *name) {
    block_t block, dir_block;
    uint8_t block_data[BYTE_BLOCK_SIZE];
    z80fs_extend_t *ext = (z80fs_extend_t *)block_data;
    z80fs_dir_t *dir = (z80fs_dir_t *)block_data;
    block_t *blockEnd = (uint16_t *)&block_data[BYTE_BLOCK_SIZE];
    block_t *entry;

    // sanity checks
    if (fs_check_filename(name) != Z80FS_OK) {
        return Z80FS_E_NAME;
    }

    // find dir entry and load it
    error_t error = fs_find_in_dir(fs_current_dir, name, &block, &dir_block);
    if (error != Z80FS_OK) {
        return error;
    }

    // check if file or empty directory
    dir_iterator_t it;
    dir_entry_t e;
    error = fs_iterate(&it, block);
    if (error == Z80FS_OK) {
        // this is a directory, we need to check for emptyness
        if (fs_next_entry(&it, &e) != Z80FS_E_NOTFOUND) {
            // we found at least one entry
            return Z80FS_E_NOTEMPTY;
        }
    } else if (error != Z80FS_E_NODIR) {
        // something went wrong
        return error;
    }

    // remove entry from directory
    if (!readBlock(dir_block, block_data)) {
        return Z80FS_E_IO;
    }
    entry = Z80FS_FIRST_DIR_ENTRY(block_data);
    while (true) {
        if (*entry == block) {
            *entry = Z80FS_EMPTY;
            if (!writeBlock(dir_block, block_data)) {
                return Z80FS_E_IO;
            }
            break;
        }
        entry++;
        if (entry >= blockEnd) {
            return Z80FS_E_FSBROKEN;
        }
    }

    // iterate over all chained blocks to clear them
    do {
        block_t extend;
        if (!readBlock(block, block_data)) {
            return Z80FS_E_IO;
        }
        extend = ext->extend;
        clearBlock(block);
        block = extend;
    } while (block);

    return Z80FS_OK;
}

/**
 * open existing file for reading.
 *
 * @param f pointer to file structure.
 * @param name name of the file to open.
 *
 * @return Z80FS_OK if all went well or Z80FS_E_xxx for an error.
 */
error_t fs_open(file_t *f, char *name) {
    z80fs_file_t *file = (z80fs_file_t *)f->block_data;

    memset(f, 0, sizeof(file_t));

    // sanity checks
    if (fs_check_filename(name) != Z80FS_OK) {
        return Z80FS_E_NAME;
    }

    // find file entry and load it
    error_t error = fs_find_in_dir(fs_current_dir, name, &f->file_block, NULL);
    if (error != Z80FS_OK) {
        return error;
    }
    if (!readBlock(f->file_block, f->block_data)) {
        return Z80FS_E_IO;
    }

    // check if it is a file
    if (file->type != Z80FS_FNODE_FILE) {
        return Z80FS_E_NOFILE;
    }

    // initialize file structure
    f->mode = MODE_READ;
    f->size = file->size;
    f->current_block = f->file_block;
    f->data_pointer = Z80FS_FIRST_FILE_BYTE(file);

    return Z80FS_OK;
}

/**
 * read a number of bytes from a file.
 *
 * @param f the file object.
 * @param buf memory to store the data.
 * @param size number of bytes to read.
 *
 * @return the number of bytes read or (if negative) one of Z80FS_E_xxx.
 */
int16_t fs_read(file_t *f, uint8_t *buf, int16_t size) {
    // check if the file is opened in read mode
    if (f->mode != MODE_READ) {
        return Z80FS_E_INVALID;
    }
    int16_t num = 0;

    // get as many bytes as there is space in the buffer
    while (size > 0) {
        error_t err = fs_read_byte(f, buf);
        if (err == Z80FS_E_EOF) {
            // EOF => bail out
            break;
        } else if (err != Z80FS_OK) {
            // return other errors
            return err;
        }

        // next byte
        buf++;
        size--;
        num++;
    }

    return num;
}

/**
 * read single byte from file.
 *
 * @param f the file object.
 * @param buf pointer to the memory destination to store the byte.
 *
 * @return Z80FS_OK if the byte was read, Z80FS_E_EOF if there are no more bytes or Z80FS_E_xxx for other errors.
 */
error_t fs_read_byte(file_t *f, uint8_t *buf) {
    // check if the file is opened in read mode
    if (f->mode != MODE_READ) {
        return Z80FS_E_INVALID;
    }

    // check if there is data left to read
    if (f->size == 0) {
        return Z80FS_E_EOF;
    }

    // check if we still are in the boundaries of the current block
    if (f->data_pointer >= &f->block_data[BYTE_BLOCK_SIZE]) {
        z80fs_file_t *file = (z80fs_file_t *)f->block_data;

        // if there is no extend := error
        if (!file->extend) {
            return Z80FS_E_FSBROKEN;
        }

        // read the extend and set as current block
        f->current_block = file->extend;
        if (!readBlock(file->extend, f->block_data)) {
            return Z80FS_E_IO;
        }

        // sanity check
        if (file->type != Z80FS_FNODE_FEXTEND) {
            return Z80FS_E_FSBROKEN;
        }

        // set read-pointer to beginning of new block
        f->data_pointer = Z80FS_FIRST_EXT_BYTE(file);
    }

    // get the current byte, increase pointer and decrease size left
    *buf = *f->data_pointer;
    f->data_pointer++;
    f->size--;

    return Z80FS_OK;
}

/**
 * create a new file for writing.
 *
 * @param f pointer to file structure.
 * @param name name of the file to open.
 *
 * @return Z80FS_OK if all went well or Z80FS_E_xxx for an error.
 */
error_t fs_create(file_t *f, char *name) {
    z80fs_file_t *file = (z80fs_file_t *)f->block_data;

    memset(f, 0, sizeof(file_t));

    // sanity checks
    if (fs_check_filename(name) != Z80FS_OK) {
        return Z80FS_E_NAME;
    }

    error_t error = fs_find_in_dir(fs_current_dir, name, &f->file_block, NULL);
    if (error == Z80FS_OK) {
        return Z80FS_E_EXISTS;
    } else if (error != Z80FS_E_NOTFOUND) {
        return error;
    }
    f->mode = MODE_READ;

    // get space for the first block of the new dir
    f->file_block = fs_find_free_block();
    if (!f->file_block) {
        return Z80FS_E_FULL;
    }

    // create new dir and store it to disk
    memset(f->block_data, 0, BYTE_BLOCK_SIZE);
    file->type = Z80FS_FNODE_FILE;
    file->extend = Z80FS_EMPTY;
    strncpy(file->name, name, Z80FS_NAME_SIZE);
    file->attributes = Z80FS_EMPTY;
    file->size = 0x00;
    if (!writeBlock(f->file_block, f->block_data)) {
        clearBlock(f->file_block);
        return Z80FS_E_IO;
    }

    // add the new dir-entry to the current directory
    error = fs_add_dir_entry(f->file_block);
    if (error != Z80FS_OK) {
        clearBlock(f->file_block);
        return error;
    }

    // initialize file structure
    f->mode = MODE_WRITE;
    f->current_block = f->file_block;
    f->data_pointer = Z80FS_FIRST_FILE_BYTE(file);

    return Z80FS_OK;
}

/**
 * write a number of bytes to a file.
 *
 * @param f the file object.
 * @param buf memory with the data.
 * @param size number of bytes to write.
 *
 * @return Z80FS_OK or one of Z80FS_E_xxx for an error.
 */
error_t fs_write(file_t *f, uint8_t *buf, uint16_t size) {
    // check if the file is opened in write mode
    if (f->mode != MODE_WRITE) {
        return Z80FS_E_INVALID;
    }

    // write as many bytes as there is space in the buffer
    while (size > 0) {
        error_t err = fs_write_byte(f, *buf);
        if (err != Z80FS_OK) {
            // return errors
            return err;
        }

        // next byte
        buf++;
        size--;
    }

    return Z80FS_OK;
}

/**
 * write a single byte to an open file.
 *
 * @param f the file object.
 * @param b the byte to write.
 *
 * @return Z80FS_OK if all went well or Z80FS_E_xxx for an error.
 */
extern error_t fs_write_byte(file_t *f, uint8_t b) {
    z80fs_file_t *file = (z80fs_file_t *)f->block_data;
    z80fs_extend_t *ext = (z80fs_extend_t *)f->block_data;

    // check if the file is opened in write mode
    if (f->mode != MODE_WRITE) {
        return Z80FS_E_INVALID;
    }

    // check if block is full
    if (f->data_pointer >= &f->block_data[BYTE_BLOCK_SIZE]) {
        // find new block
        block_t next = fs_find_free_block();
        if (!next) {
            return Z80FS_E_FULL;
        }

        // store new block number and write current block
        ext->extend = next;
        if (!writeBlock(f->current_block, f->block_data)) {
            return Z80FS_E_IO;
        }

        // clear block data, initialize as file-extend and write it
        memset(f->block_data, 0, BYTE_BLOCK_SIZE);
        ext->type = Z80FS_FNODE_FEXTEND;
        ext->reserved1 = 0;
        ext->extend = 0x00;
        f->current_block = next;
        if (!writeBlock(f->current_block, f->block_data)) {
            return Z80FS_E_IO;
        }

        // set write-pointer to beginning of new block
        f->data_pointer = Z80FS_FIRST_EXT_BYTE(file);
    }

    // store byte and update size
    *f->data_pointer = b;
    f->data_pointer++;
    f->size++;

    return Z80FS_OK;
}

/**
 * close file. Files with MODE_WRITE are flushed to disk.
 *
 * @param f the file object.
 *
 * @return Z80FS_OK if all went well or Z80FS_E_xxx for an error.
 */
error_t fs_close(file_t *f) {
    z80fs_file_t *file = (z80fs_file_t *)f->block_data;

    // we have to flush pending data if this file is opened in write mode
    if (f->mode == MODE_WRITE) {
        // store current block
        if (f->current_block != f->file_block) {
            if (!writeBlock(f->current_block, f->block_data)) {
                return Z80FS_E_IO;
            }

            // read file-node
            if (!readBlock(f->file_block, f->block_data)) {
                return Z80FS_E_IO;
            }
        }
        file->size = f->size;  // update size

        // store file-node
        if (!writeBlock(f->file_block, f->block_data)) {
            return Z80FS_E_IO;
        }
    }
    return Z80FS_OK;
}

/**
 * initialize a new filesystem.
 *
 * @param name the name of the filesystem.
 */
error_t fs_format(char *name) {
    uint8_t block_data[BYTE_BLOCK_SIZE];
    z80fs_super_t *super = (z80fs_super_t *)block_data;
    z80fs_dir_t *dir = (z80fs_dir_t *)block_data;

    if (fs_check_filename(name) != Z80FS_OK) {
        return Z80FS_E_NAME;
    }

    // clear all blocks
    for (int i = FIRST_BLOCK; i < NUM_BLOCKS; i++) {
        clearBlock(i);
    }

    // create superblock
    memset(block_data, 0, BYTE_BLOCK_SIZE);
    super->fs_type = Z80FS_VER1;
    super->blk_size = BLOCK_SIZE;
    super->first_block = 0x01;
    super->num_blocks = NUM_BLOCKS;
    strncpy(super->name, name, Z80FS_NAME_SIZE);
    if (!writeBlock(Z80FS_SUPERBLOCK, block_data)) {
        return Z80FS_E_IO;
    }

    // create root directory
    memset(block_data, 0, BYTE_BLOCK_SIZE);
    dir->type = Z80FS_FNODE_DIRECTORY;
    dir->extend = Z80FS_EMPTY;
    strcpy(dir->name, "/");
    dir->attributes = Z80FS_EMPTY;
    dir->parent = Z80FS_ROOT_DIR;
    if (!writeBlock(Z80FS_ROOT_DIR, block_data)) {
        return Z80FS_E_IO;
    }

    return Z80FS_OK;
}

/**
 * copy the necessary information from the block to the dir_entry_t.
 *
 * @param entry destination entry.
 * @param the block number
 *
 * @return Z80FS_OK if all went well or Z80FS_E_xxx for an error.
 */
static error_t fs_fill_dir_entry(dir_entry_t *entry, block_t blockNum) {
    uint8_t block_data[BYTE_BLOCK_SIZE];
    z80fs_file_t *file = (z80fs_file_t *)block_data;

    if (!readBlock(blockNum, block_data)) {
        return Z80FS_E_IO;
    }
    entry->type = block_data[0];
    strncpy(entry->name, file->name, Z80FS_NAME_SIZE);
    entry->name[Z80FS_NAME_SIZE] = 0x00;
    entry->size = file->size;

    if ((entry->type == Z80FS_FNODE_DIRECTORY) || (entry->type == Z80FS_FNODE_FILE)) {
        return Z80FS_OK;
    } else {
        return Z80FS_E_FSBROKEN;
    }
}

/**
 * find an unused block on the fs.
 *
 * @return block number or Z80FS_EMPTY if none found.
 */
static block_t fs_find_free_block() {
    uint8_t data = 0x00;
    block_t start = fs_last_free_checked;

    do {
        fs_last_free_checked++;
        if (fs_last_free_checked > NUM_BLOCKS) {
            fs_last_free_checked = FIRST_BLOCK;
        }

        if (!getBlockType(fs_last_free_checked, &data)) {
            return Z80FS_EMPTY;
        }

        if (data == Z80FS_FNODE_EMPTY) {
            // Serial.print(F(" found @")); Serial.println(lastFreeChecked);
            return fs_last_free_checked;
        }
    } while (fs_last_free_checked != start);

    return Z80FS_EMPTY;
}

/**
 * find a named entry in the given directory.
 *
 * @param blockNum block of the directory to check
 * @param name the name to search for.
 * @param block the blocknumber for the found entry.
 * @param dir_block the block of the z80fs_dir_t or z80fs_extend_t where the entry was found.
 *
 * @return Z80FS_OK and a block number or one of Z80FS_E_xxx. Z80FS_E_NOTFOUND meaning there was no such entry.
 */
static error_t fs_find_in_dir(block_t blockNum, char *name, block_t *block, block_t *dir_block) {
    while (true) {
        uint8_t block_data[BYTE_BLOCK_SIZE];
        z80fs_dir_t *dir = (z80fs_dir_t *)block_data;
        z80fs_extend_t *ext = (z80fs_extend_t *)block_data;
        block_t *blockEnd = (block_t *)&block_data[BYTE_BLOCK_SIZE];
        block_t *currentEntry;
        block_t extend;

        if (!readBlock(blockNum, block_data)) {
            return Z80FS_E_IO;
        }
        if (block_data[0] == Z80FS_FNODE_DIRECTORY) {
            extend = dir->extend;
            currentEntry = Z80FS_FIRST_DIR_ENTRY(dir);
        } else if (block_data[0] == Z80FS_FNODE_DIREXTEND) {
            extend = ext->extend;
            currentEntry = Z80FS_FIRST_EXT_ENTRY(ext);
        } else {
            return Z80FS_E_FSBROKEN;
        }

        while (currentEntry < blockEnd) {
            if (*currentEntry != Z80FS_EMPTY) {
                error_t name_check = fs_check_name(*currentEntry, name);
                if (name_check == Z80FS_OK) {
                    *block = *currentEntry;
                    if (dir_block) {
                        *dir_block = blockNum;
                    }
                    return Z80FS_OK;
                } else if (name_check != Z80FS_E_NOTFOUND) {
                    return name_check;
                }
            }
            currentEntry++;
        }

        if (extend != Z80FS_EMPTY) {
            blockNum = extend;
        } else {
            return Z80FS_E_NOTFOUND;
        }
    }
}

/**
 * check if the node at block_num has the name name.
 *
 * @param block_num block to check.
 * @param name the wanted name.
 *
 * @return Z80FS_OK if the name matches, Z80FS_E_NOTFOUND if not. Z80FS_E_xxx for other errors.
 */
static error_t fs_check_name(block_t block_num, char *name) {
    uint8_t block_data[BYTE_BLOCK_SIZE];
    if (!readBlock(block_num, block_data)) {
        return Z80FS_E_IO;
    }

    if (block_data[0] == Z80FS_FNODE_FILE) {
        z80fs_file_t *file = (z80fs_file_t *)block_data;

        if (fs_name_compare(file->name, name)) {
            return Z80FS_OK;
        } else {
            return Z80FS_E_NOTFOUND;
        }
    } else if (block_data[0] == Z80FS_FNODE_DIRECTORY) {
        z80fs_dir_t *dir = (z80fs_dir_t *)block_data;

        if (fs_name_compare(dir->name, name)) {
            return Z80FS_OK;
        } else {
            return Z80FS_E_NOTFOUND;
        }
    } else {
        return Z80FS_E_FSBROKEN;
    }
}

/**
 * compare a given name with a name stored in a node.
 *
 * @param name_name the address of the name in the node.
 * @param name a normal cstring.
 *
 * @return TRUE the names match, else FALSE.
 */
static bool fs_name_compare(char *node_name, char *name) {
    for (int i = 0; i < Z80FS_NAME_SIZE; i++) {
        if (name[i] == 0x00) {
            return true;
        } else if (node_name[i] != name[i]) {
            return false;
        }
    }
    return true;
}

/**
 * add a new entry to the current directory. The directory is extended if there are no free slots for a new entry.
 *
 * @param the block number for the new entry.
 *
 * @return Z80FS_OK if the name matches, Z80FS_E_xxx for errors.
 */
static error_t fs_add_dir_entry(block_t new_entry) {
    block_t current = fs_current_dir;
    uint8_t block_data[BYTE_BLOCK_SIZE];
    z80fs_dir_t *dir = (z80fs_dir_t *)block_data;
    block_t *blockEnd = (uint16_t *)&block_data[BYTE_BLOCK_SIZE];
    block_t *entry;

    // modify parent dir
    if (!readBlock(current, block_data)) {
        return Z80FS_E_IO;
    }
    entry = Z80FS_FIRST_DIR_ENTRY(block_data);
    while (true) {
        while (entry < blockEnd) {
            if (*entry == Z80FS_EMPTY) {
                *entry = new_entry;
                if (!writeBlock(current, block_data)) {
                    return Z80FS_E_IO;
                }
                return Z80FS_OK;
            }
            entry++;
        }

        if (dir->extend) {
            // move to extend
            current = dir->extend;
            if (!readBlock(current, block_data)) {
                return Z80FS_E_IO;
            }
            entry = Z80FS_FIRST_EXT_ENTRY(block_data);
        } else {
            // find free block
            uint16_t extend = fs_find_free_block();
            if (!extend) {
                return Z80FS_E_FULL;
            }

            // store prev-block with new extend
            dir->extend = extend;
            if (!writeBlock(current, block_data)) {
                return Z80FS_E_IO;
            }

            // create extend data struct
            memset(block_data, 0, BYTE_BLOCK_SIZE);
            dir->type = Z80FS_FNODE_DIREXTEND;
            dir->extend = Z80FS_EMPTY;
            if (!writeBlock(extend, block_data)) {
                return Z80FS_E_IO;
            }

            // re-enter loop
            current = extend;
            entry = Z80FS_FIRST_EXT_ENTRY(block_data);
        }
    }
}

/**
 * check name for length and allowed characters.
 *
 * @param name the name to check.
 *
 * @return Z80FS_OK or Z80FS_E_NAME.
 */
static error_t fs_check_filename(char *name) {
    if (name == NULL) {
        return Z80FS_E_NAME;
    }

    if ((strlen(name) > Z80FS_NAME_SIZE) || (strlen(name) == 0)) {
        return Z80FS_E_NAME;
    }

    if ((strcmp(name, Z80FS_CURRENT) == 0) || (strcmp(name, Z80FS_PARENT) == 0)) {
        return Z80FS_E_NAME;
    }

    while (*name) {
        if (!(((*name >= 'a') && (*name <= 'z')) || ((*name >= 'A') && (*name <= 'Z')) || ((*name >= '0') && (*name <= '9')) || (*name == '.') || (*name == '_') ||
              (*name == '-') || (*name == '+') || (*name == '*') || (*name == '$') || (*name == '%') || (*name == '&') || (*name == '!') || (*name == '#') || (*name == '^'))) {
            return Z80FS_E_NAME;
        }
        name++;
    }

    return Z80FS_OK;
}
