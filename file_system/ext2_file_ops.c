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
    uint64_t buf_offset = 0, append, remain = to_read, start_block = file->position / block_size;
    void* temp_buf;

    for (uint64_t i = start_block; i < (blocks_to_read + start_block) && remain > 0; i++) {
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

int64_t AddBlocksToInode(inode_t* inode, uint64_t count) {
    if (inode == NULL || count == 0) return -EINVAL;

    ext2_info_t* vol = (ext2_info_t*)inode->sb->fs_info;
    ext2_inode_data_t* data = (ext2_inode_data_t*)inode->fs_specific;

    if (vol->bgdt[data->block_group].free_blocks_count < count) return -ENOSPC;

    uint32_t new_block;
    for (uint64_t i = 0; i < count; i++) {
        new_block = EXT2AllocBlock(inode->sb, data->block_group);
        if (new_block == 0) return -ENOSPC;
        EXT2AddBlockToInode(inode, new_block);
    }
    return 0;
}

int64_t CheckSizeForWrite(file_t* file, uint64_t size) {
    if (file == NULL) return -EINVAL;
    if (file->inode == NULL || file->dentry == NULL) return -EINVAL;
    if (UINT64_MAX - size < file->position) return -EINVAL;

    uint64_t end_offset = file->position + size;
    uint32_t block_size = file->inode->sb->block_size;

    if (end_offset > file->inode->size) {
        int64_t truncate_ret = EXT2Truncate(file, end_offset);
        if (truncate_ret < 0) return truncate_ret;
    }
    return 0;
} 

int64_t EXT2Write(file_t* file, const void* buf, uint64_t size) {
    if (file == NULL || buf == NULL) return -EINVAL;
    if (file->inode == NULL || file->dentry == NULL) return -EINVAL;
    if (UINT64_MAX - size < file->position) return -EINVAL;

    int64_t blockscheck_ret = CheckSizeForWrite(file, size);
    if (blockscheck_ret < 0) return blockscheck_ret;

    superblock_t* sb = file->inode->sb;
    uint64_t block_size = sb->block_size;

    uint64_t start_block = file->position / block_size;
    uint64_t blocks_to_write = (size + block_size - 1) / block_size;
    uint64_t remain = size, buf_offset = 0;

    uint64_t curr_block, append;
    void* temp_buf;

    for (uint64_t i = start_block; i < (start_block + blocks_to_write); i++) {
        curr_block = FindDataBlock(file->inode, i);
        if (curr_block == 0) continue;
        temp_buf = bread(sb, curr_block);

        append = (block_size - (file->position % block_size));
        append = append > remain ? remain: append;

        memcpy((void*)((uint64_t)temp_buf + (file->position % block_size)), (void*)((uint64_t)buf + buf_offset), append);
        bwrite(sb, curr_block);
        brelse(sb, curr_block);

        file->position += append;
        buf_offset += append;
        remain -= append;
    }
    return 0;
}

uint64_t EXT2BlocksNeeded(uint64_t block_size, uint64_t current_size, uint64_t data_size) {
    uint64_t current_end = current_size + data_size;
    
    uint64_t blocks_after  = (current_end + block_size - 1) / block_size;
    uint64_t blocks_before = (current_size + block_size - 1) / block_size;
    
    return blocks_after - blocks_before;
}

uint64_t EXT2BlockToRemove(uint64_t block_size, uint64_t current_size, uint64_t rem_size) {
    if (rem_size > current_size) return 0;
    uint64_t current_end = current_size - rem_size;

    uint64_t blocks_after  = (current_end + block_size - 1) / block_size;
    uint64_t blocks_before = (current_size + block_size - 1) / block_size;

    return blocks_before - blocks_after;
}

int64_t EXT2ExpandFile(inode_t* inode, uint64_t add) {
    if (inode == NULL) return -EINVAL;
    if (add == 0) return 0;

    uint32_t block_size = inode->sb->block_size;
    uint64_t count_blocks = EXT2BlocksNeeded(block_size, inode->size, add);

    if (count_blocks == 0) {
        uint32_t last_block = FindDataBlock(inode, inode->size / block_size);
        if (last_block == 0) return -EIO;

        uint64_t buf = (uint64_t) bread(inode->sb, last_block);
        if (buf == NULL) return -EIO;

        memset((void*)(buf + inode->size % block_size), 0, add);
        bwrite(inode->sb, last_block);
        brelse(inode->sb, last_block);
        inode->size += add;
        inode->sb->ops->write_inode(inode);
        
        return 0;
    }
    AddBlocksToInode(inode, count_blocks);
    uint64_t start_block_idx = inode->size / block_size, end_block_idx = start_block_idx + count_blocks;
    uint64_t start, size, remain = add, buf;
    uint32_t curr_block;

    for (uint64_t i = start_block_idx; i < end_block_idx && remain > 0; i++) {
        start = inode->size % block_size;
        size = (block_size - start > remain) ? remain : block_size - start;

        curr_block = FindDataBlock(inode, i);
        if (curr_block == 0) continue;
        buf = (uint64_t) bread(inode->sb, curr_block);

        memset((void*)(buf + start), 0, size);
        bwrite(inode->sb, curr_block);
        brelse(inode->sb, curr_block);

        remain -= size;
        inode->size += size;
    }
    inode->sb->ops->write_inode(inode);

    return 0;
}

int64_t EXT2ShrinkFile(inode_t* inode, uint64_t sub) {
    if (inode == NULL || inode->fs_specific == NULL) return -EINVAL;
    if (sub == 0) return 0;
    if (sub > inode->size) return -EINVAL;

    uint64_t block_size = inode->sb->block_size;
    uint64_t new_size = inode->size - sub;
    uint64_t new_block_count = (new_size + block_size - 1) / block_size;
    uint64_t tail = new_size % block_size;

    if (tail != 0 && new_block_count > 0) {
        uint32_t last_block = FindDataBlock(inode, new_block_count - 1);
        if (last_block != 0) {
            uint8_t* buf = (uint8_t*) bread(inode->sb, last_block);
            if (buf == NULL) return -EIO;
            memset(buf + tail, 0, block_size - tail);
            bwrite(inode->sb, last_block);
            brelse(inode->sb, last_block);
        }
    }

    int64_t free_ret = EXT2FreeBlocksFrom(inode, new_block_count);
    if (free_ret < 0) return free_ret;

    inode->size = new_size;

    ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;
    total_time_t now;
    GetTotalTime(&now);
    data->modified_at = CalculateUnixTimestamp(&now);

    inode->sb->ops->write_inode(inode);
    return 0;
}

int64_t EXT2Truncate(file_t* file, uint64_t new_size) {
    if (file == NULL || file->inode == NULL || file->inode->fs_specific == NULL) return -EINVAL;

    inode_t* inode = file->inode;
    if (new_size == inode->size) return 0;

    if (inode->type == VFS_TYPE_SYMLINK && inode->size <= MAX_FAST_SYMLINK_LENGTH) {
        if (new_size > MAX_FAST_SYMLINK_LENGTH) return -EINVAL;

        ext2_inode_data_t* data = (ext2_inode_data_t*) inode->fs_specific;
        if (new_size < inode->size) {
            memset(((uint8_t*) data->i_block) + new_size, 0, inode->size - new_size);
        }
        inode->size = new_size;

        total_time_t now;
        GetTotalTime(&now);
        data->modified_at = CalculateUnixTimestamp(&now);

        inode->sb->ops->write_inode(inode);
        return 0;
    }

    if (new_size < inode->size) return EXT2ShrinkFile(inode, inode->size - new_size);
    return EXT2ExpandFile(inode, new_size - inode->size);
}

int64_t EXT2Open(inode_t* inode, file_t* file) {
    return 0;
}