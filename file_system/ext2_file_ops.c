#include "ext2_file_ops.h"

int64_t EXT2Read(file_t* file, void* buf, uint64_t size) {
    if (file == NULL || buf == NULL) return -EINVAL;
    if (file->inode == NULL || file->dentry == NULL) return -EINVAL;
    if (UINT64_MAX - size < file->position) return -EINVAL;


    superblock_t* sb = file->inode->sb;
    uint64_t end_offset = file->position + size;
    uint64_t clamped    = end_offset < file->inode->size ? end_offset : file->inode->size;
    uint64_t to_read    = clamped - file->position;
    uint32_t block_size = sb->block_size;
    uint64_t blocks_to_read = (to_read + block_size - 1) / block_size;

    uint32_t curr_block;
    uint64_t buf_offset = 0, append, remain = to_read;
    void* temp_buf;

    for (uint64_t i = 0; i < blocks_to_read && remain > 0; i++) {
        curr_block = FindDataBlock(file->inode, i);
        if (curr_block == 0) continue;
        temp_buf = bread(sb, curr_block);

        append = (block_size - (file->position % block_size));
        append = append > remain ? remain: append;

        memcpy((void*)((uint64_t)buf + buf_offset), (void*)((uint64_t)temp_buf + (file->position % block_size)), append);
        brelse(sb, curr_block);

        file->position += append;
        buf_offset += append;
        remain -= append;
    }
    return to_read;
}

int64_t EXT2Open(inode_t* inode, file_t* file) {
    return 0;
}