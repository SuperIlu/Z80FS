# Introduction
This is the C implementation of a minimal filesystem for my Z80 homebrew box. It uses an Arduino nano and Microchip 24LC256 I2C EEPROMS.

At first I wanted to implement the CP/M or even the FAT filesystem for my system. But when reading some documentation for these FS I realized I didn't understand enough about filesystem operation to be able to reliable implement these.

Therefore I decided to implement a very simple FS on my own just to get the hang of it. It is neither very elegant, fast or appropriate for FLASH based storage. But my hopes are it is easy to implemnt in Z80 assembly language.

The FS specification is able to handle different block sizes, this implementation is hardcoded to support only the blocksize defined at compile time in config.h (64 byte right now).

The max number of blocks is 2^16, this can be adjusted by changing the size of block_t.

# Compiling/using
It can be compiled using [Arduino IDE](http://www.arduino.cc) and the [I2C EEPROM library](https://github.com/PaoloP74/extEEPROM). It should be no problem to compile with any C compiler and by re-implementing blockio it can be altered to use other means of storage.

The 'user' part of the filesystem is described in *fileio.h*.

# Minimal FS description
* See *z80fs.h* for details about the data structures.
* All data is store little endian.
* File/Dir names are not NULL terminated if all 12 bytes are used.
* FileSystem.pdf contains (a little outdated) graphic representation of a FS example.

## Superblock
* The superblock contains metainformation about the filesystem.
* It always resides in block 0x00 of the medium.
* *blk_size* contains one of *Z80FS_BSxx*
* *first_block* points to the first used block of the filesystem. This is always the root-directory.

## Directories
Directory nodes are marked by *Z80FS_FNODE_DIRECTORY* and are linked by block number. Child nodes are stored right after the fields in *z80fs_dir_t*. Unused entries are NULL. If the block containing the directory is filled the number of entries can be extended by allocating another block, marking it as *Z80FS_FNODE_DIREXTEND* and linking it via the *extend* field in the directory structure.

## Files
File nodes are marked by *Z80FS_FNODE_FILE* and the file data starts right after the metadata in *z80fs_file_t*. If more space is needed additional blocks can be chained with the *extend* field. These blocks are marked by *Z80FS_FNODE_FEXTEND*.

## Free block management
Free blocks are indicated by 0xFF in the first byte of the block. An occupied block is marked by one of the *Z80FS_FNODE_xxx* values.
