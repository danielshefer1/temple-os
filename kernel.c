 #include "kernel.h"

void kmain() {
    start();
    
    ParseDevicesMbrs();   
    PrintParitions();
    end();
}