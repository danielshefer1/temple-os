 #include "kernel.h"

void kmain() {
    start();
    
    ParseDevicesMbrs();   
    InsertDisksAndPartsInVFS();

    dentry_t* root = ProbeForData();
    //if (root != NULL) PrintVFS_Dentry(root, 0);
    end();
}