#pragma once
#include "includes.h"

// PIC
#define PIC_TIMER_FREQUENCY 100
#define MASTER_PIC 0x21
#define SLAVE_PIC 0xA1

// APIC
#define IOAPIC_REG_INDEX    0x00
#define IOAPIC_REG_DATA     0x10
#define IOAPIC_REDTBL_BASE  0x10
