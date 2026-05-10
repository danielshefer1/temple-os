#include "utility.h"
#include "cpu_local.h"
#include "scheduler.h"
#include "tty.h"
#include "pty.h"
#include "devfs.h"
#include "mem_devs.h"
#include "ram_block.h"
#include "disk_devs.h"
#include "procfs.h"
#include "vfs_path.h"
#include "vfs_path_ops.h"
#include "fb.h"
#include "fb_dev.h"
#include "kbd_dev.h"
#include "mouse_dev.h"
#include "ps2.h"
#include "vt.h"

void start() {
    SetGDT();
    InitPaging();
    pat_init();
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
    pty_init();
    mem_devs_init();
    ram_block_init();

    fb_map();
    vt_init_all();
    fb_dev_init();
    kbd_dev_init();
    mouse_dev_init();

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
    ps2_init_aux();
    InitMouse();

    PciEnumeration();
    AhciInit();
    ParseDevicesMbrs();

    superblock_t* sb = EXT2MountRoot();
    vfs_mount_root(sb);
    disk_devs_init();

    // Mount procfs at /proc. The mount point is an ext2 directory created on
    // first boot and reused thereafter; the procfs superblock is allocated
    // afresh each boot since none of its content is persistent.
    procfs_init();
    {
        dentry_t* proc_dir = NULL;
        int64_t r = vfs_namei("/proc", &proc_dir);
        if (r == -ENOENT) {
            vfs_mkdir_path("/proc", 0755);
            r = vfs_namei("/proc", &proc_dir);
        }
        if (r == 0 && proc_dir != NULL) {
            superblock_t* psb = procfs_create_sb();
            if (psb != NULL) {
                int64_t mr = vfs_mount_at(proc_dir, psb);
                if (mr < 0) {
                    kprintf("procfs: mount failed: %d\n", (int)mr);
                }
            } else {
                kprintf("procfs: create_sb failed\n");
            }
        } else {
            kprintf("procfs: /proc lookup failed: %d\n", (int)r);
        }
    }

    BootCores();

    sleep(100); // Wait for APs to finish initializing

    DisableIdentityMapping();

    kprintf("Kernel Initialized Successfully\n");

    // Last visible kernel line. From here on, kprintf/kerror go to the klog
    // VT — they remain capturable by switching to it, but won't clobber the
    // framebuffer once /bin/term takes over.
    vt_klog_redirect(KLOG_VT_IDX);
}


void end() {
    vfs_unmount_root();

    kprintf("Kernel has finished! Press Shift + Q to shutdown!");
    while (true) {
        HltHelper();
    }
}