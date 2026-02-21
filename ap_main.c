#include "ap_main.h"

void ap_kmain() {
    LoadGDTHelper((gdt_ptr_t*)getGdtPointer());
    LoadIDTHelper((idt_ptr_t*)getIdtPtr());
    uint8_t cpu_id = get_cpuid();
    EnableLapic();
    InitTimer(TIMER_TICK_PER_MS);
    enable_sse();

    kprintf("Core %d woke up!\n", cpu_id);
    cpus_active++;

    CliHelper();
    while (true) HltHelper();
}