#include "ahci_driver.h"

void AhciReset() {
    if (hba == NULL) {
        kprintf("Haven't found AHCI controller yet!\n");
        return;
    }
    hba->ghc |= (1 << 31); 
    
    // Trigger reset
    hba->ghc |= (1 << 0);
    
    while (hba->ghc & 0x01) {
        PauseHelper();
    }
    
    hba->ghc |= (1 << 31);
}

void TakeAhciOwnership() {
    if (hba == NULL) {
        kprintf("Haven't found AHCI controller yet!\n");
        return;
    }
    if (!(hba->cap2 & 1)) {
        kprintf("AHCI controller doesn't support OS ownership change!\n");
        return;
    }
    hba->bohc |= 1;
    uint64_t cpu_id = get_cpuid();
    uint64_t ticks = timer_ticks[cpu_id];
    while ((hba->bohc & 1) && (timer_ticks[cpu_id] - ticks < BIOS_TIMEOUT)) {
        PauseHelper();
    }
    if (hba->bohc & 1) {
        kprintf("Failed to take ownership of AHCI controller within timeout!\n");
    } else {
        kprintf("Successfully took ownership of AHCI controller.\n");
    }
}

void AhciEnableInterrupts() {
    if (hba == NULL) {
        kprintf("Haven't found AHCI controller yet!\n");
        return;
    }
    hba->ghc |= (1 << 1); 
}

uint8_t AhciNumberOfPorts() {
    if (hba == NULL) {
        kprintf("Haven't found AHCI controller yet!\n");
        return 0;
    }
    uint8_t ports_count = 0;
    for (int i = 0; i < 32; i++) {
        if (hba->pi & (1 << i)) {
            ports_count++;
        }
    }
    return ports_count;
}

void InitPort(hba_port_t* port) {
    volatile hba_cmd_header_t* cmd_list = (hba_cmd_header_t*) AddNonCachableKernelPages(2);
    port->clb = (uint64_t)cmd_list;
}

void AhciInit() {

}