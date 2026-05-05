#pragma once
#include "includes.h"

#define ROOT_LABEL "TEMPLE_OS_ROOT"
#define ROOT_LABEL_LENGTH 9

#define FAT32_BAD     0x0FFFFFF7
#define FAT32_FREE    0x00000000

#define MAX_FILENAME_FAT32 256

#define END_ENTRY 0x00
#define DELETED_ENTRY 0xE5


#define LFN_ATTR 0xF
#define OS_ATTR 0x4
#define VOLUME_LABEL_ATTR 0x8

#define LAST_LFN 0x40

#define FAT32_MAGIC 0xFA732000
