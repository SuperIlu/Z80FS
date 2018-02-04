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
#include "config.h"
#include "extEEPROM.h"
#include "fileio.h"
#include "z80fs.h"

#define I2C_ERROR(x)                                \
    {                                               \
        if (i2cStat != 0) {                         \
            Serial.print(F("I2C Problem: "));       \
            if (i2cStat == EEPROM_ADDR_ERR) {       \
                Serial.println(F("Wrong address")); \
            } else {                                \
                Serial.print(F("I2C error: "));     \
                Serial.print(i2cStat);              \
                Serial.println(F(""));              \
            }                                       \
            return false;                           \
        } else {                                    \
            return true;                            \
        }                                           \
    }

extEEPROM myEEPROM(kbits_256, 1, 64);
char tmp_name[Z80FS_NAME_SIZE + 1];

void setup(void) {
    Serial.begin(115200);

    myEEPROM.begin(myEEPROM.twiClock100kHz);
}

void printInfo() {
    uint8_t current_block_data[BYTE_BLOCK_SIZE];
    readBlock(Z80FS_SUPERBLOCK, current_block_data);
    z80fs_super_t *super = (z80fs_super_t *)current_block_data;

    if (super->fs_type != Z80FS_VER1) {
        Serial.print(F("Unknown FileSystem type "));
        Serial.println(super->fs_type);
    } else {
        Serial.println(F("FileSystem:"));
        Serial.print(F("  fsType     = "));
        Serial.println(super->fs_type);
        Serial.print(F("  blockSize  = "));
        Serial.println(Z80FS_GET_BLOCKSIZE(super->blk_size));
        Serial.print(F("  firstBlock = "));
        Serial.println(super->first_block);
        Serial.print(F("  numBlocks  = "));
        Serial.println(super->num_blocks);
        Serial.print(F("  name       = "));
        Serial.println(extract_name(super->name));
    }
}

char *extract_name(void *start) {
    char *src = (char *)start;
    memset(tmp_name, 0, Z80FS_NAME_SIZE + 1);
    for (int i = 0; i < Z80FS_NAME_SIZE; i++) {
        tmp_name[i] = src[i];
    }
    return tmp_name;
}

void eraseFlash() {
    Serial.print(F("\n\nErasing FLASH..."));
    uint8_t current_block_data[BYTE_BLOCK_SIZE];
    memset(current_block_data, 0xFF, BYTE_BLOCK_SIZE);
    writeBlock(Z80FS_SUPERBLOCK, current_block_data);
    writeBlock(Z80FS_ROOT_DIR, current_block_data);
    Serial.println(F("OK"));
}

void printBlock(block_t block_num) {
    Serial.print(F("Block @"));
    Serial.println(block_num);

    uint8_t current_block_data[BYTE_BLOCK_SIZE];
    readBlock(current_block_data, block_num);

    for (int i = 0; i < BYTE_BLOCK_SIZE; i++) {
        Serial.print(current_block_data[i], HEX);
        Serial.print(' ');
        if (((i + 1) % 8) == 0) {
            Serial.println();
        }
    }
}

void print_error(error_t e, char *msg) {
    if (e < Z80FS_OK) {
        Serial.print(msg);
        Serial.print(" ERROR=");
        Serial.print(e);
        Serial.print(" := ");
        switch (e) {
            case Z80FS_OK:
                Serial.print("all OK");
                break;
            case Z80FS_E_IO:
                Serial.print("IO error");
                break;
            case Z80FS_E_NODIR:
                Serial.print("not a directory");
                break;
            case Z80FS_E_NOTFOUND:
                Serial.print("name not found");
                break;
            case Z80FS_E_FSBROKEN:
                Serial.print("structural error in FS");
                break;
            case Z80FS_E_EXISTS:
                Serial.print("name already exists");
                break;
            case Z80FS_E_FULL:
                Serial.print("no free blocks");
                break;
            case Z80FS_E_INVALID:
                Serial.print("invalid function");
                break;
            case Z80FS_E_EOF:
                Serial.print("end of file");
                break;
            case Z80FS_E_NAME:
                Serial.print("invalid file name");
                break;
            case Z80FS_E_NOFILE:
                Serial.print("not a file");
                break;
            case Z80FS_E_NOTEMPTY:
                Serial.print("dir is not empty");
                break;
            default:
                Serial.print("UNKNOWN");
                break;
        }
        Serial.println();
    }
}

