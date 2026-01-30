#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "vga.h"

void InitIDT();
void ReplaceTimer();
idt_ptr_t* getIdtPtr();