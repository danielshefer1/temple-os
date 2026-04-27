#include "cpu_local.h"
#include "extern.h"
#include "set_gdt.h"
#include "msr.h"
#include "string.h"
#include "vga.h"

cpu_local_t cpu_locals[MAX_CPUS];
uint8_t apic_to_index[256];
static tss64_t tss_array[MAX_CPUS];

cpu_local_t* this_cpu(void) {
    uint8_t apic = get_cpuid();
    return &cpu_locals[apic_to_index[apic]];
}

void cpu_init_late(uint32_t idx) {
    cpu_local_t* c = &cpu_locals[idx];
    tss64_t* t = &tss_array[idx];

    // Zero the TSS.
    uint8_t* p = (uint8_t*)t;
    for (uint64_t i = 0; i < sizeof(tss64_t); i++) p[i] = 0;
    t->iopb_offset = sizeof(tss64_t);   // no I/O bitmap
    t->rsp0 = c->kstack_top;

    c->self = (uint64_t)c;
    c->kernel_rsp = c->kstack_top;
    c->scratch_user_rsp = 0;
    c->cpu_index = idx;
    c->apic_id = get_cpuid();
    c->tss = t;

    // Install per-CPU TSS descriptor in the GDT (slot pair 5+2*idx, 5+2*idx+1).
    uint64_t base = (uint64_t)t;
    SetTSSDescriptor(base, sizeof(tss64_t) - 1, 5 + 2 * idx);

    // Selector for slot N is N*8. Pair starts at 5+2*idx, RPL=0.
    uint16_t tss_sel = (uint16_t)((5 + 2 * idx) << 3);
    LoadTSS(tss_sel);

    // Program SYSCALL/SYSRET MSRs.
    wrmsr(IA32_EFER, rdmsr(IA32_EFER) | EFER_SCE);
    wrmsr(IA32_STAR,
          ((uint64_t)KERNEL_CS_SEL << 32) |   // SYSCALL: CS=0x08, SS=0x10
          ((uint64_t)KERNEL_DS_SEL << 48));   // SYSRET base: SS=0x1B, CS=0x23
    wrmsr(IA32_LSTAR, (uint64_t)&syscall_entry);
    wrmsr(IA32_FMASK, RFLAGS_IF | RFLAGS_DF);

    // Per-CPU pointer lives in KERNEL_GS_BASE; user GS is 0. SWAPGS in
    // syscall_entry brings the per-CPU pointer into GS on entry.
    wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)c);
    wrmsr(IA32_GS_BASE, 0);
}
