#pragma once
#include "includes.h"

typedef struct buddy_node_t {
    bool free;
    void* address;
    uint32_t order;
    struct buddy_node_t* next;
} buddy_node_t;

typedef struct buddy_bin_t {
    buddy_node_t* head_free;
    buddy_node_t* head_used;
} buddy_bin_t;

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

typedef struct slab_t
{
    void* start;
    uint32_t num_slots;
    uint32_t free_count;
    struct slab_t* next;
    uint32_t bitmap_size;
    uint32_t bitmap[];
} slab_t;

typedef struct cache_t
{
    uint32_t size;
    slab_t* full_slabs;
    slab_t* partial_slabs;
    slab_t* empty_slabs;
} cache_t;

typedef struct e820_entry_t {
    uint32_t base_low;    // Lower 32 bits of base
    uint32_t base_high;   // Upper 32 bits of base  
    uint32_t length_low;  // Lower 32 bits of length
    uint32_t length_high; // Upper 32 bits of length
    uint32_t type;
} e820_entry_t;

typedef struct e820_info_t {
    uint32_t signature;
    uint32_t num_entries;
    e820_entry_t* entries;
    uint32_t address;
} e820_info_t;

typedef struct tuple_t {
    uint32_t first;
    uint32_t second;
} tuple_t;

typedef struct int_node_t {
    uint32_t val;
    struct int_node_t* next;
} int_node_t;

typedef struct tuple_node_t {
    tuple_t val;
    struct tuple_node_t* next;
} tuple_node_t;

typedef struct gdt_entry_t {
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
} __attribute__((packed)) gdt_entry_t;

typedef struct gdt_ptr_t {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct interrupt_frame_t {
    // Pushed by isr_common_stub
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // pusha
    uint32_t int_no, err_code;
    // Pushed by CPU
    uint32_t eip, cs, eflags, useresp, ss;
} __attribute__((packed)) interrupt_frame_t;

typedef struct idt_entry_t {
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
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr_t {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) idt_ptr_t;

typedef struct input_buffer_t {
    struct timed_key_t* buffer;
    uint32_t size;
    uint32_t head;
    uint32_t tail;
} input_buffer_t;

typedef struct timed_key_t {
    uint32_t time;
    char c;
} timed_key_t;

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

typedef struct mutex_t {
    volatile bool locked;     
    uint32_t owner_pcb;      
    void* wait_queue;        
} mutex_t;

struct dentry_t;

typedef struct vfs_ops_t {
    uint32_t (*read)(struct dentry_t* node, uint32_t offset, uint32_t size, char* buffer);
    uint32_t (*write)(struct dentry_t* node, uint32_t offset, uint32_t size, char* buffer);
    struct dentry_t* (*finddir)(struct dentry_t* node, char* name);
} vfs_ops_t;

typedef struct inode_t {
    uint32_t type;
    uint32_t size;
    uint32_t permissions;
    uint32_t owner_id;
    uint32_t group_id;
    uint32_t link_count;
    mutex_t mutex;
} inode_t;

typedef struct dentry_t {
    char* name;
    char* syslink_name;
    inode_t* inode;
    vfs_ops_t* ops;
    void* driver_data;

    struct dentry_t* parent;
    struct dentry_t* children;
    struct dentry_t* next;

    struct dentry_t* mount_root;
} dentry_t;


typedef struct dcache_entry {
    dentry_t* dentry;
    struct dcache_entry* next;
} dcache_entry_t;


typedef struct rsdp_t {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
} __attribute__((packed)) rsdp_t;

typedef struct rsdt_t{
    char signature[4];      
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
    uint32_t entry[];       
} __attribute__((packed)) rsdt_t;