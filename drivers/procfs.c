#include "procfs.h"
#include "vfs.h"
#include "scheduler.h"
#include "cpu_local.h"
#include "task_types.h"
#include "global.h"
#include "buddy_alloc.h"
#include "string.h"
#include "memory.h"
#include "slab_alloc.h"
#include "extern.h"
#include "timer.h"

// ---- Procfs internal types --------------------------------------------------
//
// Every procfs inode carries a procfs_inode_priv_t in its `fs_specific` slot.
// `kind` determines how lookup/readdir/read behave; `pid` is meaningful only
// for PID_DIR / PID_FILE; `entry_idx` indexes a static or per-pid entry table.

typedef enum {
    PROCFS_KIND_ROOT = 0,
    PROCFS_KIND_STATIC_FILE,
    PROCFS_KIND_PID_DIR,
    PROCFS_KIND_PID_FILE,
} procfs_kind_t;

typedef struct procfs_inode_priv {
    procfs_kind_t kind;
    uint64_t      pid;
    int64_t       entry_idx;   // index into the relevant static table; -1 = N/A
} procfs_inode_priv_t;

// File-side state. For regular files we generate the entire content into a
// kmalloc'd buffer at open time and stream it out of `read`. Directories
// reuse `cursor` as a readdir position. `buf_cap` is recorded so close()
// can kfree with the right size.
typedef struct procfs_file_priv {
    char*    buf;
    uint64_t len;
    uint64_t buf_cap;
    uint64_t cursor;
} procfs_file_priv_t;

// ---- Forward decls ----------------------------------------------------------

static int64_t  procfs_lookup       (inode_t* dir, dentry_t* d);
static int64_t  procfs_open         (inode_t* in, file_t* f);
static int64_t  procfs_close        (file_t* f);
static int64_t  procfs_read         (file_t* f, void* buf, uint64_t size);
static int64_t  procfs_seek         (file_t* f, int64_t off, int64_t whence);
static int64_t  procfs_readdir_root (file_t* f, dentry_t* out);
static int64_t  procfs_readdir_pid  (file_t* f, dentry_t* out);
static int64_t  procfs_readdir      (file_t* f, dentry_t* out);

static inode_t* procfs_alloc_inode  (superblock_t* sb);
static int64_t  procfs_free_inode   (inode_t* in);
static int64_t  procfs_read_inode   (inode_t* in);
static int64_t  procfs_write_inode  (inode_t* in);
static int64_t  procfs_sync         (superblock_t* sb);
static int64_t  procfs_unmount_op   (superblock_t* sb);

// Generators (defined further down).
static int64_t gen_uptime  (char* buf, uint64_t size, uint64_t pid);
static int64_t gen_meminfo (char* buf, uint64_t size, uint64_t pid);
static int64_t gen_cpuinfo (char* buf, uint64_t size, uint64_t pid);
static int64_t gen_version (char* buf, uint64_t size, uint64_t pid);
static int64_t gen_mounts  (char* buf, uint64_t size, uint64_t pid);
static int64_t gen_stat_kern(char* buf, uint64_t size, uint64_t pid);

static int64_t gen_pid_comm   (char* buf, uint64_t size, uint64_t pid);
static int64_t gen_pid_cmdline(char* buf, uint64_t size, uint64_t pid);
static int64_t gen_pid_stat   (char* buf, uint64_t size, uint64_t pid);
static int64_t gen_pid_status (char* buf, uint64_t size, uint64_t pid);

// ---- Entry tables -----------------------------------------------------------
//
// Static /proc entries (regular files in the procfs root). Order matters:
// `entry_idx` is an index into this array.

typedef int64_t (*procfs_gen_fn)(char* buf, uint64_t size, uint64_t pid);

typedef struct procfs_entry_t {
    const char*   name;
    procfs_gen_fn generate;
} procfs_entry_t;

static const procfs_entry_t static_entries[] = {
    { "uptime",  gen_uptime  },
    { "meminfo", gen_meminfo },
    { "cpuinfo", gen_cpuinfo },
    { "version", gen_version },
    { "mounts",  gen_mounts  },
    { "stat",    gen_stat_kern },
};
static const uint64_t static_entries_count =
    sizeof(static_entries) / sizeof(static_entries[0]);

