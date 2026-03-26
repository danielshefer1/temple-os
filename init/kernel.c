#include "kernel.h"

void kmain() {
    start();

    sleep(1000);
    Shutdown();
    
    end();
}