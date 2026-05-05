#pragma once
#include "includes.h"
#include "lock_types.h"
#include "utility_types.h"

typedef struct ext2_superblock_disk {
    uint32_t s_inodes_count;
    uint32_t s_blocks_count;
    uint32_t s_r_blocks_count;
    uint32_t s_free_blocks_count;
    uint32_t s_free_inodes_count;
    uint32_t s_first_data_block;
    uint32_t s_log_block_size;
    uint32_t s_log_frag_size;
    uint32_t s_blocks_per_group;
    uint32_t s_frags_per_group;
    uint32_t s_inodes_per_group;
    uint32_t s_mtime;
    uint32_t s_wtime;
    uint16_t s_mnt_count;
    uint16_t s_max_mnt_count;
    uint16_t s_magic;
    uint16_t s_state;
    uint16_t s_errors;
    uint16_t s_minor_rev_level;
    uint32_t s_lastcheck;
    uint32_t s_checkinterval;
    uint32_t s_creator_os;
    uint32_t s_rev_level;
    uint16_t s_def_resuid;
    uint16_t s_def_resgid;
    // revision 1 fields
    uint32_t s_first_ino;
    uint16_t s_inode_size;
    uint16_t s_block_group_nr;
    uint32_t s_feature_compat;
    uint32_t s_feature_incompat;
    uint32_t s_feature_ro_compat;
    uint8_t  s_uuid[16];
    char     s_volume_name[16];
    char     s_last_mounted[64];
    uint32_t s_algo_bitmap;
    uint8_t  s_prealloc_blocks;
    uint8_t  s_prealloc_dir_blocks;
    uint16_t s_padding;

    uint8_t  s_journal_uuid[16];
    uint32_t s_journal_inum;
    uint32_t s_journal_dev;
    uint32_t s_last_orphan;
    uint32_t s_hash_seed[4];
    uint8_t  s_def_hash_version;
    uint8_t  s_reserved_char_pad;
    uint16_t s_reserved_word_pad;
    uint32_t s_default_mount_opts;
    uint32_t s_first_meta_bg;
    uint8_t  s_reserved[760];
} __attribute__((packed)) ext2_superblock_disk_t;

typedef struct ext2_inode_disk {
    uint16_t i_mode;                 // file type + permissions
    uint16_t i_uid;
    uint32_t i_size;                 // file size in bytes
    uint32_t i_atime;                // last access time
    uint32_t i_ctime;                // change time
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
} __attribute__((packed)) ext2_inode_disk_t;

typedef struct ext2_dir_entry {
    uint32_t inode;
    uint16_t rec_len;
    uint8_t  name_len;
    uint8_t  file_type;
    char     name[];
} __attribute__((packed)) ext2_dir_entry_t;

typedef struct ext2_block_group_desc_disk_t {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint16_t free_blocks_count;
    uint16_t free_inodes_count;
    uint16_t used_dirs_count;
    uint16_t pad;
    uint8_t  reserved[12];
} __attribute__((packed)) ext2_block_group_desc_disk_t;


typedef struct ext2_block_group_t {
    uint32_t block_bitmap;
    uint32_t inode_bitmap;
    uint32_t inode_table;
    uint32_t free_blocks_count;
    uint32_t free_inodes_count;
    uint32_t used_dirs_count;
} ext2_block_group_t;



typedef struct ext2_info_t {
    uint32_t sectors_per_block;
    uint32_t blocks_per_group;
    uint32_t inodes_per_group;
    uint32_t inode_size;          // size of one ext2_inode_t on disk
    uint32_t block_group_count;
    uint32_t total_blocks;
    uint32_t total_inodes;

    // free space — updated on alloc/free
    uint32_t free_blocks;
    uint32_t free_inodes;

    uint32_t root_inode_number;
    uint32_t first_data_block;
    uint32_t first_usable_inode;

    // block group descriptor table — one entry per block group
    ext2_block_group_t* bgdt;

    // feature flags — read from superblock
    uint32_t feature_compat;
    uint32_t feature_incompat;
    uint32_t feature_ro_compat;

    uint32_t state;

    uint32_t hash_seed[4];
    uint8_t def_hash_version;

    mutex_t inode_alloc_lock;
} ext2_info_t;

typedef struct ext2_inode_data_t {
    uint32_t inode_number;
    uint32_t block_group;
    uint64_t disk_offset;

    uint32_t ref_count;
    uint32_t changed_at;
    uint32_t modified_at;
    uint32_t accessed_at;
    uint32_t i_blocks;

    uint32_t i_generation;
    uint32_t i_file_acl;
    uint32_t i_dir_acl;
    uint32_t i_faddr;
    uint32_t i_osd1;
    uint8_t  i_osd2[12];
    uint32_t i_dtime;
    uint32_t i_block[15];
} ext2_inode_data_t;

typedef struct ext2_private_file_t {
    uint32_t dir_blocks_offset;
    uint32_t dir_bytes_offset;

    u64_node_t* cached_blocks;
} ext2_private_file_t;
