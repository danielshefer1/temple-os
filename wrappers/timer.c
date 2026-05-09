#include "timer.h"
#include "cpu_local.h"
#include "scheduler.h"

void sleep(uint64_t ms) {
    cpu_local_t* cpu = this_cpu();

    // Pre-scheduler boot path (called before scheduler_attach_bootstrap):
    // no current task to park, so busy-wait. Used by start() and ap_start().
    if (cpu->current == NULL) {
        uint8_t cpu_id = get_cpuid();
        uint64_t start = timer_ticks[cpu_id];
        while ((timer_ticks[cpu_id] - start) * TIMER_TICK_PER_MS < ms) {
            PauseHelper();
        }
        return;
    }

    if (ms == 0) return;

    bool ie = check_interrupts();
    CliHelper();
    task_t* cur = cpu->current;
    cur->sleep_deadline = timer_ticks[cpu->cpu_index] + ms; // TIMER_TICK_PER_MS == 1
    sleep_queue_insert(cpu, cur);
    cur->state = TASK_STATE_BLOCKED;
    schedule();
    if (ie) StiHelper();
}

void WaitSeconds(uint64_t seconds) {
    sleep(seconds * SEC);
}
