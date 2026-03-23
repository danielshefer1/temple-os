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
    uint32_t base_low;    
    uint32_t base_high;     
    uint32_t length_low;  
    uint32_t length_high; 
    uint32_t type;
} e820_entry_t;

typedef struct e820_info_t {
    uint32_t signature;
    uint32_t num_entries;
    e820_entry_t* entries;
    uint64_t address;
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
    uint64_t gs, fs;
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8; // PUSHAQ additional regs
    uint64_t rdi, rsi, rbp, rsp, rbx, rdx, rcx, rax;  // PUSHAQ normal regs
    uint64_t int_no, err_code;
    // Pushed by CPU
    uint64_t rip, cs, qflags, userrsp, ss;
} __attribute__((packed)) interrupt_frame_t;

typedef struct idt_entry_t {
    uint16_t base_low;
    uint16_t sel;
    uint8_t ist;
    // ---- Flags ----
    uint8_t gate_type : 4;
    uint8_t storage_segment : 1;
    uint8_t privilege : 2;
    uint8_t present : 1;
    // ----------------
    uint16_t base_mid;
    uint32_t base_high;
    uint32_t reserved;
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
    uint64_t esp; uint64_t ebp; uint64_t esi; uint64_t rdi;
    uint64_t es; uint64_t cs; uint64_t ss; uint64_t ds; uint64_t fs; uint64_t gs;
    uint64_t ldt; uint16_t trap; uint16_t iomap_base;
} __attribute__((packed)) ;

typedef struct tss_entry_struct tss_entry_t;


typedef struct rsdp_t {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;

    uint32_t length;
    uint64_t xsdt_address;     
    uint8_t extended_checksum;
    uint8_t reserved[3];

} __attribute__((packed)) rsdp_t;

typedef struct acpi_header_t {
    char signature[4];      
    uint32_t length;         
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;    
    uint32_t creator_id;      
    uint32_t creator_revision; 
} __attribute__((packed)) acpi_header_t;

typedef struct rsdt_t {
    acpi_header_t header;
    uint32_t entries[];
} __attribute__((packed)) rsdt_t;

typedef struct madt_t {
    acpi_header_t header;           
    uint32_t local_apic_address;    
    uint32_t flags;                 
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
    uint32_t reserved;              
} __attribute__((packed)) mcfg_entry_t;

typedef struct mcfg_t {
    acpi_header_t header;
    uint64_t reserved;
    mcfg_entry_t entries[]; 
} __attribute__((packed)) mcfg_t;


typedef struct pci_config_t{
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

    uint32_t bars[6];

    uint32_t cardbus_cis_ptr;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t expansion_rom_base_addr;
    uint8_t  capabilities_ptr; 
    uint8_t  reserved0[3];
    uint32_t reserved1;
    uint8_t  interrupt_line;
    uint8_t  interrupt_pin;
    uint8_t  min_grant;
    uint8_t  max_latency;

    uint8_t  device_specific[4032]; 

} __attribute__((packed)) pci_config_t;

typedef struct {
    uint32_t clb;       // Command List Base Address (Low)
    uint32_t clbu;      // Command List Base Address (Upper)
    uint32_t fb;        // FIS Base Address (Low)
    uint32_t fbu;       // FIS Base Address (Upper)
    uint32_t is;        // Interrupt Status
    uint32_t ie;        // Interrupt Enable
    uint32_t cmd;       // Command and Status
    uint32_t reserved0;
    uint32_t tfd;       // Task File Data
    uint32_t sig;       // Signature
    uint32_t ssts;      // SATA Status (SCR0: SStatus)
    uint32_t sctl;      // SATA Control (SCR2: SControl)
    uint32_t serr;      // SATA Error (SCR1: SError)
    uint32_t sact;      // SATA Active (SCR3: SActive)
    uint32_t ci;        // Command Issue
    uint32_t sntf;      // SATA Notification (SCR4: SNotification)
    uint32_t fbs;       // FIS-based Switching Control
    uint32_t reserved1[11];
    uint32_t vendor[4]; // Vendor specific
} __attribute__((packed)) hba_port_t;