void loop() {
    // create new FS
    eraseFlash();
    print_error(fs_format("Z80FS"), "format");
    printInfo();

    // change to root dir and create two directories
    print_error(fs_change_dir(NULL), "chdir /");
    print_error(fs_create_dir("dir_1"), "mkdir 1");
    print_error(fs_create_dir("dir_2"), "mkdir 2");
    list();

    // rename second directory
    print_error(fs_rename("dir_2", "dir_2_ren"), "rename");
    list();

    // change to dir1 and create two subdirectories
    print_error(fs_change_dir("dir_1"), "chdir 1");
    print_error(fs_create_dir("sub_1"), "mkdir sub1");
    print_error(fs_create_dir("sub_2"), "mkdir sub2");
    list();

    // change back to parent
    print_error(fs_change_dir(".."), "chdir ..");
    list();

    // create small file
    file_t small;
    uint8_t small_buf[16];
    fill_buffer(small_buf, sizeof(small_buf));
    print_error(fs_create(&small, "small"), "create small");
    print_error(fs_write(&small, small_buf, sizeof(small_buf)), "write small");
    print_error(fs_close(&small), "close small");
    list();

    // read small file
    memset(small_buf, 0, sizeof(small_buf));
    print_error(fs_open(&small, "small"), "open small");
    print_error(fs_read(&small, small_buf, sizeof(small_buf)), "read small");
    print_error(fs_close(&small), "close small");
    check_buffer(small_buf, sizeof(small_buf));

    // create big file
    file_t big;
    uint8_t big_buf[256];
    fill_buffer(big_buf, sizeof(big_buf));
    print_error(fs_create(&big, "big"), "create big");
    print_error(fs_write(&big, big_buf, sizeof(big_buf)), "write big");
    print_error(fs_close(&big), "close big");
    list();

    // read big file
    memset(big_buf, 0, sizeof(big_buf));
    print_error(fs_open(&big, "big"), "open big");
    print_error(fs_read(&big, big_buf, sizeof(big_buf)), "read big");
    print_error(fs_close(&big), "close big");
    check_buffer(big_buf, sizeof(big_buf));

    // check invalif filenames
    print_error(fs_create_dir(NULL), "mkdir fail 1");
    print_error(fs_create_dir(".."), "mkdir fail 2");
    print_error(fs_create_dir("."), "mkdir fail 3");
    print_error(fs_create_dir("*&^$)(*"), "mkdir fail 4");
    print_error(fs_create_dir(""), "mkdir fail 5");
    print_error(fs_create_dir("NULLNULLNULLNULLNULLNULLNULL"), "mkdir fail 6");

    // check delete
    print_error(fs_create_dir("deltest"), "mkdir deltest");
    print_error(fs_change_dir("deltest"), "chdir deltest");
    print_error(fs_create(&small, "small"), "create small");
    print_error(fs_write(&small, small_buf, sizeof(small_buf)), "write small");
    print_error(fs_close(&small), "close small");
    print_error(fs_change_dir(".."), "chdir ..");
    print_error(fs_delete("deltest"), "del deltest");
    print_error(fs_change_dir("deltest"), "chdir deltest");
    print_error(fs_delete("small"), "del small");
    print_error(fs_change_dir(".."), "chdir ..");
    print_error(fs_delete("deltest"), "del deltest");
    list();

    waitKey();
}

void check_buffer(uint8_t *buf, int buf_size) {
    for (int i = 0; i < buf_size; i++) {
        if (buf[i] != i) {
            Serial.print("Data mismatch at ");
            Serial.print(i, HEX);
            Serial.print(" is ");
            Serial.println(buf[i], HEX);
        }
    }
    Serial.println("Data check done");
}

void fill_buffer(uint8_t *buf, int buf_size) {
    for (int i = 0; i < buf_size; i++) {
        buf[i] = i;
    }
}

void list() {
    error_t error;

    dir_iterator_t iterator;
    print_error(fs_iterate_dir(&iterator), "iterate");

    dir_entry_t entry;
    do {
        error = fs_next_entry(&iterator, &entry);
        if (error == Z80FS_OK) {
            Serial.print(entry.name);
            if (entry.type == ENTRY_FILE) {
                Serial.print("\t");
                Serial.print(entry.size);
            } else {
                Serial.print("/\t<DIR>");
            }
            Serial.println();
        }
    } while (error == Z80FS_OK);
}

void waitKey() {
    while (Serial.available() <= 0) {
        ;
    }
    while (Serial.available()) {
        Serial.read();
    }
}
