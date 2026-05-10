#include "std/std.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        st_puts("usage: touch <path...>\n");
        return 1;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        long fd = sys_open(argv[i], O_WRONLY | O_CREAT, 0644);
        if (fd < 0) {
            st_puts("touch: ");
            st_puts(argv[i]);
            st_puts(": failed\n");
            rc = 1;
            continue;
        }
        sys_close(fd);
    }
    return rc;
}
