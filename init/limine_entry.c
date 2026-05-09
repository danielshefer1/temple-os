#include "limine.h"
#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "global.h"

extern char _stack_top[];
extern char _kernel_VMA_start[];
extern char _kernel_VMA_end[];

extern void kmain(void);

__attribute__((used))
volatile LIMINE_BASE_REVISION(2);

__attribute__((used))
static volatile struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

__attribute__((used))
static volatile struct limine_hhdm_request hhdm_request = {
    .id = LIMINE_HHDM_REQUEST,
    .revision = 0,
};

__attribute__((used))
static volatile struct limine_kernel_address_request kernel_address_request = {
    .id = LIMINE_KERNEL_ADDRESS_REQUEST,
    .revision = 0,
};

__attribute__((used))
static volatile struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
};

#define MAX_E820_ENTRIES 256
static e820_entry_t shim_e820_entries[MAX_E820_ENTRIES];
static uint32_t shim_e820_count;

// Shim page tables used only when Limine loaded us at a physical address other
// than KERNEL_BASE. They map virt KERNEL_VIRTUAL+ -> phys 0+, so a kernel image
// physically at 0x200000 ends up at virt 0xFFFFFFFF80200000 (linker layout).
static page_entry_t shim_pml4[512]    __attribute__((aligned(4096)));
static page_entry_t shim_id_pdpt[512] __attribute__((aligned(4096)));
static page_entry_t shim_id_pd[512]   __attribute__((aligned(4096)));
static page_entry_t shim_k_pdpt[512]  __attribute__((aligned(4096)));
static page_entry_t shim_k_pd[512]    __attribute__((aligned(4096)));

static void shim_memcpy(volatile uint8_t* dst, const volatile uint8_t* src, uint64_t n) {
    for (uint64_t i = 0; i < n; i++) dst[i] = src[i];
}

static uint32_t limine_to_e820_type(uint64_t t) {
    switch (t) {
        case LIMINE_MEMMAP_USABLE:
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            return 1;
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            return 3;
        case LIMINE_MEMMAP_ACPI_NVS:
            return 4;
        case LIMINE_MEMMAP_BAD_MEMORY:
            return 5;
        default:
            return 2;
    }
}

static void populate_e820_entries(void) {
    struct limine_memmap_response* mm = memmap_request.response;
    uint32_t count = 0;

    if (mm != NULL && mm->entry_count > 0) {
        uint64_t n = mm->entry_count;
        if (n > MAX_E820_ENTRIES) n = MAX_E820_ENTRIES;
        for (uint64_t i = 0; i < n; i++) {
            struct limine_memmap_entry* e = mm->entries[i];
            shim_e820_entries[count].base_low    = (uint32_t)e->base;
            shim_e820_entries[count].base_high   = (uint32_t)(e->base >> 32);
            shim_e820_entries[count].length_low  = (uint32_t)e->length;
            shim_e820_entries[count].length_high = (uint32_t)(e->length >> 32);
            shim_e820_entries[count].type        = limine_to_e820_type(e->type);
            count++;
        }
    }
    shim_e820_count = count;
}

// Write the e820_info_t header at physical 0x500 (where init_E820 expects it).
// Done while Limine's HHDM is still active.
static void write_e820_header(uint64_t hhdm) {
    volatile uint32_t* hdr32 = (volatile uint32_t*)(hhdm + 0x500);
    hdr32[0] = E820_SIGNATURE;
    hdr32[1] = shim_e820_count;
    volatile uint64_t* hdr64 = (volatile uint64_t*)(hhdm + 0x508);
    hdr64[0] = KERNEL_VIRT_TO_PHYS((uint64_t)shim_e820_entries);
    hdr64[1] = 0;
}

static void build_shim_page_tables(void) {
    for (int i = 0; i < 512; i++) {
        shim_pml4[i]    = (page_entry_t){0};
        shim_id_pdpt[i] = (page_entry_t){0};
        shim_id_pd[i]   = (page_entry_t){0};
        shim_k_pdpt[i]  = (page_entry_t){0};
        shim_k_pd[i]    = (page_entry_t){0};
    }

    shim_pml4[0].present  = 1;
    shim_pml4[0].writable = 1;
    shim_pml4[0].address  = (uint64_t)KERNEL_VIRT_TO_PHYS((uint64_t)shim_id_pdpt) >> 12;

    shim_id_pdpt[0].present  = 1;
    shim_id_pdpt[0].writable = 1;
    shim_id_pdpt[0].address  = (uint64_t)KERNEL_VIRT_TO_PHYS((uint64_t)shim_id_pd) >> 12;

    for (int i = 0; i < 8; i++) {
        shim_id_pd[i].present   = 1;
        shim_id_pd[i].writable  = 1;
        shim_id_pd[i].page_size = 1;
        shim_id_pd[i].address   = (uint64_t)(i * 2 * MB) >> 12;
    }

    uint64_t pml4_idx = PML4_IDX(KERNEL_VIRTUAL);
    uint64_t pdpt_idx = PDPT_IDX(KERNEL_VIRTUAL);
    uint64_t pd_idx   = PD_IDX(KERNEL_VIRTUAL);

    shim_pml4[pml4_idx].present  = 1;
    shim_pml4[pml4_idx].writable = 1;
    shim_pml4[pml4_idx].address  = (uint64_t)KERNEL_VIRT_TO_PHYS((uint64_t)shim_k_pdpt) >> 12;

    shim_k_pdpt[pdpt_idx].present  = 1;
    shim_k_pdpt[pdpt_idx].writable = 1;
    shim_k_pdpt[pdpt_idx].address  = (uint64_t)KERNEL_VIRT_TO_PHYS((uint64_t)shim_k_pd) >> 12;

    for (int i = 0; i < 8; i++) {
        shim_k_pd[pd_idx + i].present   = 1;
        shim_k_pd[pd_idx + i].writable  = 1;
        shim_k_pd[pd_idx + i].page_size = 1;
        shim_k_pd[pd_idx + i].address   = (uint64_t)(i * 2 * MB) >> 12;
    }
}

