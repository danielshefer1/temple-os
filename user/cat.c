#include "std/std.h"

static int cat_one(const char* path) {
    long fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) {
        st_puts("cat: ");
        st_puts(path);
        st_puts(": cannot open\n");
        return 1;
    }
    char buf[256];
    for (;;) {
        long n = sys_read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        sys_write(1, buf, (unsigned long)n);
    }
    sys_close(fd);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        // No files given — read stdin and pass through. Lets `cat` work
        // as a sink in pipelines (e.g. `ls | cat`).
        char buf[256];
        for (;;) {
            long n = sys_read(0, buf, sizeof(buf));
            if (n <= 0) break;
            sys_write(1, buf, (unsigned long)n);
        }
        return 0;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (cat_one(argv[i]) != 0) rc = 1;
    }
    return rc;
}
