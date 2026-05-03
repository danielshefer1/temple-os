#include "elf64.h"
#include "elf64_defs.h"
#include "pml4_clone.h"
#include "paging.h"
#include "buddy_alloc.h"
#include "slab_alloc.h"
#include "memory.h"
#include "string.h"
#include "vga.h"
#include "defintions.h"
#include "vfs.h"
#include "vfs_file.h"
#include "vfs_path_ops.h"

#define USER_STACK_TOP_VA  0x00007FFFF0000000ULL
#define USER_STACK_PAGES   4

static uint64_t flags_for_phdr(uint32_t p_flags) {
    uint64_t f = PRESENT_PAGE | USER_PAGE;
    if (p_flags & PF_W) f |= RW_PAGE;
    if (!(p_flags & PF_X)) f |= NX_PAGE;
    return f;
}

// Allocate a single user page, install it in the cloned PML4 at va, and return
// the page's kernel-virt alias so the caller can write into it.
static int64_t alloc_and_map_user_page(uint64_t cr3_phys, uint64_t va, uint64_t flags,
                                       void** out_kvirt) {
    void* phys_p = RequestBuddy(PAGE_SIZE, false);
    if (!phys_p) return -ENOMEM;
    uint64_t phys = (uint64_t)phys_p;
    void* kvirt = (void*)(phys + KERNEL_VIRTUAL);
    memset(kvirt, 0, PAGE_SIZE);
    page_entry_t* pml4_kvirt = (page_entry_t*)(cr3_phys + KERNEL_VIRTUAL);
    int64_t r = map_page_to_virt_in(pml4_kvirt, va, phys, flags, false);
    if (r < 0) {
        FreeBuddy(phys_p, false);
        return r;
    }
    *out_kvirt = kvirt;
    return 0;
}

// Translate a user virtual address (in the cloned PML4) to its kernel-virt alias.
// Returns NULL if the page isn't mapped.
static void* user_va_to_kvirt(uint64_t cr3_phys, uint64_t user_va) {
    uint64_t phys;
    if (lookup_user_in_pml4(cr3_phys, user_va & ~((uint64_t)0xFFF), &phys) < 0) return NULL;
    return (void*)(phys + KERNEL_VIRTUAL + (user_va & 0xFFF));
}

// Write `len` bytes from `src` to user-virtual `dst` in the cloned PML4, by
// translating page-by-page through the kernel-virt alias.
static int64_t write_user(uint64_t cr3_phys, uint64_t dst, const void* src, uint64_t len) {
    const uint8_t* s = (const uint8_t*)src;
    while (len > 0) {
        uint64_t page_off = dst & 0xFFF;
        uint64_t chunk = PAGE_SIZE - page_off;
        if (chunk > len) chunk = len;
        void* kdst = user_va_to_kvirt(cr3_phys, dst);
        if (!kdst) return -EINVAL;
        memcpy(kdst, s, chunk);
        dst += chunk;
        s += chunk;
        len -= chunk;
    }
    return 0;
}

static int64_t map_pt_load(uint64_t cr3_phys, file_t* f, Elf64_Phdr* ph, uint64_t bias,
                           elf64_image_t* out, void* scratch) {
    uint64_t va_start = (ph->p_vaddr + bias) & ~((uint64_t)0xFFF);
    uint64_t va_end_unaligned = ph->p_vaddr + bias + ph->p_memsz;
    uint64_t va_end = (va_end_unaligned + 0xFFF) & ~((uint64_t)0xFFF);

    if (va_end_unaligned >= 0xFFFF800000000000ULL) return -EINVAL;
    if (va_start < 0x1000) return -EINVAL;

    uint64_t flags = flags_for_phdr(ph->p_flags);

    for (uint64_t va = va_start; va < va_end; va += PAGE_SIZE) {
        void* kvirt;
        int64_t r = alloc_and_map_user_page(cr3_phys, va, flags, &kvirt);
        if (r < 0) return r;
    }

    if (ph->p_filesz > 0) {
        if (vfs_seek(f, (int64_t)ph->p_offset, 0) < 0) return -EIO;
        uint64_t remaining = ph->p_filesz;
        uint64_t dst = ph->p_vaddr + bias;
        while (remaining > 0) {
            uint64_t chunk = remaining > PAGE_SIZE ? PAGE_SIZE : remaining;
            int64_t n = vfs_read(f, scratch, chunk);
            if (n <= 0) return -EIO;
            int64_t w = write_user(cr3_phys, dst, scratch, (uint64_t)n);
            if (w < 0) return w;
            dst += (uint64_t)n;
            remaining -= (uint64_t)n;
        }
    }

    if (va_end > out->brk) out->brk = va_end;
    return 0;
}

