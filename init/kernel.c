#include "kernel.h"

void kmain() {
    start();


    superblock_t* sb = (superblock_t*)EXT2MountRoot();

    end();
}