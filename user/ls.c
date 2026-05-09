#include "libu.h"
#include "sys/dirent.h"

int main(int argc, char** argv) {
    const char* path = (argc >= 2) ? argv[1] : ".";
    long fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) {
        u_puts("ls: ");
        u_puts(path);
        u_puts(": cannot open\n");
        return 1;
    }

    char buf[1024];
    for (;;) {
        long n = sys_getdents(fd, buf, sizeof(buf));
        if (n <= 0) break;
        for (long off = 0; off < n;) {
            struct linux_dirent64* e = (struct linux_dirent64*)(buf + off);
            if (e->d_reclen == 0) { off = n; break; }
            // Skip "." and ".." for tidier output (toggle later if needed).
            const char* name = e->d_name;
            int hide = (name[0] == '.' &&
                        (name[1] == 0 || (name[1] == '.' && name[2] == 0)));
            if (!hide) {
                sys_write(1, name, u_strlen(name));
                if (e->d_type == DT_DIR) sys_write(1, "/", 1);
                sys_write(1, "\n", 1);
            }
            off += e->d_reclen;
        }
    }
    sys_close(fd);
    return 0;
}
