#include "std/std.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        st_puts("usage: mv <src> <dst>\n");
        return 1;
    }
    long r = sys_rename(argv[1], argv[2]);
    if (r < 0) {
        st_puts("mv: ");
        st_puts(argv[1]);
        st_puts(" -> ");
        st_puts(argv[2]);
        st_puts(": failed\n");
        return 1;
    }
    return 0;
}
