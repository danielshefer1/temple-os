 #include "kernel.h"

void kmain() {
    start();

    AhciInit();
    
    GetAhciDriveInfo();
    end();
}