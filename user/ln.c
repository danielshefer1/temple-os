#include "std/std.h"

int main(int argc, char** argv) {
    // Only -s (symbolic) is supported — there's no hardlink syscall.
    if (argc != 4 || argv[1][0] != '-' || argv[1][1] != 's' || argv[1][2] != 0) {
        st_puts("usage: ln -s <target> <linkpath>\n");
        return 1;
    }
    long r = sys_symlink(argv[2], argv[3]);
    if (r < 0) {
        st_puts("ln: ");
        st_puts(argv[3]);
        st_puts(" -> ");
        st_puts(argv[2]);
        st_puts(": failed\n");
        return 1;
    }
    return 0;
}
