#include "std/std.h"

static long parse_long(const char* s) {
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

int main(int argc, char** argv) {
    if (argc != 3) {
        st_puts("usage: truncate <path> <length>\n");
        return 1;
    }
    long len = parse_long(argv[2]);
    if (len < 0) {
        st_puts("truncate: invalid length\n");
        return 1;
    }
    long r = sys_truncate(argv[1], len);
    if (r < 0) {
        st_puts("truncate: ");
        st_puts(argv[1]);
        st_puts(": failed\n");
        return 1;
    }
    return 0;
}
