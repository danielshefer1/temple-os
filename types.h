#pragma once
#include "includes.h"

typedef struct buddy_node_t {
    bool free;
    void* address;
    uint64_t order;
    struct buddy_node_t* next;
} buddy_node_t;

typedef struct buddy_bin_t {
    buddy_node_t* head_free;
    buddy_node_t* head_used;
} buddy_bin_t;

typedef struct {
    uint64_t present    : 1;
    uint64_t writable   : 1;
    uint64_t user       : 1;
    uint64_t pwt        : 1;
    uint64_t pcd        : 1;
    uint64_t accessed   : 1;
    uint64_t dirty      : 1;
    uint64_t page_size  : 1; 
    uint64_t global     : 1;
    uint64_t available  : 3;
    uint64_t address    : 40; 
    uint64_t reserved   : 11;
    uint64_t no_execute : 1;
} __attribute__((packed)) page_entry_t;

typedef struct slab_t
{
    void* start;
    uint64_t num_slots;
    uint64_t free_count;
    struct slab_t* next;
    uint64_t bitmap_size;
    uint64_t bitmap[];
} slab_t;

typedef struct cache_t
{
    uint64_t size;
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
    uint64_t first;
    uint64_t second;
} tuple_t;

typedef struct int_node_t {
    uint64_t val;
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
    uint64_t base;
} __attribute__((packed)) gdt_ptr_t;

typedef struct interrupt_frame_t {
    // Pushed by isr_common_stub
    uint64_t gs, fs, es, ds;
    uint64_t edi, esi, ebp, esp, ebx, edx, ecx, eax;  // pusha
    uint64_t int_no, err_code;
    // Pushed by CPU
    uint64_t eip, cs, eflags, useresp, ss;
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
    uint64_t base;
} __attribute__((packed)) idt_ptr_t;

typedef struct input_buffer_t {
    struct timed_key_t* buffer;
    uint64_t size;
    uint64_t head;
    uint64_t tail;
} input_buffer_t;

typedef struct timed_key_t {
    uint64_t time;
    char c;
} timed_key_t;

struct tss_entry_struct {
    uint64_t prev_tss;   // Previous TSS (not used in software switching)
    uint64_t esp0;       // The stack pointer to load when switching to Ring 0
    uint64_t ss0;        // The stack segment to load when switching to Ring 0
    uint64_t esp1; uint64_t ss1; uint64_t esp2; uint64_t ss2; // Not used
    uint64_t cr3; uint64_t eip; uint64_t eflags;
    uint64_t eax; uint64_t ecx; uint64_t edx; uint64_t ebx;
    uint64_t esp; uint64_t ebp; uint64_t esi; uint64_t edi;
    uint64_t es; uint64_t cs; uint64_t ss; uint64_t ds; uint64_t fs; uint64_t gs;
    uint64_t ldt; uint16_t trap; uint16_t iomap_base;
} __attribute__((packed));

typedef struct tss_entry_struct tss_entry_t;

typedef struct mutex_t {
    volatile bool locked;     
    uint64_t owner_pcb;      
    void* wait_queue;        
} mutex_t;

struct dentry_t;

typedef struct vfs_ops_t {
    uint64_t (*read)(struct dentry_t* node, uint64_t offset, uint64_t size, char* buffer);
    uint64_t (*write)(struct dentry_t* node, uint64_t offset, uint64_t size, char* buffer);
    struct dentry_t* (*finddir)(struct dentry_t* node, char* name);
} vfs_ops_t;

typedef struct inode_t {
    uint64_t type;
    uint64_t size;
    uint64_t permissions;
    uint64_t owner_id;
    uint64_t group_id;
    uint64_t link_count;
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
    uint64_t rsdt_address;
} __attribute__((packed)) rsdp_t;

typedef struct acpi_header_t {
    char signature[4];      
    uint64_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint64_t oem_revision;
    uint64_t creator_id;
    uint64_t creator_revision;
} __attribute__((packed)) acpi_header_t;

typedef struct rsdt_t {
    acpi_header_t header;
    acpi_header_t** entries[];
} __attribute__((packed)) rsdt_t;

typedef struct madt_t {
    acpi_header_t header;           
    uint64_t local_apic_address;    
    uint64_t flags;                 
} __attribute__((packed)) madt_t;

typedef struct madt_entry_header_t {
    uint8_t type;
    uint8_t length;
} __attribute__((packed)) madt_entry_header_t;

typedef struct local_apic_t {
    madt_entry_header_t header;
    uint8_t acpi_processor_id;  
    uint8_t apic_id;
    uint64_t flags;
} __attribute__((packed)) local_apic_t;

typedef struct io_apic_t {
    madt_entry_header_t header;
    uint8_t ioapic_id; 
    uint8_t reserved;
    uint64_t ioapic_address;
    uint64_t global_system_interrupt_base;
} __attribute__((packed)) io_apic_t;

typedef struct int_override_t {
    madt_entry_header_t header;
    uint8_t bus_source; 
    uint8_t irq_source;
    uint64_t global_system_interrupt;
    uint16_t flags;
} __attribute__((packed)) int_override_t;

typedef struct madt_local_apic_nmi_t {
    madt_entry_header_t header;           
    uint8_t acpi_processor_id; 
    uint16_t flags;            
    uint8_t lint;              
} __attribute__((packed)) madt_local_apic_nmi_t;

typedef struct spinlock_t {
    volatile uint64_t locked;
} spinlock_t;

typedef struct mcfg_entry_t {
    uint64_t base_address;     
    uint16_t pci_segment_group;
    uint8_t start_bus_number;
    uint8_t end_bus_number;
    uint64_t reserved;
} __attribute__((packed)) mcfg_entry_t;

typedef struct mcfg_t{
    acpi_header_t header;
    uint64_t reserved;
    mcfg_entry_t entries[]; 
} __attribute__((packed)) mcfg_t;

#include <stdint.h>

typedef struct pci_config_t{
    // --- Standard PCI Header (First 64 bytes) ---
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t  revision_id;
    uint8_t  prog_if;
    uint8_t  subclass;
    uint8_t  class_code;
    uint8_t  cache_line_size;
    uint8_t  latency_timer;
    uint8_t  header_type;
    uint8_t  bist;

    uint64_t bars[6];

    uint64_t cardbus_cis_ptr;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint64_t expansion_rom_base_addr;
    uint8_t  capabilities_ptr; 
    uint8_t  reserved0[3];
    uint64_t reserved1;
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
    uint8_t  min_grant;
    uint8_t  max_latency;

    uint8_t  device_specific[4032]; 

} __attribute__((packed)) pci_config_t;