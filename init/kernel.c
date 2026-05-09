#include "kernel.h"
#include "user_launch.h"
#include "scheduler.h"
#include "elf64.h"
#include "user_task.h"

static void task_a(void) {
    for (uint64_t i = 0; i < 10; i++) {
        kprintf("[A:%d] ", (uint64_t)i);
        for (volatile uint64_t j = 0; j < 50000000; j++) {}
    }
}

static void task_b(void) {
    for (uint64_t i = 0; i < 50; i++) {
        kprintf("[B:%d] ", (uint64_t)i);
        for (volatile uint64_t j = 0; j < 50000000; j++) {}
    }
}

static void idle_task(void) {
    while (true) HltHelper();
}

void kmain() {
    start();

    scheduler_attach_bootstrap("bsp_init");

    create_kernel_task(task_a, "task_a");
    create_kernel_task(task_b, "task_b");
    create_kernel_task(idle_task, "idle");

    // step 1: full loader + user task
    elf64_image_t img;
    int64_t r = load_elf64("/bin/hello", &img);
    if (r < 0) {
        kprintf("load_elf64=%d\n", r);
    } else {
        create_user_task(&img, "hello");
    }
    VerifyKernelBuddyShadow();

    // /proc smoke test — verify the procfs mount delivers content.
    {
        const char* paths[] = {
            "/proc/uptime", "/proc/meminfo", "/proc/cpuinfo",
            "/proc/version", "/proc/mounts", "/proc/stat",
        };
        for (uint64_t i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
            file_t* pf = NULL;
            int64_t pr = vfs_open_path(paths[i], O_RDONLY, 0, &pf);
            if (pr < 0) { kprintf("procfs: open %s = %d\n", paths[i], (int)pr); continue; }
            char pbuf[256];
            int64_t rr = vfs_read(pf, pbuf, sizeof(pbuf) - 1);
            if (rr < 0) { kprintf("procfs: read %s = %d\n", paths[i], (int)rr); }
            else {
                pbuf[rr] = '\0';
                kprintf("--- %s (%d bytes) ---\n%s", paths[i], (int)rr, pbuf);
            }
            vfs_file_put(pf);
        }
    }

    // Yield into the scheduler so the new tasks start running.
    StiHelper();
    while (true) {
        HltHelper();
    }
}