// Clear CR0.WP. The original bootstrap left WP=0; Limine sets it to 1, which
// turns a latent bug in paging.c::InitPaging (small-page writability marked
// inverted from intent) into an immediate fault on the first BSS write after
// switching to kernel page tables. Matching the original boot environment
// keeps the kernel unmodified.
// Capture framebuffer info before we leave Limine's page tables. The address
// Limine reports is a virtual pointer in its HHDM mapping; convert it to a
// physical address now and stash it in fb_info so start() can map it later.
static void capture_framebuffer(uint64_t hhdm) {
    struct limine_framebuffer_response* fbr = framebuffer_request.response;
    if (fbr == NULL || fbr->framebuffer_count == 0) return;
    struct limine_framebuffer* fb = fbr->framebuffers[0];
    if (fb == NULL || fb->address == NULL) return;

    uint64_t fb_virt_hhdm = (uint64_t)fb->address;
    uint64_t fb_phys      = fb_virt_hhdm - hhdm;
    uint64_t bytes        = fb->pitch * fb->height;
    uint64_t size_aligned = (bytes + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1);

    fb_info.fb_phys     = fb_phys;
    fb_info.fb_virt     = 0;
    fb_info.pitch       = fb->pitch;
    fb_info.width       = fb->width;
    fb_info.height      = fb->height;
    fb_info.size        = size_aligned;
    fb_info.bpp         = fb->bpp;
    fb_info.red_shift   = fb->red_mask_shift;
    fb_info.red_size    = fb->red_mask_size;
    fb_info.green_shift = fb->green_mask_shift;
    fb_info.green_size  = fb->green_mask_size;
    fb_info.blue_shift  = fb->blue_mask_shift;
    fb_info.blue_size   = fb->blue_mask_size;
}

static void clear_cr0_wp(void) {
    uint64_t cr0;
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(1ULL << 16);
    __asm__ volatile ("mov %0, %%cr0" :: "r"(cr0) : "memory");
}

__attribute__((noreturn))
void limine_entry(void) {
    if (!LIMINE_BASE_REVISION_SUPPORTED) {
        for (;;) __asm__ volatile ("hlt");
    }

    clear_cr0_wp();

    struct limine_kernel_address_response* ka = kernel_address_request.response;
    struct limine_hhdm_response*            hh = hhdm_request.response;

    if (ka == NULL || hh == NULL) {
        for (;;) __asm__ volatile ("hlt");
    }

    uint64_t phys_base = ka->physical_base;
    uint64_t hhdm      = hh->offset;
    bool     relocate  = (phys_base != KERNEL_BASE);

    if (relocate) {
        build_shim_page_tables();
    }

    populate_e820_entries();
    write_e820_header(hhdm);
    capture_framebuffer(hhdm);

    if (relocate) {
        uint64_t kernel_size = (uint64_t)_kernel_VMA_end - (uint64_t)_kernel_VMA_start;
        shim_memcpy((volatile uint8_t*)(hhdm + KERNEL_BASE),
                    (volatile uint8_t*)(hhdm + phys_base),
                    kernel_size);

        uint64_t new_cr3 = KERNEL_VIRT_TO_PHYS((uint64_t)shim_pml4);
        __asm__ volatile (
            "mov %0, %%rsp\n"
            "mov %1, %%cr3\n"
            "xor %%rbp, %%rbp\n"
            "call kmain\n"
            "1: hlt\n"
            "jmp 1b\n"
            :: "r"(_stack_top), "r"(new_cr3) : "memory"
        );
    } else {
        __asm__ volatile (
            "mov %0, %%rsp\n"
            "xor %%rbp, %%rbp\n"
            "call kmain\n"
            "1: hlt\n"
            "jmp 1b\n"
            :: "r"(_stack_top) : "memory"
        );
    }
    __builtin_unreachable();
}
