#include "ap_main.h"
#include "cpu_local.h"
#include "scheduler.h"

void ap_kmain() {
    LoadGDTHelper((gdt_ptr_t*)getGdtPointer());
    LoadIDTHelper((idt_ptr_t*)getIdtPtr());
    uint8_t cpu_id = get_cpuid();
    cpu_init_late(apic_to_index[cpu_id]);
    EnableLapic();
    InitTimer(TIMER_TICK_PER_MS);
    enable_sse();

    kprintf("Core %d woke up!\n", cpu_id);
    cpus_active++;

    // Register this AP with the scheduler so this_cpu()->current is non-NULL.
    // The bootstrap task acts as our per-CPU idle: when the run queue is
    // empty, schedule() returns and we keep hlt'ing until the timer fires
    // again or work arrives.
    // Register this AP with the scheduler so this_cpu()->current is non-NULL.
    // The bootstrap task acts as our per-CPU idle: when the run queue is
    // empty, schedule() returns and we keep hlt'ing until the timer fires
    // again or work arrives.
    scheduler_attach_bootstrap("ap_idle");
    StiHelper();
    while (true) HltHelper();
}