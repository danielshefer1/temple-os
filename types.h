#pragma once
#include "includes.h"

typedef struct BuddyNode {
    bool free;
    void* address;
    uint32_t order;
    struct BuddyNode* next;
} BuddyNode;

typedef struct BuddyBin {
    BuddyNode* head_free;
    BuddyNode* head_used;
} BuddyBin;

typedef struct pte_t {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t write_thru : 1;
    uint32_t cache_dis  : 1;
    uint32_t accessed   : 1;
    uint32_t dirty      : 1;
    uint32_t pat        : 1;
    uint32_t global     : 1;
    uint32_t avail      : 3;
    uint32_t frame      : 20;
} pte_t;

typedef struct pde_t {
    uint32_t present    : 1;
    uint32_t rw         : 1;
    uint32_t user       : 1;
    uint32_t write_thru : 1;
    uint32_t cache_dis  : 1;
    uint32_t accessed   : 1;
    uint32_t dirty      : 1;
    uint32_t pat        : 1;
    uint32_t global     : 1;
    uint32_t avail      : 3;
    uint32_t frame      : 20;
} pde_t;

typedef struct Slab
{
    void* start;
    uint32_t num_slots;
    uint32_t free_count;
    struct Slab* next;
    uint32_t bitmap_size;
    uint32_t bitmap[];
} Slab;

typedef struct 
{
    uint32_t size;
    Slab* full_slabs;
    Slab* partial_slabs;
    Slab* empty_slabs;
} Cache;

typedef struct E820Entry {
    uint32_t base_low;    // Lower 32 bits of base
    uint32_t base_high;   // Upper 32 bits of base  
    uint32_t length_low;  // Lower 32 bits of length
    uint32_t length_high; // Upper 32 bits of length
    uint32_t type;
} E820Entry;

typedef struct E820Info {
    uint32_t signature;
    uint32_t num_entries;
    E820Entry* entries;
    uint32_t address;
} E820Info;

typedef struct Tuple {
    uint32_t first;
    uint32_t second;
} Tuple;

typedef struct intNode {
    uint32_t val;
    struct intNode* next;
} intNode;

typedef struct tupleNode {
    Tuple val;
    struct tupleNode* next;
} tupleNode;

typedef struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    // ---- Access byte ----
    uint8_t  accessed : 1;
    uint8_t  readable_writable : 1;
    uint8_t  direction_conforming : 1;
    uint8_t  executable : 1;
    uint8_t  descriptor_type : 1;
    uint8_t  privilege : 2;
    uint8_t  present : 1;
    // --------------------
    uint8_t  limit_high : 4;
    // ---- Flags ----
    uint8_t reserved : 1;
    uint8_t  long_mode : 1;
    uint8_t  default_big : 1;
    uint8_t  granularity : 1;
    // ----------------
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry;

typedef struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr;

typedef struct interrupt_frame {
    // Pushed by isr_common_stub
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // pusha
    uint32_t int_no, err_code;
    // Pushed by CPU
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) interrupt_frame;

typedef struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t reserved;
    // ---- Flags ----
    uint8_t gate_type : 4;
    uint8_t storage_segment : 1;
    uint8_t privilege : 2;
    uint8_t present : 1;
    // ----------------
    uint16_t base_high;
} __attribute__((packed)) idt_entry;

typedef struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr;

typedef struct InputBuffer {
    struct TimedKey* buffer;
    uint32_t size;
    uint32_t head;
    uint32_t tail;
} InputBuffer;

typedef struct TimedKey {
    uint32_t time;
    char c;
} TimedKey;

struct tss_entry_struct {
    uint32_t prev_tss;   // Previous TSS (not used in software switching)
    uint32_t esp0;       // The stack pointer to load when switching to Ring 0
    uint32_t ss0;        // The stack segment to load when switching to Ring 0
    uint32_t esp1; uint32_t ss1; uint32_t esp2; uint32_t ss2; // Not used
    uint32_t cr3; uint32_t eip; uint32_t eflags;
    uint32_t eax; uint32_t ecx; uint32_t edx; uint32_t ebx;
    uint32_t esp; uint32_t ebp; uint32_t esi; uint32_t edi;
    uint32_t es; uint32_t cs; uint32_t ss; uint32_t ds; uint32_t fs; uint32_t gs;
    uint32_t ldt; uint16_t trap; uint16_t iomap_base;
} __attribute__((packed));

typedef struct tss_entry_struct tss_entry_t;

typedef struct VFSAttr {
    uint32_t type;
    uint32_t size;
    uint32_t permissions;
    uint32_t owner_id;
    uint32_t group_id;
    uint32_t link_count;
    bool lock;
} VFSAttr;
typedef struct VFSNode {
    VFSAttr attr;
    char* name;
    struct VFSNode* parent;
    struct VFSNode* children;
    struct VFSNode* next;
} VFSNode;
