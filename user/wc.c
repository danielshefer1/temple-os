#include "std/std.h"

typedef struct {
    unsigned long lines;
    unsigned long words;
    unsigned long bytes;
} counts_t;

static void count_fd(long fd, counts_t* c) {
    char buf[1024];
    int in_word = 0;
    for (;;) {
        long n = sys_read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        c->bytes += (unsigned long)n;
        for (long i = 0; i < n; i++) {
            char ch = buf[i];
            if (ch == '\n') c->lines++;
            int is_space = (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' ||
                            ch == '\v' || ch == '\f');
            if (is_space) {
                in_word = 0;
            } else if (!in_word) {
                in_word = 1;
                c->words++;
            }
        }
    }
}

static void emit(const counts_t* c, int show_l, int show_w, int show_b,
                 const char* name) {
    int first = 1;
    if (show_l) {
        if (!first) sys_write(1, " ", 1);
        st_putn(c->lines); first = 0;
    }
    if (show_w) {
        if (!first) sys_write(1, " ", 1);
        st_putn(c->words); first = 0;
    }
    if (show_b) {
        if (!first) sys_write(1, " ", 1);
        st_putn(c->bytes); first = 0;
    }
    if (name) {
        sys_write(1, " ", 1);
        st_puts(name);
    }
    sys_write(1, "\n", 1);
}

int main(int argc, char** argv) {
    int show_l = 0, show_w = 0, show_b = 0;
    int i = 1;
    for (; i < argc; i++) {
        const char* a = argv[i];
        if (a[0] != '-' || a[1] == 0) break;
        for (int j = 1; a[j]; j++) {
            if      (a[j] == 'l') show_l = 1;
            else if (a[j] == 'w') show_w = 1;
            else if (a[j] == 'c') show_b = 1;
            else { st_puts("wc: unknown flag\n"); return 1; }
        }
    }
    if (!show_l && !show_w && !show_b) { show_l = show_w = show_b = 1; }

    if (i >= argc) {
        counts_t c = {0, 0, 0};
        count_fd(0, &c);
        emit(&c, show_l, show_w, show_b, 0);
        return 0;
    }

    int rc = 0;
    counts_t total = {0, 0, 0};
    int multi = (argc - i) > 1;
    for (; i < argc; i++) {
        long fd = sys_open(argv[i], O_RDONLY, 0);
        if (fd < 0) {
            st_puts("wc: ");
            st_puts(argv[i]);
            st_puts(": cannot open\n");
            rc = 1;
            continue;
        }
        counts_t c = {0, 0, 0};
        count_fd(fd, &c);
        sys_close(fd);
        emit(&c, show_l, show_w, show_b, argv[i]);
        total.lines += c.lines;
        total.words += c.words;
        total.bytes += c.bytes;
    }
    if (multi) emit(&total, show_l, show_w, show_b, "total");
    return rc;
}
