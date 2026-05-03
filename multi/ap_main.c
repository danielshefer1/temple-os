#include "ap_main.h"
#include "cpu_local.h"
#include "scheduler.h"

void ap_kmain() {
    // First thing: tell BSP the trampoline scratch area is free to reuse
    // for the next AP. We're already on our own stack with our own CR3.
    __atomic_store_n(&ap_online_ack, 1, __ATOMIC_RELEASE);

    LoadGDTHelper((gdt_ptr_t*)getGdtPointer());
    LoadIDTHelper((idt_ptr_t*)getIdtPtr());
    uint8_t cpu_id = get_cpuid();
    cpu_init_late(apic_to_index[cpu_id]);
    EnableLapic();
    InitTimer(TIMER_TICK_PER_MS);
    enable_sse();

    kprintf("Core %d woke up!\n", cpu_id);
    __atomic_fetch_add(&cpus_active, 1, __ATOMIC_RELEASE);

    scheduler_attach_bootstrap("ap_idle");
    StiHelper();
    while (true) HltHelper();
}