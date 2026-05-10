#include "std/std.h"

#define GREP_LINE_MAX 4096

static int contains(const char* hay, unsigned long hlen, const char* pat) {
    unsigned long plen = st_strlen(pat);
    if (plen == 0) return 1;
    if (plen > hlen) return 0;
    for (unsigned long i = 0; i + plen <= hlen; i++) {
        unsigned long j = 0;
        while (j < plen && hay[i + j] == pat[j]) j++;
        if (j == plen) return 1;
    }
    return 0;
}

static int grep_fd(long fd, const char* pat, const char* tag, int multi) {
    static char line[GREP_LINE_MAX];
    char buf[1024];
    unsigned long llen = 0;
    int found = 0;

    for (;;) {
        long n = sys_read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        for (long i = 0; i < n; i++) {
            char ch = buf[i];
            if (ch == '\n' || llen == GREP_LINE_MAX - 1) {
                if (contains(line, llen, pat)) {
                    if (multi) {
                        st_puts(tag);
                        sys_write(1, ":", 1);
                    }
                    sys_write(1, line, llen);
                    sys_write(1, "\n", 1);
                    found = 1;
                }
                llen = 0;
                if (ch != '\n') line[llen++] = ch;
            } else {
                line[llen++] = ch;
            }
        }
    }
    if (llen > 0) {
        if (contains(line, llen, pat)) {
            if (multi) {
                st_puts(tag);
                sys_write(1, ":", 1);
            }
            sys_write(1, line, llen);
            sys_write(1, "\n", 1);
            found = 1;
        }
    }
    return found;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        st_puts("usage: grep <pattern> [file...]\n");
        return 2;
    }
    const char* pat = argv[1];

    if (argc == 2) {
        return grep_fd(0, pat, "(stdin)", 0) ? 0 : 1;
    }

    int any = 0;
    int rc_open = 0;
    int multi = (argc - 2) > 1;
    for (int i = 2; i < argc; i++) {
        long fd = sys_open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            st_puts("grep: ");
            st_puts(argv[i]);
            st_puts(": cannot open\n");
            rc_open = 1;
            continue;
        }
        if (grep_fd(fd, pat, argv[i], multi)) any = 1;
        sys_close(fd);
    }
    if (rc_open) return 2;
    return any ? 0 : 1;
}
