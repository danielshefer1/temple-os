#include "ap_main.h"

void ap_kmain() {
    LoadGDTHelper(getGdtPointer());
    LoadIDTHelper(getIdtPtr());
    uint8_t cpu_id = get_cpuid();
    SetNewTss(cpu_id);
    EnableLapic();
    InitTimer(TIMER_TICK_PER_MS);
    enable_sse();
    cpus_active++;

    kprintf("Core %d woke up!\n", cpu_id);
    CliHelper();
    while (true) HltHelper();
}