#include "std/std.h"

#define FIND_PATH_MAX 1024
#define FIND_DIRENT_BUF 2048
#define FIND_MAX_DEPTH 64

static char path_buf[FIND_PATH_MAX];

static void emit_path(unsigned long len) {
    sys_write(1, path_buf, len);
    sys_write(1, "\n", 1);
}

static void walk(unsigned long path_len, int depth) {
    path_buf[path_len] = 0;
    emit_path(path_len);
    if (depth >= FIND_MAX_DEPTH) return;

    long fd = sys_open(path_buf, O_RDONLY, 0);
    if (fd < 0) return;

    char dbuf[FIND_DIRENT_BUF];
    for (;;) {
        long n = sys_getdents(fd, dbuf, sizeof(dbuf));
        if (n <= 0) break;
        for (long off = 0; off < n;) {
            struct linux_dirent64* e = (struct linux_dirent64*)(dbuf + off);
            if (e->d_reclen == 0) { off = n; break; }
            const char* name = e->d_name;
            int skip = (name[0] == '.' &&
                        (name[1] == 0 || (name[1] == '.' && name[2] == 0)));
            if (!skip) {
                unsigned long nlen = st_strlen(name);
                int need_slash = !(path_len == 1 && path_buf[0] == '/');
                unsigned long new_len = path_len + (need_slash ? 1 : 0) + nlen;
                if (new_len < FIND_PATH_MAX - 1) {
                    unsigned long p = path_len;
                    if (need_slash) path_buf[p++] = '/';
                    st_memcpy(path_buf + p, name, nlen);
                    if (e->d_type == DT_DIR) {
                        walk(new_len, depth + 1);
                    } else {
                        path_buf[new_len] = 0;
                        emit_path(new_len);
                    }
                }
            }
            off += e->d_reclen;
        }
    }
    sys_close(fd);
}

int main(int argc, char** argv) {
    const char* root = (argc >= 2) ? argv[1] : ".";
    unsigned long len = st_strlen(root);
    if (len >= FIND_PATH_MAX - 1) {
        st_puts("find: path too long\n");
        return 1;
    }
    st_memcpy(path_buf, root, len);
    // Strip trailing slash unless the root *is* "/".
    while (len > 1 && path_buf[len - 1] == '/') len--;
    walk(len, 0);
    return 0;
}
