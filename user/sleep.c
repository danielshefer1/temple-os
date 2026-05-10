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

int main(int argc, char** argv) {
    if (argc != 2) {
        st_puts("usage: sleep <seconds>\n");
        return 1;
    }
    long secs = parse_ulong(argv[1]);
    if (secs < 0) {
        st_puts("sleep: invalid number\n");
        return 1;
    }
    sys_sleep((unsigned long)secs * 1000UL);
    return 0;
}
