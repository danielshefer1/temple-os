#include "kernel.h"
#include "user_launch.h"
#include "scheduler.h"
#include "elf64.h"
#include "user_task.h"

static void idle_task(void) {
    while (true) HltHelper();
}

void kmain() {
    start();

    scheduler_attach_bootstrap("bsp_init");

    // Cheap kernel task that always halts. Guarantees every CPU has at least
    // one runnable task even when init/term/syncer are all blocked, so the
    // scheduler never has to fall into its no-runnable-task idle spin.
    create_kernel_task(idle_task, "idle");

    // Launch /bin/init as PID 1. Init forks a periodic syncer and supervises
    // /bin/term (respawning it on exit). /bin/term in turn opens /dev/fb,
    // /dev/kbd, /dev/ptmx and forks /bin/hello inside the pty.
    elf64_image_t img;
    int64_t r = load_elf64("/bin/init", &img);
    if (r < 0) {
        kprintf("FATAL: load_elf64(/bin/init)=%d - no userspace started\n", r);
    } else {
        create_user_task(&img, "init");
    }
    // Yield into the scheduler so the new tasks start running.
    StiHelper();
    while (true) {
        HltHelper();
    }
}