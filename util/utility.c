#include "utility.h"
#include "cpu_local.h"
#include "scheduler.h"
#include "tty.h"
#include "devfs.h"
#include "mem_devs.h"
#include "ram_block.h"

void start() {
    SetGDT();
    InitPaging();
    DisablePic();
    InitIDT();
    InitVGA();
    enable_sse();
    fpu_init_template();

    uint64_t kernel_size = (uint64_t)&__total_pages;
    InitSlabAlloc(KERNEL_VIRTUAL + kernel_size*PAGE_SIZE);
    e820_info_t* e820_info = init_E820(E820_ADDRESS);
    InitUserBuddyAlloc(e820_info);
    InitKernelBuddyShadow(KERNEL_VIRT_TO_PHYS(GetCurrPrimitveAddr()), GB);
    InitKernelBuddyAlloc(KERNEL_VIRT_TO_PHYS(GetCurrPrimitveAddr()), GB);
    VerifyKernelBuddyShadow();

    devfs_init();
    tty_init(&console_tty);
    mem_devs_init();
    ram_block_init();

    InitRsdt();
    InitMadt();
    InitMcfg();
    InitFadt();

    // BSP per-CPU init: TSS + SYSCALL/SYSRET MSRs. Must come after MADT
    // parsing fills cpu_ids[]/apic_to_index[].
    cpu_locals[0].kstack_top = (uint64_t)&_stack_top;
    cpu_init_late(0);

    // Per-CPU run queues must be initialised before APs come online.
    SchedulerInit();

    EnableLapic();

    InitTimer(TIMER_TICK_PER_MS);
    InitKeyboard();

    PciEnumeration();
    AhciInit();
    ParseDevicesMbrs();

    superblock_t* sb = EXT2MountRoot();
    vfs_mount_root(sb);

    BootCores();

    sleep(100); // Wait for APs to finish initializing

    DisableIdentityMapping();

    kprintf("Kernel Initialized Successfully\n");
}


void end() {
    vfs_unmount_root();

    kprintf("Kernel has finished! Press Shift + Q to shutdown!");
    while (true) {
        HltHelper();
    }
}