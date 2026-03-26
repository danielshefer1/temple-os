#pragma once

#include "includes.h"
#include "defintions.h"
#include "extern.h"
#include "acpi.h"

int64_t GetDate(date_t* out);
int64_t GetTime(time_t* out);
int64_t GetTotalTime(total_time_t* out);
uint32_t CalculateUnixTimestamp(total_time_t* total_time);