typedef struct {
    uint32_t cap;       // Host Capabilities
    uint32_t ghc;       // Global Host Control
    uint32_t is;        // Interrupt Status
    uint32_t pi;        // Ports Implemented
    uint32_t vs;        // Version
    uint32_t ccc_ctl;   // Command Completion Coalescing Control
    uint32_t ccc_pts;   // Command Completion Coalescing Ports
    uint32_t em_loc;    // Enclosure Management Location
    uint32_t em_ctl;    // Enclosure Management Control
    uint32_t cap2;      // Host Capabilities Extended
    uint32_t bohc;      // BIOS/OS Handoff Control and Status

    uint8_t  reserved[0xA0 - 0x2C]; // Padding

    uint8_t  vendor_specific[0x100 - 0xA0];

    hba_port_t ports[32];
} __attribute__((packed)) hba_mem_t;

typedef struct {
    uint8_t  cap_id;        
    uint8_t  next_ptr;      // Offset to next capability
    uint16_t message_ctl;   // Control bits
    uint32_t message_addr;  // Lower 32 bits of the address
    uint32_t message_addr_u; // (only if bit 7 of message_ctl is set)
    uint16_t message_data;  // vector number
    uint16_t reserved;
} __attribute__((packed)) msi_cap_t;

typedef struct {
    uint32_t dba;       // Data Base Address (Low 32 bits)
    uint32_t dbau;      // Data Base Address Upper (High 32 bits)
    uint32_t reserved0; 
    // Bits 0-21: Byte count (0-based, so 4095 = 4KB)
    // Bit 31: Interrupt on Completion
    uint32_t dw3;       
} __attribute__((packed)) hba_prdt_entry_t;

typedef struct {
    uint8_t  fis_type;   // 0x27 (Register H2D)
    uint8_t  pmport:4;   // Port multiplier
    uint8_t  reserved0:3;
    uint8_t  c:1;        // 1 = Command, 0 = Control
    uint8_t  command;    // Actual command (e.g., 0x25 for READ DMA EXT)
    uint8_t  featurel;   // Feature low

    uint8_t  lba0;       // LBA 0-7
    uint8_t  lba1;       // LBA 8-15
    uint8_t  lba2;       // LBA 16-23
    uint8_t  device;     // Device register

    uint8_t  lba3;       // LBA 24-31
    uint8_t  lba4;       // LBA 32-39
    uint8_t  lba5;       // LBA 40-47
    uint8_t  featureh;   // Feature high

    uint8_t  countl;     // Count low
    uint8_t  counth;     // Count high
    uint8_t  icc;        // Isochronous command completion
    uint8_t  control;    // Control register

    uint8_t  reserved1[4];
} __attribute__((packed)) fis_reg_h2d_t;

typedef struct {
    // 64 bytes of FIS area
    uint8_t  cfis[64];    // Command FIS (usually a fis_reg_h2d_t)
    
    // 16 bytes of ATAPI command (only for CD/DVD drives)
    uint8_t  acmd[16];    
    
    uint8_t  reserved[48]; 

    // Physical Region Descriptor Table (PRDT)
    hba_prdt_entry_t prdt_entries[]; 
} __attribute__((packed)) hba_cmd_table_t;

typedef struct {
    // DW0
    uint8_t  cfl:5;      // Command FIS Length (in DWORDS, 5 for H2D)
    uint8_t  a:1;        // ATAPI
    uint8_t  w:1;        // Write (1 = Host to Device)
    uint8_t  p:1;        // Prefetchable
    uint8_t  r:1;        // Reset
    uint8_t  b:1;        // BIST
    uint8_t  c:1;        // Clear Busy upon R_OK
    uint8_t  reserved0:1;
    uint8_t  pmp:4;      // Port Multiplier Port
    uint16_t prdtl;      // PRDT Length (Number of entries in the table)

    // DW1
    uint32_t prdbc;      // Physical Region Descriptor Byte Count (Transferred so far)

    // DW2 & 3
    uint32_t ctba;       // Command Table Base Address (Low)
    uint32_t ctbau;      // Command Table Base Address Upper (High)

    // DW4 - 7
    uint32_t reserved1[4];
} __attribute__((packed)) hba_cmd_header_t;

typedef struct {
    uint8_t  attributes;    
    uint8_t  chs_start[3];  
    uint8_t  partition_type; 
    uint8_t  chs_end[3];    
    uint32_t lba_start;     
    uint32_t sector_count;  
} mbr_partition_t;

typedef struct {
    uint8_t         bootstrap[446]; // Bootloader code area
    mbr_partition_t partitions[4];  // Four partition entries
    uint16_t        signature;      // 0xAA55
} __attribute__((packed)) mbr_t;

