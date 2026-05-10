#include "std/std.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        st_puts("usage: cp <src> <dst>\n");
        return 1;
    }

    long src = sys_open(argv[1], O_RDONLY, 0);
    if (src < 0) {
        st_puts("cp: ");
        st_puts(argv[1]);
        st_puts(": cannot open\n");
        return 1;
    }
    long dst = sys_open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst < 0) {
        st_puts("cp: ");
        st_puts(argv[2]);
        st_puts(": cannot create\n");
        sys_close(src);
        return 1;
    }

    char buf[4096];
    int rc = 0;
    for (;;) {
        long n = sys_read(src, buf, sizeof(buf));
        if (n == 0) break;
        if (n < 0) { rc = 1; break; }
        long off = 0;
        while (off < n) {
            long w = sys_write(dst, buf + off, (unsigned long)(n - off));
            if (w <= 0) { rc = 1; break; }
            off += w;
        }
        if (rc) break;
    }
    sys_close(src);
    sys_close(dst);
    if (rc) st_puts("cp: I/O error\n");
    return rc;
}
