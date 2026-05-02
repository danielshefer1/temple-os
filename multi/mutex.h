#pragma once

#include "includes.h"
#include "types.h"

void mutex_init(mutex_t* m);
void mutex_lock(mutex_t* m);
void mutex_unlock(mutex_t* m);
