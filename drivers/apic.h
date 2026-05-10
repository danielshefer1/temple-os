#pragma once

#include "includes.h"
#include "extern.h"
#include "types.h"
#include "defintions.h"
#include "memory.h"
#include "string.h"
#include "vga.h"
#include "global.h"
#include "set_idt.h"
#include "utility.h"

void DisablePic();
void EnableLapic();
void InitTimer(uint64_t ms);
void InitKeyboard();
void InitMouse();

// Broadcast a fixed IPI of `vector` to every CPU except the caller. Spins
// until the LAPIC reports the send is complete. Caller is responsible for
// providing any handshake/ack protocol on top of this.
void SendIpiAllExcludingSelf(uint8_t vector);

// Same, but delivers an NMI. Survives a CLI on the target — used by
// shutdown to halt APs that may be running with interrupts disabled.
void SendNmiAllExcludingSelf(void);