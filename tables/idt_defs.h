#pragma once
#include "includes.h"

#define IDT_SIZE 256
#define GDT_CODE_SEGMENT 0x08
#define IDT_TYPE_INTERRUPT_GATE 0xE
#define IDT_TYPE_TRAP_GATE 0xF
#define SYS_CALL 0x80
#define TIMER_IDT 32