static const procfs_entry_t pid_entries[] = {
    { "comm",    gen_pid_comm    },
    { "cmdline", gen_pid_cmdline },
    { "stat",    gen_pid_stat    },
    { "status",  gen_pid_status  },
};
static const uint64_t pid_entries_count =
    sizeof(pid_entries) / sizeof(pid_entries[0]);

// ---- Op vectors -------------------------------------------------------------

static superblock_ops_t procfs_sb_ops = {
    .alloc_inode = procfs_alloc_inode,
    .free_inode  = procfs_free_inode,
    .read_inode  = procfs_read_inode,
    .write_inode = procfs_write_inode,
    .mount       = NULL,
    .unmount     = procfs_unmount_op,
    .sync        = procfs_sync,
    .stat        = NULL,
};

static inode_ops_t procfs_inode_ops = {
    .lookup   = procfs_lookup,
    .create   = NULL,
    .mkdir    = NULL,
    .rmdir    = NULL,
    .unlink   = NULL,
    .rename   = NULL,
    .hardlink = NULL,
    .mknod    = NULL,
    .symlink  = NULL,
    .readlink = NULL,
    .getattr  = NULL,
    .setattr  = NULL,
};

static file_ops_t procfs_file_ops = {
    .read     = procfs_read,
    .write    = NULL,
    .seek     = procfs_seek,
    .truncate = NULL,
    .readdir  = procfs_readdir,
    .open     = procfs_open,
    .close    = procfs_close,
    .flush    = NULL,
    .ioctl    = NULL,
};

// ---- Helpers ----------------------------------------------------------------

