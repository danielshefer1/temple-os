#include "std/std.h"

#define YES_BUF_SZ 1024

int main(int argc, char** argv) {
    char buf[YES_BUF_SZ];
    unsigned long pos = 0;

    if (argc < 2) {
        // Default: "y\n" repeated.
        while (pos + 2 <= YES_BUF_SZ) {
            buf[pos++] = 'y';
            buf[pos++] = '\n';
        }
    } else {
        // Concatenate argv[1..] separated by spaces, then '\n'.
        // Pack as many copies as fit in the buffer for fewer syscalls.
        char unit[256];
        unsigned long ulen = 0;
        for (int i = 1; i < argc; i++) {
            if (i > 1) {
                if (ulen >= sizeof(unit) - 1) break;
                unit[ulen++] = ' ';
            }
            const char* s = argv[i];
            while (*s && ulen < sizeof(unit) - 1) unit[ulen++] = *s++;
        }
        if (ulen >= sizeof(unit) - 1) ulen = sizeof(unit) - 2;
        unit[ulen++] = '\n';

        while (pos + ulen <= YES_BUF_SZ) {
            for (unsigned long k = 0; k < ulen; k++) buf[pos + k] = unit[k];
            pos += ulen;
        }
        if (pos == 0) {
            // Single repeat doesn't fit a 1KB buffer — emit as-is each loop.
            for (;;) {
                long w = sys_write(1, unit, ulen);
                if (w <= 0) return 1;
            }
        }
    }

    for (;;) {
        long w = sys_write(1, buf, pos);
        if (w <= 0) return 1;
    }
}
