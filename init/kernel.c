 #include "kernel.h"

void kmain() {
    start();
    
    ParseDevicesMbrs();

    fat32_internal_info_t info;
    int64_t mountroot_code = Fat32MountRoot(&info);
    kprintf("MountRoot return code: %x\n", mountroot_code);
    if (mountroot_code == 0) Fat32_LookUp(&info, info.root_cluster, NULL, NULL);

    end();
}