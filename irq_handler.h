#pragma once

#include "includes.h"
#include "extern.h"
#include "types.h"
#include "defintions.h"
#include "global.h"
#include "vga.h"
#include "keyboard.h"

extern void irq_handler(interrupt_frame_t* frame);
void TimerHandler();
void KeyboardHandler();