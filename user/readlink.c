#include "std/std.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        st_puts("usage: readlink <link>\n");
        return 1;
    }
    char buf[256];
    long n = sys_readlink(argv[1], buf, sizeof(buf) - 1);
    if (n < 0) {
        st_puts("readlink: ");
        st_puts(argv[1]);
        st_puts(": failed\n");
        return 1;
    }
    if (n >= (long)sizeof(buf)) n = sizeof(buf) - 1;
    buf[n] = 0;
    st_puts(buf);
    sys_write(1, "\n", 1);
    return 0;
}
