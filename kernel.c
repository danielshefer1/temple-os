 #include "kernel.h"

void kmain() {
    start();
    
    PrintAhciDevices();
    end();
}