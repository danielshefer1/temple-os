#include "vfs.h"
#include "cpu_local.h"

// Pick the anchor for a path: absolute paths start at vfs_root; relative
// paths start at the current task's cwd, falling back to vfs_root if there
// is no current task (early-boot kernel callers) or no cwd set yet.
static dentry_t* path_anchor(const char* path) {
    if (path != NULL && path[0] == '/') return vfs_root;
    cpu_local_t* cpu = this_cpu();
    if (cpu == NULL || cpu->current == NULL) return vfs_root;
    dentry_t* cwd = cpu->current->cwd;
    return cwd ? cwd : vfs_root;
}

// Copy one path component starting at *p into out (cap bytes incl. NUL).
// Advances *p past the component and any trailing '/'. Returns component
// length, 0 on end-of-path, or -ENAMETOOLONG.
static int64_t next_component(const char** p, char* out, uint64_t cap) {
    const char* s = *p;
    while (*s == '/') s++;
    if (*s == '\0') { *p = s; return 0; }

    uint64_t n = 0;
    while (s[n] != '/' && s[n] != '\0') {
        if (n + 1 >= cap) return -ENAMETOOLONG;
        out[n] = s[n];
        n++;
    }
    out[n] = '\0';
    *p = s + n;
    return (int64_t) n;
}

static int64_t walk_inner(dentry_t* start, const char* path,
                          dentry_t** out, uint64_t depth);

// Resolve a single component against curr (handles ".", "..", and dcache lookup).
static dentry_t* step(dentry_t* curr, const char* comp) {
    if (strcmp((char*) comp, ".") == 0) return curr;
    if (strcmp((char*) comp, "..") == 0) return curr->parent ? curr->parent : curr;
    return vfs_dentry_get(curr, comp);
}

// Follow symlinks at curr; bounded depth. Returns the resolved non-symlink
// dentry, or NULL on error (which the caller turns into -ELOOP / -ENOENT).
static dentry_t* follow_symlinks(dentry_t* curr, uint64_t depth) {
    while (curr != NULL && curr->inode != NULL &&
           curr->inode->type == VFS_TYPE_SYMLINK) {
        if (depth >= VFS_SYMLINK_MAX) return NULL;

        char target[VFS_PATH_MAX];
        int64_t r = vfs_readlink(curr->inode, target, sizeof(target));
        if (r < 0) return NULL;
        uint64_t tlen = (uint64_t) r;
        if (tlen >= sizeof(target)) tlen = sizeof(target) - 1;
        target[tlen] = '\0';

        dentry_t* base = (target[0] == '/') ? vfs_root : curr->parent;
        dentry_t* next = NULL;
        if (walk_inner(base, target, &next, depth + 1) < 0) return NULL;
        curr = next;
    }
    return curr;
}

static int64_t walk_inner(dentry_t* start, const char* path,
                          dentry_t** out, uint64_t depth) {
    if (path == NULL || out == NULL) return -EINVAL;
    if (depth > VFS_SYMLINK_MAX) return -ELOOP;
    if (strlen(path) >= VFS_PATH_MAX) return -ENAMETOOLONG;

    dentry_t* curr = (path[0] == '/') ? vfs_root : start;
    if (curr == NULL) return -ENOENT;

    char comp[VFS_NAME_MAX + 1];
    const char* p = path;
    int64_t cn;
    while ((cn = next_component(&p, comp, sizeof(comp))) > 0) {
        curr = vfs_traverse_mount(curr);
        curr = step(curr, comp);
        if (curr == NULL) return -ENOENT;
        curr = follow_symlinks(curr, depth);
        if (curr == NULL) return -ELOOP;
    }
    if (cn < 0) return cn;

    *out = vfs_traverse_mount(curr);
    return 0;
}

int64_t vfs_path_walk(dentry_t* start, const char* rel, dentry_t** out) {
    if (start == NULL) start = vfs_root;
    return walk_inner(start, rel, out, 0);
}

int64_t vfs_namei(const char* path, dentry_t** out) {
    if (vfs_root == NULL) return -ENOENT;
    return walk_inner(path_anchor(path), path, out, 0);
}

int64_t vfs_namei_parent(const char* path,
                         dentry_t** parent_out,
                         char* leaf_out, uint64_t leaf_cap) {
    if (path == NULL || parent_out == NULL || leaf_out == NULL || leaf_cap == 0) return -EINVAL;
    uint64_t plen = strlen(path);
    if (plen == 0 || plen >= VFS_PATH_MAX) return -EINVAL;

    // find last '/' that has a non-empty component after it
    int64_t last_slash = -1;
    int64_t end = (int64_t) plen;
    while (end > 0 && path[end - 1] == '/') end--;     // strip trailing '/'
    if (end == 0) return -EINVAL;                       // path was all slashes

    for (int64_t i = end - 1; i >= 0; i--) {
        if (path[i] == '/') { last_slash = i; break; }
    }

    uint64_t leaf_len = (uint64_t)end - (uint64_t)(last_slash + 1);
    if (leaf_len == 0 || leaf_len > VFS_NAME_MAX) return -ENAMETOOLONG;
    if (leaf_len + 1 > leaf_cap) return -ENAMETOOLONG;
    memcpy(leaf_out, &path[last_slash + 1], leaf_len);
    leaf_out[leaf_len] = '\0';

    if (last_slash < 0) {
        // No slash at all: parent is wherever the path resolves from. For
        // a relative input that's the cwd; for "foo" with no cwd, vfs_root.
        dentry_t* anchor = path_anchor(path);
        if (anchor == NULL) return -ENOENT;
        *parent_out = anchor;
        return 0;
    }
    if (last_slash == 0) {
        if (vfs_root == NULL) return -ENOENT;
        *parent_out = vfs_root;
        return 0;
    }

    char parent_path[VFS_PATH_MAX];
    memcpy(parent_path, path, (uint64_t) last_slash);
    parent_path[last_slash] = '\0';
    return vfs_namei(parent_path, parent_out);
}
