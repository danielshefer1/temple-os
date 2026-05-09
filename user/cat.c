#include "libu.h"

static int cat_one(const char* path) {
    long fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) {
        u_puts("cat: ");
        u_puts(path);
        u_puts(": cannot open\n");
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
        u_puts("usage: cat <file...>\n");
        return 1;
    }
    int rc = 0;
    for (int i = 1; i < argc; i++) {
        if (cat_one(argv[i]) != 0) rc = 1;
    }
    return rc;
}
