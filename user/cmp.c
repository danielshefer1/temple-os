#include "std/std.h"

static long fill(long fd, char* buf, long want) {
    long got = 0;
    while (got < want) {
        long n = sys_read(fd, buf + got, (unsigned long)(want - got));
        if (n <= 0) return got;
        got += n;
    }
    return got;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        st_puts("usage: cmp <a> <b>\n");
        return 2;
    }
    long fa = sys_open(argv[1], O_RDONLY, 0);
    if (fa < 0) {
        st_puts("cmp: ");
        st_puts(argv[1]);
        st_puts(": cannot open\n");
        return 2;
    }
    long fb = sys_open(argv[2], O_RDONLY, 0);
    if (fb < 0) {
        st_puts("cmp: ");
        st_puts(argv[2]);
        st_puts(": cannot open\n");
        sys_close(fa);
        return 2;
    }

    char ba[1024], bb[1024];
    unsigned long byte = 1;
    unsigned long line = 1;
    int rc = 0;

    for (;;) {
        long na = fill(fa, ba, sizeof(ba));
        long nb = fill(fb, bb, sizeof(bb));
        long m = (na < nb) ? na : nb;
        for (long i = 0; i < m; i++) {
            if (ba[i] != bb[i]) {
                st_puts(argv[1]);
                st_puts(" ");
                st_puts(argv[2]);
                st_puts(" differ: byte ");
                st_putn(byte);
                st_puts(", line ");
                st_putn(line);
                sys_write(1, "\n", 1);
                rc = 1;
                goto done;
            }
            if (ba[i] == '\n') line++;
            byte++;
        }
        if (na != nb) {
            // One side ran short; the other has at least one extra byte.
            st_puts("cmp: EOF on ");
            st_puts((na < nb) ? argv[1] : argv[2]);
            sys_write(1, "\n", 1);
            rc = 1;
            goto done;
        }
        if (na == 0) break;
    }

done:
    sys_close(fa);
    sys_close(fb);
    return rc;
}