typedef struct block_device {
    char name[32];             
    uint64_t total_sectors;    
    uint32_t sector_size;      
    void* device_specific_ptr; 

    int64_t (*read)(struct block_device* dev, uint64_t lba, uint32_t count, void* buffer);
    int64_t (*write)(struct block_device* dev, uint64_t lba, uint32_t count, void* buffer);    
    int64_t (*flush)(struct block_device* dev);
} block_device_t;

typedef struct partition_device {
    block_device_t* physical_device; 
    uint64_t start_lba;              
    uint64_t sector_count;           
    uint8_t partition_type;          
    
} partition_device_t;

typedef struct block_device_node {
    block_device_t* value;
    struct block_device_node* next;
} block_device_node_t;

typedef struct partition_device_node {
    partition_device_t* value;
    struct partition_device_node* next;
} partition_device_node_t;

typedef struct mutex_t {
    volatile bool locked;     
    uint64_t owner_pcb;      
    void* wait_queue;        
} mutex_t;




typedef struct superblock_t {
    uint64_t magic;
    uint64_t block_size;
    
    void* fs_info;             
    struct superblock_ops_t* ops;
    block_device_t* bdev;        
    uint64_t start_lba;   
    
    struct inode_t* root_inode; 
       
} superblock_t;

typedef struct inode_t {
    uint64_t type;
    uint64_t size;
    uint64_t permissions;
    uint64_t owner_id;
    uint64_t group_id;
    uint64_t ref_count;

    uint64_t created_at;      
    uint64_t modified_at;     
    uint64_t accessed_at;  

    struct inode_ops_t* ops;
    void* fs_data;
    char* syslink_name;
    superblock_t* sb;

    mutex_t mutex;

} inode_t;

typedef enum {
    MOUNT_NONE = 0,
    MOUNT_BIND,       
    MOUNT_FILESYSTEM  
} mount_type_t;

typedef struct dentry_t {
    mutex_t mutex;

    mount_type_t mount_type;
    struct dentry_t* mount_dentry;

    char* name;
    inode_t* inode;
    struct dentry_t* parent;
    struct dentry_t* children;
    struct dentry_t* next;
} dentry_t;


typedef struct dcache_entry {
    dentry_t* dentry;
    struct dcache_entry* next;
} dcache_entry_t;

typedef struct {
    uint64_t free_blocks;     // free_clusters
    uint64_t last_alloc_block;
} fs_stat_t;

typedef struct file_t {
    inode_t*      inode;        
    dentry_t*     dentry;       
    struct file_ops_t*   ops;          
    uint64_t      position;     
    uint64_t      flags;        
    uint64_t      ref_count;    
    void*         driver_data;  
} file_t;

typedef struct superblock_ops_t {
    // inode lifecycle
    inode_t* (*alloc_inode) (struct superblock_t* sb);
    void     (*free_inode)  (struct superblock_t* sb, inode_t* inode);
    int64_t  (*read_inode)  (struct superblock_t* sb, inode_t* inode);
    int64_t  (*write_inode) (struct superblock_t* sb, inode_t* inode);

    // filesystem lifecycle
    int64_t  (*mount)       (struct superblock_t* sb);
    int64_t  (*unmount)     (struct superblock_t* sb);
    int64_t  (*sync)        (struct superblock_t* sb);

    // filesystem info
    int64_t  (*stat)        (struct superblock_t* sb, fs_stat_t* stat);
} superblock_ops_t;


typedef struct inode_ops_t {
    // directory operations
    int64_t  (*lookup)      (inode_t* dir, dentry_t* dentry);
    int64_t  (*create)      (inode_t* dir, dentry_t* dentry, uint64_t permissions);
    int64_t  (*mkdir)       (inode_t* dir, dentry_t* dentry, uint64_t permissions);
    int64_t  (*rmdir)       (inode_t* dir, dentry_t* dentry);
    int64_t  (*unlink)      (inode_t* dir, dentry_t* dentry);
    int64_t  (*rename)      (inode_t* old_dir, dentry_t* old_dentry,
                             inode_t* new_dir, dentry_t* new_dentry);

    // symlink operations
    int64_t  (*symlink)     (inode_t* dir, dentry_t* dentry, const char* target);
    int64_t  (*readlink)    (inode_t* inode, char* buf, uint64_t size);

    // inode info
    int64_t  (*getattr)     (inode_t* inode, fs_stat_t* stat);
    int64_t  (*setattr)     (inode_t* inode, fs_stat_t* stat);
} inode_ops_t;


