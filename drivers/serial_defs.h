#pragma once
#include "includes.h"

#define COM1_BASE 0x3F8
#define COM1_FCR COM1_BASE + 2
#define COM1_IIR COM1_BASE + 2
#define COM1_LSR COM1_BASE + 5
#define MASTER_PIC_IMR 0x21
#define IRQ_COM1 4
