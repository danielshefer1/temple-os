 #include "kernel.h"

void kmain() {
    start();
    
    ParseDevicesMbrs();   
    InsertDisksAndPartsInVFS();
    PrintVFS_Root();
    end();
}