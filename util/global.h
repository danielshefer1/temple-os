#pragma once

#include "includes.h"
#include "defintions.h"
#include "types.h"

extern volatile bool pit_timer_fired;
extern uint64_t timer_ticks[UINT8_MAX];
extern volatile uint32_t* lapic;
extern volatile uint32_t* ioapic;
extern uint64_t cpu_count;
extern int_override_t* overrides[16];
extern volatile uint64_t overrides_length;
extern uint8_t cpu_ids[UINT8_MAX];
extern volatile uint64_t cpus_active;
extern volatile uint64_t ap_online_ack;
extern volatile pci_config_t* ecam_ptr;
extern volatile hba_mem_t* hba;
extern block_device_node_t* devices_head;
extern partition_device_node_t* parts_head;
extern volatile bool shutdown_req;

// Framebuffer info captured from Limine before we leave its page tables.
// fb_phys is the physical FB base; fb_virt is the kernel-space mapping
// established in start(). When fb_phys == 0, no framebuffer is available.
typedef struct {
    uint64_t fb_phys;
    uint64_t fb_virt;
    uint64_t pitch;
    uint64_t width;
    uint64_t height;
    uint64_t size;
    uint16_t bpp;
    uint8_t  red_shift, red_size;
    uint8_t  green_shift, green_size;
    uint8_t  blue_shift, blue_size;
} fb_info_t;

extern fb_info_t fb_info;