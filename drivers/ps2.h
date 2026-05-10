#pragma once
#include "includes.h"

// 8042 controller bring-up. Disables both ports, flushes the output buffer,
// programs the configuration byte to enable the auxiliary (mouse) port and
// its IRQ, then sends 0xF6/0xF4 to put the mouse in streaming mode at
// default settings (3-byte packets, 100 Hz). Returns 0 on success, < 0 on
// timeout / NAK. Safe to call once at boot, before InitMouse() unmasks the
// IOAPIC entry.
int64_t ps2_init_aux(void);
