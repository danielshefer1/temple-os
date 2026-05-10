#include "std/std.h"

#define REV_LINE_MAX 4096

static void emit_reversed(const char* line, unsigned long len) {
    char out[REV_LINE_MAX];
    for (unsigned long i = 0; i < len; i++) out[i] = line[len - 1 - i];
    sys_write(1, out, len);
    sys_write(1, "\n", 1);
}

static void rev_fd(long fd) {
    static char line[REV_LINE_MAX];
    char buf[1024];
    unsigned long llen = 0;

    for (;;) {
        long n = sys_read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        for (long i = 0; i < n; i++) {
            char ch = buf[i];
            if (ch == '\n' || llen == REV_LINE_MAX) {
                emit_reversed(line, llen);
                llen = 0;
                if (ch != '\n') line[llen++] = ch;
            } else {
                line[llen++] = ch;
            }
        }
    }
    if (llen > 0) emit_reversed(line, llen);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        rev_fd(0);
        return 0;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        long fd = sys_open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            st_puts("rev: ");
            st_puts(argv[i]);
            st_puts(": cannot open\n");
            rc = 1;
            continue;
        }
        rev_fd(fd);
        sys_close(fd);
    }
    return rc;
}
