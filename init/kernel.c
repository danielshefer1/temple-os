#include "kernel.h"
#include "user_launch.h"
#include "scheduler.h"

static void task_a(void) {
    for (uint64_t i = 0; ; i++) {
        kprintf("[A:%d] ", (uint64_t)i);
        for (volatile uint64_t j = 0; j < 50000000; j++) {}
    }
}

static void task_b(void) {
    for (uint64_t i = 0; ; i++) {
        kprintf("[B:%d] ", (uint64_t)i);
        for (volatile uint64_t j = 0; j < 50000000; j++) {}
    }
}

static void idle_task(void) {
    while (true) HltHelper();
}

void kmain() {
    start();

    SchedulerInit();
    scheduler_attach_bootstrap("bsp_init");

    create_kernel_task(task_a, "task_a");
    create_kernel_task(task_b, "task_b");
    create_kernel_task(idle_task, "idle");

    // Yield into the scheduler so the new tasks start running.
    StiHelper();
    while (true) {
        HltHelper();
    }
}