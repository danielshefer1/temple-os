#pragma once

#include "types.h"
#include "defintions.h"
#include "includes.h"
#include "extern.h"

void EnableAcpi(fadt_t* fadt);
void AcpiShutdown(fadt_t* fadt);
uint8_t GetCenturyRegInternal(fadt_t* fadt);