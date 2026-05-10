#include "std/std.h"

static const char hex_tbl[] = "0123456789abcdef";

static void put_hex_n(unsigned long v, int width, char* dst) {
    for (int i = width - 1; i >= 0; i--) {
        dst[i] = hex_tbl[v & 0xF];
        v >>= 4;
    }
}

static void dump_fd(long fd) {
    unsigned char buf[16];
    char line[80];
    unsigned long offset = 0;

    for (;;) {
        long want = 16, got = 0;
        while (got < want) {
            long n = sys_read(fd, buf + got, (unsigned long)(want - got));
            if (n <= 0) break;
            got += n;
        }
        if (got == 0) break;

        // Layout: "OOOOOOOO: HH HH HH HH HH HH HH HH  HH HH HH HH HH HH HH HH  ASCII...."
        for (int i = 0; i < 80; i++) line[i] = ' ';
        put_hex_n(offset, 8, line);
        line[8] = ':';
        for (long i = 0; i < got; i++) {
            int col = 10 + (int)i * 3 + (i >= 8 ? 1 : 0);
            put_hex_n(buf[i], 2, line + col);
        }
        for (long i = 0; i < got; i++) {
            int col = 60 + (int)i;
            unsigned char c = buf[i];
            line[col] = (c >= 0x20 && c < 0x7F) ? (char)c : '.';
        }
        sys_write(1, line, 60 + (unsigned long)got);
        sys_write(1, "\n", 1);
        offset += (unsigned long)got;
        if (got < want) break;
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        dump_fd(0);
        return 0;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        long fd = sys_open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            st_puts("xxd: ");
            st_puts(argv[i]);
            st_puts(": cannot open\n");
            rc = 1;
            continue;
        }
        dump_fd(fd);
        sys_close(fd);
    }
    return rc;
}
