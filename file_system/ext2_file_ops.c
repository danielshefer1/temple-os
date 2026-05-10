#include "ext2_file_ops.h"

static int64_t EXT2TruncateInner(file_t* file, uint64_t new_size);
static int64_t EXT2ReadDirInner(file_t* file, dentry_t* out);

int64_t EXT2Read(file_t* file, void* buf, uint64_t size) {
    if (file == NULL || buf == NULL) return -EINVAL;
    if (file->inode == NULL) return -EINVAL;
    if (UINT64_MAX - size < file->position) return -EINVAL;


    superblock_t* sb = file->inode->sb;
    int64_t io_ret = fs_io_begin(sb);
    if (io_ret < 0) return io_ret;
    uint64_t end_offset = file->position + size;
    uint64_t clamped    = end_offset < file->inode->size ? end_offset : file->inode->size;
    uint64_t to_read    = clamped - file->position;
    uint32_t block_size = sb->block_size;
    uint64_t start_block = file->position / block_size;
    uint64_t blocks_to_read = (to_read + (file->position % block_size) + block_size - 1) / block_size;

    uint32_t curr_block;
    uint64_t buf_offset = 0, append, remain = to_read;
    void* temp_buf;

    for (uint64_t i = start_block; i < (blocks_to_read + start_block) && remain > 0; i++) {
        append = (block_size - (file->position % block_size));
        append = append > remain ? remain : append;

        curr_block = FindDataBlock(file->inode, i);
        if (curr_block == 0) {
            memset((void*)((uint64_t)buf + buf_offset), 0, append);
        } else {
            temp_buf = bread(sb, curr_block);
            if (temp_buf == NULL) { fs_io_end(sb); return -EIO; }

            memcpy((void*)((uint64_t)buf + buf_offset), (void*)((uint64_t)temp_buf + (file->position % block_size)), append);
            brelse(sb, curr_block);
        }

        file->position += append;
        buf_offset += append;
        remain -= append;
    }
    fs_io_end(sb);
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
    if (file->inode == NULL) return -EINVAL;
    if (UINT64_MAX - size < file->position) return -EINVAL;

    uint64_t end_offset = file->position + size;

    if (end_offset > file->inode->size) {
        int64_t truncate_ret = EXT2TruncateInner(file, end_offset);
        if (truncate_ret < 0) return truncate_ret;
    }
    return 0;
} 

int64_t EXT2Write(file_t* file, const void* buf, uint64_t size) {
    if (file == NULL || buf == NULL) return -EINVAL;
    if (file->inode == NULL) return -EINVAL;
    if (UINT64_MAX - size < file->position) return -EINVAL;

    superblock_t* sb = file->inode->sb;
    int64_t io_ret = fs_io_begin(sb);
    if (io_ret < 0) return io_ret;

    int64_t blockscheck_ret = CheckSizeForWrite(file, size);
    if (blockscheck_ret < 0) { fs_io_end(sb); return blockscheck_ret; }

    uint64_t block_size = sb->block_size;

    uint64_t start_block = file->position / block_size;
    uint64_t blocks_to_write = (size + (file->position % block_size) + block_size - 1) / block_size;
    uint64_t remain = size, buf_offset = 0;

    uint64_t curr_block, append;
    void* temp_buf;
    ext2_private_file_t* private = (ext2_private_file_t*) file->private_data;
    u64_node_t* new_cache;

    for (uint64_t i = start_block; i < (start_block + blocks_to_write) && remain > 0; i++) {
        append = (block_size - (file->position % block_size));
        append = append > remain ? remain : append;

        curr_block = FindDataBlock(file->inode, i);
        if (curr_block == 0) {
            file->position += append;
            buf_offset += append;
            remain -= append;
            continue;
        }

        new_cache = (u64_node_t*) kmalloc(sizeof(u64_node_t));
        if (new_cache != NULL) {
            new_cache->value = curr_block;
            new_cache->next = private->cached_blocks;
            private->cached_blocks = new_cache;
        }

        temp_buf = bread(sb, curr_block);
        if (temp_buf == NULL) { fs_io_end(sb); return -EIO; }

        memcpy((void*)((uint64_t)temp_buf + (file->position % block_size)), (void*)((uint64_t)buf + buf_offset), append);
        bwrite(sb, curr_block);

        file->position += append;
        buf_offset += append;
        remain -= append;
    }
    fs_io_end(sb);
    return (int64_t)(size - remain);
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
        if (buf == 0) return -EIO;

        memset((void*)(buf + inode->size % block_size), 0, add);
        bwrite(inode->sb, last_block);
        brelse(inode->sb, last_block);
        inode->size += add;
        inode->sb->ops->write_inode(inode);
        
        return 0;
    }
    int64_t add_ret = AddBlocksToInode(inode, count_blocks);
    if (add_ret < 0) return add_ret;
    uint64_t start_block_idx = inode->size / block_size;
    // end_block_idx must cover both the previously-last partial block (when
    // inode->size isn't block-aligned) and every newly allocated block, so we
    // derive it from the target end offset rather than count_blocks.
    uint64_t end_block_idx = (inode->size + add + block_size - 1) / block_size;
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

