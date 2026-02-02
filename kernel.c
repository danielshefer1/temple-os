 #include "kernel.h"

void kmain() {
    start();

    e820_info_t* info = init_E820(E820_ADDRESS);

    
    end();
}