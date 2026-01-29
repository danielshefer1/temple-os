#include "apic.h"

static local_apic_t* lapic;
static io_apic_t* ioapic;
static int_override_t* over;
static madt_local_apic_nmi_t* nmi;

void ParseMadt(madt_t* madt) {
    madt_entry_header_t* entry = (madt_entry_header_t*)((uint32_t)madt + sizeof(madt_t));
    uint32_t end = madt->header.length + (uint32_t)madt;
    while ((uint32_t)entry < end) {
        switch (entry->type) {
            case 0:
                lapic = (local_apic_t*) entry;
                kprintf("Found Local APIC!\n");
                break;
            case 1:
                ioapic = (io_apic_t*) entry;
                kprintf("Found I/O APIC!\n");
                break;
            case 2:
                over = (int_override_t*) entry;
                kprintf("Found Interrupts Override!\n");
                break;
            case 4:
                nmi = (madt_local_apic_nmi_t*) entry;
                kprintf("Found NMI!\n");
                break;
            default:
                kprintf("Unkown MADT type: %d\n", entry->type);
        }
        entry = (madt_entry_header_t*)((uint32_t)entry + entry->length);
    }
    kprintf("Finished parsing MADT succesfully!");
}