static bool is_all_digits(const char* s) {
    if (s == NULL || *s == '\0') return false;
    for (const char* p = s; *p; p++) {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

static uint64_t parse_u64(const char* s) {
    uint64_t v = 0;
    for (const char* p = s; *p; p++) {
        v = v * 10 + (uint64_t)(*p - '0');
    }
    return v;
}

static int64_t find_static_idx(const char* name) {
    for (uint64_t i = 0; i < static_entries_count; i++) {
        if (strcmp((char*)name, (char*)static_entries[i].name) == 0) return (int64_t)i;
    }
    return -1;
}

static int64_t find_pid_idx(const char* name) {
    for (uint64_t i = 0; i < pid_entries_count; i++) {
        if (strcmp((char*)name, (char*)pid_entries[i].name) == 0) return (int64_t)i;
    }
    return -1;
}

// Build a fresh inode bound to procfs. Caller fills in kind/pid/entry_idx in
// the returned priv pointer (returned via the inode's fs_specific). Does not
// link the inode anywhere — just allocates and wires ops.
static inode_t* make_inode(superblock_t* sb, uint64_t type, uint64_t perm,
                           procfs_kind_t kind, uint64_t pid, int64_t entry_idx) {
    inode_t* in = (inode_t*) kmalloc(sizeof(inode_t));
    if (in == NULL) return NULL;
    memset(in, 0, sizeof(inode_t));

    procfs_inode_priv_t* p = (procfs_inode_priv_t*) kmalloc(sizeof(procfs_inode_priv_t));
    if (p == NULL) {
        kfree(in, sizeof(inode_t));
        return NULL;
    }
    p->kind = kind;
    p->pid = pid;
    p->entry_idx = entry_idx;

    in->sb = sb;
    in->ops = &procfs_inode_ops;
    in->file_ops = &procfs_file_ops;
    in->type = type;
    in->permissions = perm;
    in->fs_specific = p;
    in->size = 0;
    in->flags = S_IMMUTABLE;   // /proc is read-only end-to-end
    return in;
}

// ---- Superblock ops ---------------------------------------------------------

static inode_t* procfs_alloc_inode(superblock_t* sb) {
    // Default-allocated inode used by readdir's per-entry temp slot. The real
    // inode wiring happens in lookup; readdir later replaces this stub via
    // its own make_inode call. We still allocate fs_specific so vfs_iput's
    // free_inode path doesn't deref NULL.
    return make_inode(sb, VFS_TYPE_FILE, 0444, PROCFS_KIND_STATIC_FILE, 0, -1);
}

static int64_t procfs_free_inode(inode_t* in) {
    if (in == NULL) return -EINVAL;
    if (in->fs_specific != NULL) {
        kfree(in->fs_specific, sizeof(procfs_inode_priv_t));
    }
    kfree(in, sizeof(inode_t));
    return 0;
}

static int64_t procfs_read_inode(inode_t* in)  { (void)in; return 0; }
static int64_t procfs_write_inode(inode_t* in) { (void)in; return 0; }
static int64_t procfs_sync(superblock_t* sb)   { (void)sb; return 0; }

static int64_t procfs_unmount_op(superblock_t* sb) {
    if (sb == NULL) return -EINVAL;
    if (sb->root_inode != NULL) procfs_free_inode(sb->root_inode);
    kfree(sb, sizeof(superblock_t));
    return 0;
}

// ---- Lookup -----------------------------------------------------------------

static int64_t procfs_lookup(inode_t* dir, dentry_t* d) {
    if (dir == NULL || d == NULL || d->name == NULL) return -EINVAL;
    procfs_inode_priv_t* dp = (procfs_inode_priv_t*) dir->fs_specific;
    if (dp == NULL) return -EINVAL;

    if (dp->kind == PROCFS_KIND_ROOT) {
        // Static file?
        int64_t idx = find_static_idx(d->name);
        if (idx >= 0) {
            inode_t* in = make_inode(dir->sb, VFS_TYPE_FILE, 0444,
                                     PROCFS_KIND_STATIC_FILE, 0, idx);
            if (in == NULL) return -ENOMEM;
            d->inode = in;
            return 0;
        }
        // Numeric pid → check it exists, build a directory inode.
        if (is_all_digits(d->name)) {
            uint64_t pid = parse_u64(d->name);
            if (task_for_pid(pid) == NULL) return -ENOENT;
            inode_t* in = make_inode(dir->sb, VFS_TYPE_DIR, 0555,
                                     PROCFS_KIND_PID_DIR, pid, -1);
            if (in == NULL) return -ENOMEM;
            d->inode = in;
            return 0;
        }
        return -ENOENT;
    }

    if (dp->kind == PROCFS_KIND_PID_DIR) {
        int64_t idx = find_pid_idx(d->name);
        if (idx < 0) return -ENOENT;
        // Don't fail if the task already exited — generators handle that.
        inode_t* in = make_inode(dir->sb, VFS_TYPE_FILE, 0444,
                                 PROCFS_KIND_PID_FILE, dp->pid, idx);
        if (in == NULL) return -ENOMEM;
        d->inode = in;
        return 0;
    }

    return -ENOTDIR;
}

// ---- File ops ---------------------------------------------------------------
//
// Generated content is materialized once at open and freed at close. This
// keeps the read(2) implementation a trivial memcpy and matches the way
// Linux buffers small /proc files via single_open()'s seq_file backing.

#define PROCFS_BUF_SIZE 4096

static int64_t procfs_open(inode_t* in, file_t* f) {
    if (in == NULL || f == NULL) return -EINVAL;
    procfs_inode_priv_t* p = (procfs_inode_priv_t*) in->fs_specific;
    if (p == NULL) return -EINVAL;

    procfs_file_priv_t* fp = (procfs_file_priv_t*) kmalloc(sizeof(procfs_file_priv_t));
    if (fp == NULL) return -ENOMEM;
    fp->buf = NULL;
    fp->len = 0;
    fp->buf_cap = 0;
    fp->cursor = 0;

    if (in->type == VFS_TYPE_FILE) {
        fp->buf = (char*) kmalloc(PROCFS_BUF_SIZE);
        if (fp->buf == NULL) {
            kfree(fp, sizeof(procfs_file_priv_t));
            return -ENOMEM;
        }
        fp->buf_cap = PROCFS_BUF_SIZE;

        procfs_gen_fn gen = NULL;
        if (p->kind == PROCFS_KIND_STATIC_FILE && p->entry_idx >= 0 &&
            (uint64_t)p->entry_idx < static_entries_count) {
            gen = static_entries[p->entry_idx].generate;
        } else if (p->kind == PROCFS_KIND_PID_FILE && p->entry_idx >= 0 &&
                   (uint64_t)p->entry_idx < pid_entries_count) {
            gen = pid_entries[p->entry_idx].generate;
        }
        int64_t r = (gen != NULL) ? gen(fp->buf, fp->buf_cap, p->pid) : 0;
        if (r < 0) {
            kfree(fp->buf, fp->buf_cap);
            kfree(fp, sizeof(procfs_file_priv_t));
            return r;
        }
        if ((uint64_t)r > fp->buf_cap) r = (int64_t)fp->buf_cap;
        fp->len = (uint64_t)r;
        in->size = fp->len;
    }

    f->private_data = fp;
    return 0;
}

static int64_t procfs_close(file_t* f) {
    if (f == NULL) return -EINVAL;
    procfs_file_priv_t* fp = (procfs_file_priv_t*) f->private_data;
    if (fp == NULL) return 0;
    if (fp->buf != NULL) kfree(fp->buf, fp->buf_cap);
    kfree(fp, sizeof(procfs_file_priv_t));
    f->private_data = NULL;
    return 0;
}

static int64_t procfs_read(file_t* f, void* buf, uint64_t size) {
    if (f == NULL || buf == NULL) return -EINVAL;
    procfs_file_priv_t* fp = (procfs_file_priv_t*) f->private_data;
    if (fp == NULL) return -EINVAL;
    if (f->position >= fp->len) return 0;

    uint64_t avail = fp->len - f->position;
    uint64_t n = (size < avail) ? size : avail;
    memcpy(buf, fp->buf + f->position, n);
    f->position += n;
    return (int64_t)n;
}

static int64_t procfs_seek(file_t* f, int64_t off, int64_t whence) {
    if (f == NULL) return -EINVAL;
    procfs_file_priv_t* fp = (procfs_file_priv_t*) f->private_data;
    int64_t base;
    switch (whence) {
        case SEEK_SET: base = 0; break;
        case SEEK_CUR: base = (int64_t) f->position; break;
        case SEEK_END: base = (fp != NULL) ? (int64_t)fp->len : 0; break;
        default: return -EINVAL;
    }
    int64_t np = base + off;
    if (np < 0) return -EINVAL;
    f->position = (uint64_t)np;
    return np;
}

// ---- readdir ----------------------------------------------------------------
//
// vfs_iterate calls readdir repeatedly with a fresh out dentry until 0 is
// returned. We use file_t.position as a monotonically increasing cursor.
//
// Root layout: cursor 0..static_entries_count-1 enumerate static entries, then
// cursor N+ walks all pids known to the scheduler. For per-pid dirs the cursor
// is just an index into pid_entries.

static int64_t emit_dir_entry(file_t* f, dentry_t* out, const char* name,
                              uint64_t type, procfs_kind_t kind, uint64_t pid,
                              int64_t entry_idx) {
    out->name = vfs_strdup(name);
    if (out->name == NULL) return -ENOMEM;
    inode_t* in = make_inode(f->inode->sb,
                             type,
                             type == VFS_TYPE_DIR ? 0555 : 0444,
                             kind, pid, entry_idx);
    if (in == NULL) {
        vfs_strfree(out->name);
        out->name = NULL;
        return -ENOMEM;
    }
    out->inode = in;
    return (int64_t) strlen(name);
}

// Iterate every running/runnable task on every CPU, calling `cb(pid, ctx)`.
// cb returns nonzero to stop the walk early. Snapshot semantics — pids may
// appear/disappear between calls.
typedef int (*pid_visit_cb)(uint64_t pid, void* ctx);

static int walk_pids(pid_visit_cb cb, void* ctx) {
    uint64_t online = cpus_active ? cpus_active : 1;
    for (uint64_t i = 0; i < online; i++) {
        cpu_local_t* cpu = &cpu_locals[i];
        task_t* cur = cpu->current;
        if (cur != NULL) {
            int r = cb(cur->pid, ctx);
            if (r) return r;
        }
        run_queue_t* rq = &cpu->rq;
        spin_lock(&rq->lock);
        for (task_t* t = rq->head; t; t = t->next) {
            int r = cb(t->pid, ctx);
            if (r) { spin_unlock(&rq->lock); return r; }
        }
        spin_unlock(&rq->lock);
    }
    return 0;
}

// Find the Nth pid in walk order. Used to translate readdir cursor→pid.
typedef struct {
    uint64_t want;     // input: index requested
    uint64_t seen;     // running tally
    uint64_t out_pid;  // result
    bool     found;
} nth_pid_state_t;

static int nth_pid_cb(uint64_t pid, void* ctx) {
    nth_pid_state_t* s = (nth_pid_state_t*) ctx;
    if (s->seen == s->want) {
        s->out_pid = pid;
        s->found = true;
        return 1;  // stop
    }
    s->seen++;
    return 0;
}

static int64_t procfs_readdir_root(file_t* f, dentry_t* out) {
    uint64_t cursor = f->position;

    if (cursor < static_entries_count) {
        f->position = cursor + 1;
        return emit_dir_entry(f, out, static_entries[cursor].name, VFS_TYPE_FILE,
                              PROCFS_KIND_STATIC_FILE, 0, (int64_t)cursor);
    }

    nth_pid_state_t s = { .want = cursor - static_entries_count, .seen = 0,
                          .out_pid = 0, .found = false };
    walk_pids(nth_pid_cb, &s);
    if (!s.found) return 0;  // EOD

    char namebuf[24];
    ksnprintf(namebuf, sizeof(namebuf), "%lu", s.out_pid);
    f->position = cursor + 1;
    return emit_dir_entry(f, out, namebuf, VFS_TYPE_DIR,
                          PROCFS_KIND_PID_DIR, s.out_pid, -1);
}

static int64_t procfs_readdir_pid(file_t* f, dentry_t* out) {
    procfs_inode_priv_t* p = (procfs_inode_priv_t*) f->inode->fs_specific;
    uint64_t cursor = f->position;
    if (cursor >= pid_entries_count) return 0;

    f->position = cursor + 1;
    return emit_dir_entry(f, out, pid_entries[cursor].name, VFS_TYPE_FILE,
                          PROCFS_KIND_PID_FILE, p->pid, (int64_t)cursor);
}

static int64_t procfs_readdir(file_t* f, dentry_t* out) {
    if (f == NULL || out == NULL) return -EINVAL;
    procfs_inode_priv_t* p = (procfs_inode_priv_t*) f->inode->fs_specific;
    if (p == NULL) return -EINVAL;
    if (p->kind == PROCFS_KIND_ROOT)    return procfs_readdir_root(f, out);
    if (p->kind == PROCFS_KIND_PID_DIR) return procfs_readdir_pid(f, out);
    return -ENOTDIR;
}

// ---- Generators -------------------------------------------------------------

static const char* state_str(task_state_t s) {
    switch (s) {
        case TASK_STATE_NEW:     return "N";
        case TASK_STATE_READY:   return "R";
        case TASK_STATE_RUNNING: return "R";
        case TASK_STATE_BLOCKED: return "S";
        case TASK_STATE_ZOMBIE:  return "Z";
    }
    return "?";
}

static int64_t gen_uptime(char* buf, uint64_t size, uint64_t pid) {
    (void)pid;
    // timer_ticks[0] is incremented at TIMER_TICK_PER_MS rate (currently 1/ms),
    // so the BSP tick count is millisecond-granular. Output seconds.fractional.
    uint64_t ms = timer_ticks[0];
    uint64_t sec  = ms / 1000;
    uint64_t frac = (ms % 1000) / 10;  // centiseconds
    return ksnprintf(buf, size, "%lu.%lu\n", sec, frac);
}

static int64_t gen_meminfo(char* buf, uint64_t size, uint64_t pid) {
    (void)pid;
    uint64_t k_total = buddy_kernel_total_pages() * (PAGE_SIZE / 1024);
    uint64_t k_free  = buddy_kernel_free_pages()  * (PAGE_SIZE / 1024);
    uint64_t u_total = buddy_user_total_pages()   * (PAGE_SIZE / 1024);
    uint64_t u_free  = buddy_user_free_pages()    * (PAGE_SIZE / 1024);
    return ksnprintf(buf, size,
        "MemTotal:       %lu kB\n"
        "MemFree:        %lu kB\n"
        "KernelTotal:    %lu kB\n"
        "KernelFree:     %lu kB\n"
        "UserTotal:      %lu kB\n"
        "UserFree:       %lu kB\n",
        u_total + k_total, u_free + k_free,
        k_total, k_free, u_total, u_free);
}

// CPUID brand string lives in leaves 0x80000002..0x80000004 (4 leaves × 16
// bytes = 48-byte NUL-terminated string). Read once on demand.
static void read_cpu_brand(char out[49]) {
    uint32_t regs[12];
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t leaf = 0x80000002u + i;
        uint32_t a, b, c, d;
        __asm__ volatile ("cpuid"
            : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
            : "a"(leaf));
        regs[i*4 + 0] = a;
        regs[i*4 + 1] = b;
        regs[i*4 + 2] = c;
        regs[i*4 + 3] = d;
    }
    memcpy(out, regs, 48);
    out[48] = '\0';
}

static int64_t gen_cpuinfo(char* buf, uint64_t size, uint64_t pid) {
    (void)pid;
    char brand[49];
    read_cpu_brand(brand);

    uint64_t total = 0;
    uint64_t online = cpus_active ? cpus_active : 1;
    for (uint64_t i = 0; i < online; i++) {
        int64_t r = ksnprintf(buf + total,
                              (total < size) ? size - total : 0,
                              "processor\t: %lu\n"
                              "apic_id\t\t: %u\n"
                              "model name\t: %s\n\n",
                              i,
                              (uint64_t)cpu_locals[i].apic_id,
                              brand);
        if (r < 0) return r;
        total += (uint64_t)r;
    }
    return (int64_t)total;
}

static int64_t gen_version(char* buf, uint64_t size, uint64_t pid) {
    (void)pid;
    return ksnprintf(buf, size,
        "TempleOS kernel x86_64 (built with x86_64-elf-gcc)\n");
}

// Walk the dentry tree from vfs_root looking for nodes with a mounted
// filesystem attached. The current implementation has no list of mounts,
// so we walk the dentry children to find them. For now we know exactly two
// filesystems are mounted (ext2 root + procfs at /proc), so we hardcode the
// known ones plus walk children.
static int64_t gen_mounts(char* buf, uint64_t size, uint64_t pid) {
    (void)pid;
    uint64_t total = 0;

    // Root filesystem.
    int64_t r = ksnprintf(buf + total,
                          (total < size) ? size - total : 0,
                          "rootfs / ext2 ro 0 0\n");
    if (r < 0) return r;
    total += (uint64_t)r;

    // Walk vfs_root's child chain for any mount points.
    if (vfs_root != NULL) {
        for (dentry_t* c = vfs_root->children; c != NULL; c = c->next) {
            if (c->mount_type == MOUNT_FILESYSTEM && c->mount_dentry != NULL) {
                const char* fstype =
                    (c->mount_dentry->inode != NULL &&
                     c->mount_dentry->inode->sb != NULL &&
                     c->mount_dentry->inode->sb->ops == &procfs_sb_ops) ?
                    "proc" : "unknown";
                r = ksnprintf(buf + total,
                              (total < size) ? size - total : 0,
                              "%s /%s %s ro 0 0\n",
                              fstype, c->name ? c->name : "?", fstype);
                if (r < 0) return r;
                total += (uint64_t)r;
            }
        }
    }
    return (int64_t)total;
}

static int64_t gen_stat_kern(char* buf, uint64_t size, uint64_t pid) {
    (void)pid;
    uint64_t total = 0;
    uint64_t online = cpus_active ? cpus_active : 1;

    // cpu line: aggregate ticks across all CPUs. Linux prints user/nice/
    // system/idle/iowait/irq/softirq — we only track total ticks, so put
    // them in "system" and zero the rest.
    uint64_t agg = 0;
    for (uint64_t i = 0; i < online; i++) agg += timer_ticks[i];
    int64_t r = ksnprintf(buf + total,
                          (total < size) ? size - total : 0,
                          "cpu  0 0 %lu 0 0 0 0\n", agg);
    if (r < 0) return r;
    total += (uint64_t)r;

    for (uint64_t i = 0; i < online; i++) {
        r = ksnprintf(buf + total,
                      (total < size) ? size - total : 0,
                      "cpu%lu 0 0 %lu 0 0 0 0\n", i, timer_ticks[i]);
        if (r < 0) return r;
        total += (uint64_t)r;
    }

    r = ksnprintf(buf + total,
                  (total < size) ? size - total : 0,
                  "btime 0\n"
                  "processes 0\n"
                  "procs_running %lu\n",
                  online);
    if (r < 0) return r;
    total += (uint64_t)r;

    return (int64_t)total;
}

// ---- Per-PID generators ----

static int64_t gen_pid_comm(char* buf, uint64_t size, uint64_t pid) {
    task_t* t = task_for_pid(pid);
    if (t == NULL) return ksnprintf(buf, size, "(exited)\n");
    return ksnprintf(buf, size, "%s\n", t->name);
}

static int64_t gen_pid_cmdline(char* buf, uint64_t size, uint64_t pid) {
    task_t* t = task_for_pid(pid);
    if (t == NULL) return 0;
    // No argv tracking yet — emit name as the lone arg, NUL-terminated like
    // Linux's /proc/<pid>/cmdline.
    int64_t r = ksnprintf(buf, size, "%s", t->name);
    if (r < 0) return r;
    if ((uint64_t)r + 1 < size) {
        buf[r] = '\0';
        return r + 1;
    }
    return r;
}

static int64_t gen_pid_stat(char* buf, uint64_t size, uint64_t pid) {
    task_t* t = task_for_pid(pid);
    if (t == NULL) return 0;
    uint64_t ppid = (t->parent != NULL) ? t->parent->pid : 0;
    // Linux /proc/<pid>/stat is space-separated: pid (comm) state ppid pgrp
    // session tty_nr tpgid flags ... — fill the fields we don't track with 0.
    return ksnprintf(buf, size,
        "%lu (%s) %s %lu 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n",
        t->pid, t->name, state_str(t->state), ppid);
}

static int64_t gen_pid_status(char* buf, uint64_t size, uint64_t pid) {
    task_t* t = task_for_pid(pid);
    if (t == NULL) return 0;
    uint64_t ppid = (t->parent != NULL) ? t->parent->pid : 0;
    return ksnprintf(buf, size,
        "Name:\t%s\n"
        "State:\t%s\n"
        "Pid:\t%lu\n"
        "PPid:\t%lu\n"
        "CPU:\t%u\n"
        "Threads:\t1\n",
        t->name, state_str(t->state), t->pid, ppid, t->home_cpu);
}

// ---- Public API -------------------------------------------------------------

void procfs_init(void) {
    // Nothing to set up beyond op-vector statics; kept for symmetry with
    // devfs_init() and to give callers a stable hook for future state.
}

superblock_t* procfs_create_sb(void) {
    superblock_t* sb = (superblock_t*) kmalloc(sizeof(superblock_t));
    if (sb == NULL) return NULL;
    memset(sb, 0, sizeof(superblock_t));
    sb->magic = 0x50524F43;  // 'PROC'
    sb->block_size = PAGE_SIZE;
    sb->ops = &procfs_sb_ops;

    inode_t* root = make_inode(sb, VFS_TYPE_DIR, 0555,
                               PROCFS_KIND_ROOT, 0, -1);
    if (root == NULL) {
        kfree(sb, sizeof(superblock_t));
        return NULL;
    }
    sb->root_inode = root;
    return sb;
}