static int64_t EXT2TruncateInner(file_t* file, uint64_t new_size) {
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

int64_t EXT2Truncate(file_t* file, uint64_t new_size) {
    if (file == NULL || file->inode == NULL) return -EINVAL;
    superblock_t* sb = file->inode->sb;
    int64_t io_ret = fs_io_begin(sb);
    if (io_ret < 0) return io_ret;
    int64_t r = EXT2TruncateInner(file, new_size);
    fs_io_end(sb);
    return r;
}

static int64_t EXT2ReadDirInner(file_t* file, dentry_t* out) {
    if (file == NULL || out == NULL) return -EINVAL;
    superblock_t* sb = file->inode->sb;
    ext2_private_file_t* dir_pos = (ext2_private_file_t*) file->private_data;

    if (dir_pos->dir_blocks_offset * sb->block_size + dir_pos->dir_bytes_offset >= file->inode->size) return 0;

    uint32_t block_to_read = FindDataBlock(file->inode, dir_pos->dir_blocks_offset);
    uint64_t buf = (uint64_t) bread(sb, block_to_read);
    ext2_dir_entry_t* entry = (ext2_dir_entry_t*)(buf + dir_pos->dir_bytes_offset);
    if (entry->inode == 0) {
        dir_pos->dir_bytes_offset += entry->rec_len;

        if (dir_pos->dir_bytes_offset < sb->block_size) {
            brelse(sb, block_to_read);
            return EXT2ReadDirInner(file, out);
        }

        dir_pos->dir_bytes_offset = 0;
        dir_pos->dir_blocks_offset++;
        brelse(sb, block_to_read);
        return EXT2ReadDirInner(file, out); 
    }

    out->name = (char*) kmalloc(entry->name_len + 1);

    memcpy(out->name, entry->name, entry->name_len);
    out->name[entry->name_len] = '\0';

    out->inode = sb->ops->alloc_inode(sb);
    ext2_inode_data_t* data = (ext2_inode_data_t*) out->inode->fs_specific;

    data->inode_number = entry->inode;
    int64_t read_ret = sb->ops->read_inode(out->inode);
    if (read_ret < 0) {
        kfree(out->name, entry->name_len + 1);
        brelse(sb, block_to_read);
        return read_ret;
    };


    dir_pos->dir_bytes_offset += entry->rec_len;
    if (dir_pos->dir_bytes_offset >= sb->block_size) {
        dir_pos->dir_bytes_offset = 0;
        dir_pos->dir_blocks_offset++;
    }
    uint32_t ret = entry->name_len;
    brelse(sb, block_to_read);
    return ret;
}

int64_t EXT2ReadDir(file_t* file, dentry_t* out) {
    if (file == NULL || file->inode == NULL) return -EINVAL;
    superblock_t* sb = file->inode->sb;
    int64_t io_ret = fs_io_begin(sb);
    if (io_ret < 0) return io_ret;
    int64_t r = EXT2ReadDirInner(file, out);
    fs_io_end(sb);
    return r;
}

int64_t EXT2Open(inode_t* inode, file_t* file) {
    (void)inode;
    void* buf = kmalloc(sizeof(ext2_private_file_t));
    if (buf == NULL) return -ENOMEM;
    file->private_data = buf;

    ext2_private_file_t* private = (ext2_private_file_t*) file->private_data;
    private->dir_blocks_offset = 0;
    private->dir_bytes_offset = 0;
    private->cached_blocks = NULL;
    return 0;
}

int64_t EXT2Close(file_t* file) {
    if (file == NULL) return -EINVAL;
    if (file->private_data == NULL) return 0;

    int64_t flush_ret = EXT2Flush(file);
    if (flush_ret < 0) return flush_ret;

    if (file->inode != NULL && file->inode->sb != NULL &&
        file->inode->sb->ops != NULL && file->inode->sb->ops->write_inode != NULL) {
        file->inode->sb->ops->write_inode(file->inode);
    }

    kfree(file->private_data, sizeof(ext2_private_file_t));
    file->private_data = NULL;
    return 0;
}

int64_t EXT2Ioctl(file_t* file, uint64_t cmd, void* arg) {
    if (file == NULL || file->inode == NULL) return -EINVAL;

    switch (cmd) {
        case EXT2_IOC_GET_INO: {
            if (arg == NULL) return -EINVAL;
            ext2_inode_data_t* data = (ext2_inode_data_t*) file->inode->fs_specific;
            if (data == NULL) return -EINVAL;
            *(uint32_t*)arg = data->inode_number;
            return 0;
        }
        case EXT2_IOC_GET_SIZE: {
            if (arg == NULL) return -EINVAL;
            *(uint64_t*)arg = file->inode->size;
            return 0;
        }
        case EXT2_IOC_GET_BLOCK_SIZE: {
            if (arg == NULL || file->inode->sb == NULL) return -EINVAL;
            *(uint32_t*)arg = file->inode->sb->block_size;
            return 0;
        }
        case EXT2_IOC_FIBMAP: {
            if (arg == NULL) return -EINVAL;
            uint64_t logical = *(uint64_t*)arg;
            int64_t phys = FindDataBlock(file->inode, (uint32_t)logical);
            if (phys < 0) return phys;
            *(uint64_t*)arg = (uint64_t)phys;
            return 0;
        }
        case EXT2_IOC_SYNC_FILE: {
            int64_t flush_ret = EXT2Flush(file);
            if (flush_ret < 0) return flush_ret;
            if (file->inode->sb != NULL && file->inode->sb->ops != NULL &&
                file->inode->sb->ops->write_inode != NULL) {
                file->inode->sb->ops->write_inode(file->inode);
            }
            return 0;
        }
        default:
            return -ENOTTY;
    }
}

int64_t EXT2Flush(file_t* file) {
    if (file == NULL || file->private_data == NULL) return -EINVAL;

    ext2_private_file_t* private = (ext2_private_file_t*) file->private_data;
    u64_node_t* p = private->cached_blocks;
    u64_node_t* next;

    while (p != NULL) {
        bflush(file->inode->sb, p->value);
        brelse(file->inode->sb, p->value);

        next = p->next;
        kfree(p, sizeof(u64_node_t));
        p = next;
    }
    private->cached_blocks = NULL;

    return 0;
}

int64_t EXT2Seek(file_t* file, int64_t offset, int64_t whence) {
    if (file == NULL || file->inode == NULL) return -EINVAL;

    int64_t base;
    switch (whence) {
        case SEEK_SET: base = 0;                          break;
        case SEEK_CUR: base = (int64_t) file->position;   break;
        case SEEK_END: base = (int64_t) file->inode->size; break;
        default:       return -EINVAL;
    }

    int64_t new_pos = base + offset;
    if (new_pos < 0) return -EINVAL;

    file->position = (uint64_t) new_pos;
    return new_pos;
}

file_ops_t ext2_file_ops = {
    .read     = EXT2Read,
    .write    = EXT2Write,
    .seek     = EXT2Seek,
    .truncate = EXT2Truncate,
    .readdir  = EXT2ReadDir,
    .open     = EXT2Open,
    .close    = EXT2Close,
    .flush    = EXT2Flush,
    .ioctl    = EXT2Ioctl,
};