static int64_t apply_pie_relocations(uint64_t cr3_phys, Elf64_Ehdr* eh, Elf64_Phdr* ph_array,
                                     uint64_t bias) {
    Elf64_Phdr* dyn_ph = NULL;
    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        if (ph_array[i].p_type == PT_DYNAMIC) { dyn_ph = &ph_array[i]; break; }
    }
    if (!dyn_ph) return 0;

    uint64_t dyn_va = dyn_ph->p_vaddr + bias;
    uint64_t rela_va = 0, relasz = 0, relaent = 24;

    // Walk PT_DYNAMIC entries via kernel-virt aliases.
    Elf64_Dyn d;
    for (uint64_t off = 0; ; off += sizeof(Elf64_Dyn)) {
        Elf64_Dyn* dp = (Elf64_Dyn*)user_va_to_kvirt(cr3_phys, dyn_va + off);
        if (!dp) return -EINVAL;
        d = *dp;
        if (d.d_tag == DT_NULL) break;
        switch (d.d_tag) {
            case DT_RELA:    rela_va = d.d_val; break;
            case DT_RELASZ:  relasz  = d.d_val; break;
            case DT_RELAENT: relaent = d.d_val; break;
            default: break;
        }
    }
    if (rela_va == 0 || relasz == 0) return 0;
    if (relaent != sizeof(Elf64_Rela)) return -EINVAL;

    uint64_t count = relasz / relaent;
    for (uint64_t i = 0; i < count; i++) {
        Elf64_Rela* rp = (Elf64_Rela*)user_va_to_kvirt(cr3_phys, rela_va + bias + i * relaent);
        if (!rp) return -EINVAL;
        Elf64_Rela rel = *rp;
        if (ELF64_R_TYPE(rel.r_info) != R_X86_64_RELATIVE) return -ENOTSUP;
        uint64_t* tgt = (uint64_t*)user_va_to_kvirt(cr3_phys, rel.r_offset + bias);
        if (!tgt) return -EINVAL;
        *tgt = bias + (uint64_t)rel.r_addend;
    }
    return 0;
}

static int64_t setup_user_stack(uint64_t cr3_phys, elf64_image_t* out) {
    uint64_t top = USER_STACK_TOP_VA;
    uint64_t base = top - USER_STACK_PAGES * PAGE_SIZE;
    for (uint64_t va = base; va < top; va += PAGE_SIZE) {
        void* kvirt;
        int64_t r = alloc_and_map_user_page(
            cr3_phys, va,
            PRESENT_PAGE | RW_PAGE | USER_PAGE | NX_PAGE, &kvirt);
        if (r < 0) return r;
    }

    // SysV initial frame: argc=0, argv[0]=NULL, envp[0]=NULL, auxv key=0, val=0.
    // Five qwords pushed below `top`.
    uint64_t sp_va = top - 5 * sizeof(uint64_t);
    uint64_t zeros[5] = {0, 0, 0, 0, 0};
    int64_t r = write_user(cr3_phys, sp_va, zeros, sizeof(zeros));
    if (r < 0) return r;
    out->stack_top = sp_va;
    return 0;
}

int64_t load_elf64(const char* path, elf64_image_t* out) {
    if (!path || !out) return -EINVAL;
    memset(out, 0, sizeof(*out));

    file_t* f = NULL;
    int64_t r = vfs_open_path(path, O_RDONLY, 0, &f);
    if (r < 0 || !f) return r < 0 ? r : -ENOENT;

    Elf64_Ehdr eh;
    if (vfs_read(f, &eh, sizeof(eh)) != (int64_t)sizeof(eh)) {
        vfs_close(f); return -EIO;
    }

    if (eh.e_ident[0] != ELFMAG0 || eh.e_ident[1] != ELFMAG1 ||
        eh.e_ident[2] != ELFMAG2 || eh.e_ident[3] != ELFMAG3 ||
        eh.e_ident[4] != ELFCLASS64 || eh.e_ident[5] != ELFDATA2LSB ||
        eh.e_machine != EM_X86_64 ||
        (eh.e_type != ET_EXEC && eh.e_type != ET_DYN) ||
        eh.e_phentsize != sizeof(Elf64_Phdr) ||
        eh.e_phnum == 0 || eh.e_phnum > 64) {
        vfs_close(f); return -EINVAL;
    }

    uint64_t ph_bytes = (uint64_t)eh.e_phnum * sizeof(Elf64_Phdr);
    Elf64_Phdr* ph = (Elf64_Phdr*)kmalloc(ph_bytes);
    if (!ph) { vfs_close(f); return -ENOMEM; }
    if (vfs_seek(f, (int64_t)eh.e_phoff, 0) < 0 ||
        vfs_read(f, ph, ph_bytes) != (int64_t)ph_bytes) {
        kfree(ph, ph_bytes); vfs_close(f); return -EIO;
    }

    void* scratch_phys = RequestBuddy(PAGE_SIZE, false);
    if (!scratch_phys) { kfree(ph, ph_bytes); vfs_close(f); return -ENOMEM; }
    void* scratch = (void*)((uint64_t)scratch_phys + KERNEL_VIRTUAL);

    uint64_t cr3_phys = 0;
    r = clone_kernel_pml4(&cr3_phys);
    if (r < 0) { FreeBuddy(scratch_phys, false); kfree(ph, ph_bytes); vfs_close(f); return r; }

    uint64_t bias = (eh.e_type == ET_EXEC) ? 0 : USER_VIRTUAL;
    out->cr3_phys = cr3_phys;
    out->base = bias;
    out->entry = eh.e_entry + bias;

    for (uint16_t i = 0; i < eh.e_phnum; i++) {
        if (ph[i].p_type != PT_LOAD) continue;
        r = map_pt_load(cr3_phys, f, &ph[i], bias, out, scratch);
        if (r < 0) goto fail;
    }

    r = apply_pie_relocations(cr3_phys, &eh, ph, bias);
    if (r < 0) goto fail;

    r = setup_user_stack(cr3_phys, out);
    if (r < 0) goto fail;

    FreeBuddy(scratch_phys, false);
    kfree(ph, ph_bytes);
    vfs_close(f);
    return 0;

fail:
    free_cloned_pml4(cr3_phys);
    FreeBuddy(scratch_phys, false);
    kfree(ph, ph_bytes);
    vfs_close(f);
    return r;
}
