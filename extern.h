#pragma once
#include "types.h"
#include "defintions.h"
#include "includes.h"

extern uint64_t __total_pages;
extern uint64_t _text_size;
extern uint64_t trampoline_binary;
extern uint32_t trampoline_size;
extern uint64_t __kernel_size_bytes;
extern uint64_t __file_size_bytes;
extern uint64_t __text_size;
extern uint64_t _stack_top;

extern void enable_paging_bootstrap(uint64_t *page_directory);
extern void CliHelper();
extern void StiHelper();
extern void HltHelper();
extern void PauseHelper();
extern void LoadGDTHelper(gdt_ptr_t* gdtr);
extern void LoadIDTHelper(idt_ptr_t* idtr);
extern uint8_t inb(uint8_t port);
extern void outb(uint16_t port, uint8_t value);
extern bool check_interrupts();
extern void load_tss();
extern void switch_to_user_mode(uint64_t eip, uint64_t esp);
extern uint8_t get_cpuid();
extern void enable_paging(void* pd);
extern void enable_sse();
extern void spin_lock(spinlock_t* spinlock_addr);
extern void spin_unlock(spinlock_t* spinlock_addr);
extern void switch_pml4(page_entry_t* pml4_addr);
extern void flush_tlb();
extern void InvlpgHelper(uint64_t addr);

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
extern void isr_pic_stub_32();
extern void isr_apic_stub_32();
extern void isr_apic_stub_33();
extern void isr_stub_128();
extern void isr_apic_stub_64();
extern void isr_spurious();