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

#include "blockio.h"
#include "config.h"
#include "z80fs.h"

/**
 * optimized version to clear first byte of a block.
 *
 * @param block_num the block number.
 *
 * @return TRUE if writing succeeded, else FALSE.
 */
bool clearBlock(block_t block_num) {
    // Serial.print(F("Erasing ")); Serial.println(block_num);
    // Serial.print(F("Erasing ")); Serial.println(block_num * BYTE_BLOCK_SIZE);
    uint8_t data = Z80FS_FNODE_EMPTY;
    byte i2cStat = 0;
    i2cStat = myEEPROM.write(block_num * BYTE_BLOCK_SIZE, &data, 1);
    I2C_ERROR(i2cStat);
}

/**
 * optimized version to get the first byte of a block.
 *
 * @param block_num the block number.
 * @param the memory to hold the data.
 *
 * @return TRUE if reading succeeded, else FALSE.
 */
bool getBlockType(block_t block_num, uint8_t *type) {
    byte i2cStat = 0;
    i2cStat = myEEPROM.read(block_num * BYTE_BLOCK_SIZE, type, 1);
    I2C_ERROR(i2cStat);
}

/**
 * read data to from block.
 *
 * @param block_num the block number.
 * @param dst the memory to hold the block data.
 *
 * @return TRUE if reading succeeded, else FALSE.
 */
bool readBlock(block_t block_num, uint8_t *dst) {
    // Serial.print(F("Reading ")); Serial.println(block_num);
    byte i2cStat = 0;
    i2cStat = myEEPROM.read(block_num * BYTE_BLOCK_SIZE, dst, BYTE_BLOCK_SIZE);
    I2C_ERROR(i2cStat);
}

/**
 * write data to a block.
 *
 * @param block_num the block number.
 * @param src the data to write.
 *
 * @return TRUE if writing succeeded, else FALSE.
 */
bool writeBlock(block_t block_num, uint8_t *src) {
    // Serial.print(F("Writing ")); Serial.println(block_num);
    byte i2cStat = 0;
    i2cStat = myEEPROM.write(block_num * BYTE_BLOCK_SIZE, src, BYTE_BLOCK_SIZE);
    I2C_ERROR(i2cStat);
}
