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
extern void LoadIDTHelper();
extern uint8_t inb(uint8_t port);
extern void outb(uint8_t port, uint8_t value);
extern bool check_interrupts();
extern void load_tss();
extern void switch_to_user_mode(uint32_t eip, uint32_t esp);

// ISR stubs
extern void isr_stub_0();
extern void isr_stub_1();
extern void isr_stub_2();
extern void isr_stub_3();
extern void isr_stub_4();
extern void isr_stub_5();
extern void isr_stub_6();
extern void isr_stub_7();
extern void isr_stub_8();
extern void isr_stub_9();
extern void isr_stub_10();
extern void isr_stub_11();
extern void isr_stub_12();
extern void isr_stub_13();
extern void isr_stub_14();
extern void isr_stub_16();
extern void isr_stub_17();
extern void isr_stub_18();
extern void isr_stub_19();
extern void isr_stub_20();
extern void isr_stub_21();
extern void isr_stub_32();
extern void isr_stub_33();