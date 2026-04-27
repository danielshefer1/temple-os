#include "ap_main.h"
#include "cpu_local.h"

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

    CliHelper();
    while (true) HltHelper();
}