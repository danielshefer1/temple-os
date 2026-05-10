#include "std/std.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        st_puts("usage: rmdir <path>\n");
        return 1;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        long r = sys_rmdir(argv[i]);
        if (r < 0) {
            st_puts("rmdir: ");
            st_puts(argv[i]);
            st_puts(": failed\n");
            rc = 1;
        }
    }
    return rc;
}
