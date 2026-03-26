#include "kernel.h"

void kmain() {
    start();

    superblock_t* sb = EXT2MountRoot();

    end();
}