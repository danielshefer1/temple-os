 #include "kernel.h"

void kmain() {
    start();

    uint8_t ahci_ports = AhciNumberOfPorts();
    kprintf("Number of AHCI ports: %d\n", ahci_ports);

    end();
}