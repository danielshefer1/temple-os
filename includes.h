#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>
// External symbols from linker
extern uint32_t __total_pages;
extern uint32_t _text_size;

// External functions
extern void enable_paging(uint32_t *page_directory);
extern void CliHelper();
extern void StiHelper();
extern void HltHelper();
extern void LoadGDTHelper();