#include "std/std.h"

static long parse_ulong(const char* s) {
    long v = 0;
    int seen = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return -1;
        v = v * 10 + (*s - '0');
        seen = 1;
        s++;
    }
    return seen ? v : -1;
}

static int head_fd(long fd, long n_lines) {
    char buf[1024];
    long printed = 0;
    while (printed < n_lines) {
        long n = sys_read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        long start = 0;
        for (long i = 0; i < n; i++) {
            if (buf[i] == '\n') {
                sys_write(1, buf + start, (unsigned long)(i - start + 1));
                start = i + 1;
                printed++;
                if (printed >= n_lines) break;
            }
        }
        if (printed < n_lines && start < n) {
            sys_write(1, buf + start, (unsigned long)(n - start));
        }
    }
    return 0;
}

int main(int argc, char** argv) {
    long n_lines = 10;
    int i = 1;
    if (i < argc && argv[i][0] == '-' && argv[i][1] == 'n') {
        const char* num;
        if (argv[i][2]) {
            num = &argv[i][2];
            i++;
        } else if (i + 1 < argc) {
            num = argv[i + 1];
            i += 2;
        } else {
            st_puts("usage: head [-n N] [file...]\n");
            return 1;
        }
        long v = parse_ulong(num);
        if (v < 0) { st_puts("head: invalid count\n"); return 1; }
        n_lines = v;
    }

    if (i >= argc) {
        return head_fd(0, n_lines);
    }

    int rc = 0;
    int multi = (argc - i) > 1;
    for (int j = i; j < argc; j++) {
        long fd = sys_open(argv[j], O_RDONLY, 0);
        if (fd < 0) {
            st_puts("head: ");
            st_puts(argv[j]);
            st_puts(": cannot open\n");
            rc = 1;
            continue;
        }
        if (multi) {
            if (j > i) sys_write(1, "\n", 1);
            st_puts("==> ");
            st_puts(argv[j]);
            st_puts(" <==\n");
        }
        head_fd(fd, n_lines);
        sys_close(fd);
    }
    return rc;
}