typedef struct file_ops_t {
    // file I/O
    int64_t  (*read)        (file_t* file, void* buf, uint64_t size);
    int64_t  (*write)       (file_t* file, const void* buf, uint64_t size);
    int64_t  (*seek)        (file_t* file, int64_t offset, int64_t whence);

    // directory I/O
    int64_t  (*readdir)     (file_t* file, dentry_t* out);

    // file lifecycle
    int64_t  (*open)        (inode_t* inode, file_t* file);    uint64_t block_size;      

    int64_t  (*close)       (file_t* file);
    int64_t  (*flush)       (file_t* file);

    // misc
    int64_t  (*ioctl)       (file_t* file, uint64_t cmd, void* arg);
} file_ops_t;

typedef struct ext2_superblock {
    uint32_t s_inodes_count;         // total inodes
    uint32_t s_blocks_count;         // total blocks
    uint32_t s_r_blocks_count;       // reserved blocks (for root)
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;     // block containing superblock (0 or 1)
    uint32_t s_log_block_size;       // block size = 1024 << s_log_block_size
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;                // last mount time
    uint32_t s_wtime;                // last write time
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;                // 0xEF53
    uint16_t s_state;                // 1=clean, 2=errors
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;            // 0=original, 1=dynamic
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;

    // ext2 revision 1 only (s_rev_level == 1)
    uint32_t s_first_ino;            // first usable inode (usually 11)
    uint16_t s_inode_size;           // size of inode struct (usually 128)
    uint16_t s_block_group_nr;       // block group this superblock is in
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];             // filesystem UUID
    char     s_volume_name[16];      // volume label (null terminated)
    char     s_last_mounted[64];     // path where last mounted
    uint32_t s_algo_bitmap;

    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding;

    uint8_t  s_reserved[820];        // pad to 1024 bytes
} __attribute__((packed)) ext2_superblock_t;

typedef struct ext2_block_group_desc {
    uint32_t bg_block_bitmap;        // block number of block bitmap
    uint32_t bg_inode_bitmap;        // block number of inode bitmap
    uint32_t bg_inode_table;         // block number of inode table
    uint16_t bg_free_blocks_count;
    uint16_t bg_free_inodes_count;
    uint16_t bg_used_dirs_count;     // how many inodes are directories
    uint16_t bg_pad;
    uint8_t  bg_reserved[12];
} __attribute__((packed)) ext2_block_group_desc_t;

typedef struct ext2_inode {
    uint16_t i_mode;                 // file type + permissions
    uint16_t i_uid;
    uint32_t i_size;                 // file size in bytes
    uint32_t i_atime;                // last access time
    uint32_t i_ctime;                // creation time
    uint32_t i_mtime;                // last modification time
    uint32_t i_dtime;                // deletion time
    uint16_t i_gid;
    uint16_t i_links_count;          // hard link count
    uint32_t i_blocks;               // number of 512-byte blocks reserved
    uint32_t i_flags;
    uint32_t i_osd1;                 // OS-specific value 1
    uint32_t i_block[15];            // [0..11]=direct, [12]=indirect,
                                     // [13]=double indirect, [14]=triple
    uint32_t i_generation;           // file version (for NFS)
    uint32_t i_file_acl;             // extended attributes block
    uint32_t i_dir_acl;              // for regular files: high 32 bits of size
    uint32_t i_faddr;                // fragment address
    uint8_t  i_osd2[12];             // OS-specific value 2
} __attribute__((packed)) ext2_inode_t;

typedef struct ext2_dir_entry {
    uint32_t inode;                  
    uint16_t rec_len;                
    uint8_t  name_len;              
    uint8_t  file_type;              
    char     name[];                 
} __attribute__((packed)) ext2_dir_entry_t;

typedef struct {
    uint32_t block_size;          // 1024 << s_log_block_size
    uint32_t inodes_per_group;    // s_inodes_per_group
    uint32_t blocks_per_group;    // s_blocks_per_group
    uint32_t inode_size;          // s_inode_size (128 for rev0, varies for rev1)
    uint32_t first_data_block;    // s_first_data_block (0 or 1)
    uint32_t first_ino;           // s_first_ino, first usable inode

    uint32_t total_inodes;        // s_inodes_count
    uint32_t total_blocks;        // s_blocks_count
    uint32_t free_inodes;         // s_free_inodes_count
    uint32_t free_blocks;         // s_free_blocks_count

    uint32_t gdt_lba;             // LBA of the group descriptor table
} ext2_internal_info_t;