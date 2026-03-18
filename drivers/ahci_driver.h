#pragma once

#include "includes.h"
#include "extern.h"
#include "types.h"
#include "defintions.h"
#include "global.h"
#include "paging.h"
#include "vga.h"
#include "slab_alloc.h"
#include "string.h"
#include "buddy_alloc.h"

void AhciInit();
void GetAhciDriveInfo();
void InitPort(hba_port_t* port);
int64_t FindFreeSlotInCmdList(hba_port_t* port);
bool AhciRead(hba_port_t* port, uint64_t lba, uint16_t count, uint64_t buffer_phys);
bool AhciWrite(hba_port_t* port, uint64_t lba, uint16_t count, uint64_t buffer_phys);
void PrintPortsSig();
void PrintAhciDevices();