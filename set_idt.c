#include "set_idt.h"

static idt_entry idt[256];
static idt_ptr idtr;

static void* handlers[] = {
    (void*)isr_stub_0,
    (void*)isr_stub_1,
    (void*)isr_stub_2,
    (void*)isr_stub_3,
    (void*)isr_stub_4,
    (void*)isr_stub_5,
    (void*)isr_stub_6,
    (void*)isr_stub_7,
    (void*)isr_stub_8,
    (void*)isr_stub_9,
    (void*)isr_stub_10,
    (void*)isr_stub_11,
    (void*)isr_stub_12,
    (void*)isr_stub_13,
    (void*)isr_stub_14,
    (void*)isr_stub_16,
    (void*)isr_stub_17,
    (void*)isr_stub_18,
    (void*)isr_stub_19,
    (void*)isr_stub_20,
    (void*)isr_stub_21,
    (void*)isr_stub_32,
    (void*)isr_stub_33,
    (void*)isr_stub_128
};

static uint32_t handlers_idx[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 16, 17, 18, 19, 20,
    21, 32, 33, 128
};

static uint32_t num_handlers = sizeof(handlers) / sizeof(handlers[0]);
static uint32_t num_handlers_idx = sizeof(handlers_idx) / sizeof(handlers_idx[0]);

void SetIDTEntry(uint32_t offset, uint16_t sel, uint8_t present, uint8_t privilege, uint8_t type, uint32_t idx) {
    idt_entry* entry = &idt[idx];

    entry->base_low = offset & 0xFFFF;
    entry->sel = sel;
    entry->reserved = 0;

    entry->gate_type = type;
    entry->storage_segment = 0;
    entry->privilege = privilege;
    entry->present = present;

    entry->base_high = (offset >> 16) & 0xFFFF;
}

void CheckIDT() {
    idt_ptr current_idtr;
    __asm__ volatile("sidt %0" : "=m"(current_idtr));
    
    kprintf("IDTR Base: %x\n", current_idtr.base);
    kprintf("IDTR Limit: %x\n", current_idtr.limit);
    kprintf("Expected Base: %x\n", (uint32_t)&idt);  // or physical if needed
    kprintf("Expected Limit: %x\n", sizeof(idt) - 1);
}

void InitIDT() {
    if (num_handlers != num_handlers_idx) {
        kerror("Mismatch in handlers and handler indices count\n");
        return;
    }

    for (uint32_t i = 0; i < num_handlers; i++) {
        if (handlers[i] == 0) {
            kerror("Handler %d is NULL\n", i);    
        }
        if (handlers_idx[i] == 128) {
            SetIDTEntry((uint32_t)handlers[i], GDT_CODE_SEGMENT, PRESENT,
                PRIVILEGE_USER, IDT_TYPE_TRAP_GATE, handlers_idx[i]);
            continue;
        }
        SetIDTEntry((uint32_t)handlers[i], GDT_CODE_SEGMENT, PRESENT,
         PRIVILEGE_USER, IDT_TYPE_INTERRUPT_GATE, handlers_idx[i]);
    }

    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)&idt;
    LoadIDTHelper(&idtr);
    StiHelper();

    //CheckIDT();
}

void InitTimer(uint32_t frequency) {
    // 1. Calculate our divisor
    uint32_t divisor = 1193182 / frequency;

    // 2. Send Command Byte (0x36)
    // 0x36 = 00(Channel 0) 11(Lo/Hi Access) 011(Square Wave Mode) 0(16-bit Binary)
    outb(0x43, 0x36);

    // 3. Send Divisor (Split into two bytes)
    uint8_t low  = (uint8_t)(divisor & 0xFF);
    uint8_t high = (uint8_t)((divisor >> 8) & 0xFF);

    outb(0x40, low);
    outb(0x40, high);
}