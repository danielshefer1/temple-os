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

// Slow path for get_cpuid: invoked when IA32_GS_BASE is still 0 (early BSP,
// AP entry before cpu_init_late). Issues CPUID to read the APIC ID, resolves
// this CPU's cpu_local_t, and programs IA32_GS_BASE so subsequent get_cpuid
// calls hit the fast gs:[apic_id] path. cpu_init_late later re-issues the
// wrmsr (and additionally programs KERNEL_GS_BASE, TSS, SYSCALL MSRs).
uint8_t init_gs_and_get_cpuid(void) {
    uint32_t ebx_val;
    __asm__ volatile ("cpuid"
                      : "=b"(ebx_val)
                      : "a"(1)
                      : "rcx", "rdx");
    uint8_t apic = (uint8_t)(ebx_val >> 24);
    cpu_local_t* c = &cpu_locals[apic_to_index[apic]];
    wrmsr(IA32_GS_BASE, (uint64_t)c);
    return apic;
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
    // Read APIC ID via CPUID directly — get_cpuid() would take its fast
    // path (gs:[apic_id]) at this point because init_gs_and_get_cpuid has
    // already programmed GS_BASE on this CPU, and the apic_id field hasn't
    // been written yet, so the fast path would return 0.
    {
        uint32_t ebx_val;
        __asm__ volatile ("cpuid" : "=b"(ebx_val) : "a"(1) : "rcx", "rdx");
        c->apic_id = (uint8_t)(ebx_val >> 24);
    }
    c->tss = t;
    c->current = NULL;

    // Install per-CPU TSS descriptor in the GDT (slot pair 5+2*idx, 5+2*idx+1).
    uint64_t base = (uint64_t)t;
    SetTSSDescriptor(base, sizeof(tss64_t) - 1, 5 + 2 * idx);

    // Selector for slot N is N*8. Pair starts at 5+2*idx, RPL=0.
    uint16_t tss_sel = (uint16_t)((5 + 2 * idx) << 3);
    LoadTSS(tss_sel);

    // Program SYSCALL/SYSRET MSRs.
    wrmsr(IA32_EFER, rdmsr(IA32_EFER) | EFER_SCE);
    // STAR layout: [47:32] = SYSCALL kernel base, [63:48] = SYSRET user base.
    // SYSCALL sets CS = base, SS = base+8. SYSRET sets CS = base+16, SS = base+8.
    // Intel does NOT force RPL=3 into the SYSRET CS/SS values — they're the
    // raw arithmetic result. So the user base must already have RPL bits set
    // so that +8 = 0x1B (user SS, RPL=3) and +16 = 0x23 (user CS, RPL=3).
    // Without this, user code runs with SS=0x18 (RPL=0), which works for
    // memory access but causes iretq to GPF the first time an IRQ fires
    // during user execution and tries to return to ring 3 with SS RPL=0.
    wrmsr(IA32_STAR,
          ((uint64_t)KERNEL_CS_SEL << 32) |        // SYSCALL: CS=0x08, SS=0x10
          ((uint64_t)(KERNEL_DS_SEL | 3) << 48));  // SYSRET: CS=0x23, SS=0x1B
    wrmsr(IA32_LSTAR, (uint64_t)&syscall_entry);
    wrmsr(IA32_FMASK, RFLAGS_IF | RFLAGS_DF);

    // Per-CPU pointer lives in both GS_BASE (so kernel code can use gs:[off]
    // directly — get_cpuid reads gs:[apic_id]) and KERNEL_GS_BASE (so SWAPGS
    // on syscall entry continues to land the per-CPU pointer in GS).
    wrmsr(IA32_GS_BASE,        (uint64_t)c);
    wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)c);
}
