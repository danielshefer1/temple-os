#include "std/std.h"

// ---- ANSI ------------------------------------------------------------------

#define C_LOGO  "\x1b[33m"        // yellow
#define C_LABEL "\x1b[1;36m"      // bold cyan
#define C_RESET "\x1b[0m"

// ---- ASCII portrait --------------------------------------------------------

static const char* const logo[] = {
    "            .--\"\"\"\"--.",
    "          .'  //~~//  '.",
    "         /  ////////   \\",
    "        |  ////  ////   |",
    "        | (o-)    (o-)  |",
    "        |     /\\        |",
    "        |    (  )       |",
    "         \\   `vv'      /",
    "          '._______.-'",
    "          / |  |  | \\",
    "         /__|__|__|__\\",
    "        |             |",
    "        |_____________|",
};
#define LOGO_LINES   (sizeof(logo)/sizeof(logo[0]))
#define LOGO_PAD     26   // visual width to right-pad logo lines to

// ---- /proc readers ---------------------------------------------------------

// Read at most cap-1 bytes from path into buf and NUL-terminate. Returns
// bytes read, or -1 on error.
static long read_file(const char* path, char* buf, unsigned long cap) {
    long fd = sys_open(path, O_RDONLY, 0);
    if (fd < 0) return -1;
    long n = sys_read(fd, buf, cap - 1);
    sys_close(fd);
    if (n < 0) n = 0;
    buf[n] = '\0';
    return n;
}

// ---- tiny parsing helpers --------------------------------------------------

static int is_digit(char c) { return c >= '0' && c <= '9'; }

static unsigned long parse_ulong(const char* s, const char** end) {
    unsigned long v = 0;
    while (is_digit(*s)) { v = v * 10 + (unsigned)(*s - '0'); s++; }
    if (end) *end = s;
    return v;
}

// Walk to the start of the next line (past '\n'), or to the NUL.
static const char* next_line(const char* s) {
    while (*s && *s != '\n') s++;
    if (*s == '\n') s++;
    return s;
}

// ---- info gatherers --------------------------------------------------------

// Format uptime seconds as "Hh Mm Ss" / "Mm Ss" / "Ss".
static void put_uptime(unsigned long sec) {
    unsigned long h = sec / 3600;
    unsigned long m = (sec % 3600) / 60;
    unsigned long s = sec % 60;
    if (h) { st_putn(h); st_puts("h "); }
    if (h || m) { st_putn(m); st_puts("m "); }
    st_putn(s); st_puts("s");
}

static void emit_uptime(void) {
    char buf[64];
    if (read_file("/proc/uptime", buf, sizeof(buf)) <= 0) {
        st_puts("?");
        return;
    }
    const char* end;
    unsigned long sec = parse_ulong(buf, &end);
    put_uptime(sec);
}

static void emit_kernel(void) {
    char buf[256];
    long n = read_file("/proc/version", buf, sizeof(buf));
    if (n <= 0) { st_puts("?"); return; }
    // strip trailing newline
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
    st_puts(buf);
}

static void emit_cpu(void) {
    static char buf[2048];
    long n = read_file("/proc/cpuinfo", buf, sizeof(buf));
    if (n <= 0) { st_puts("?"); return; }

    unsigned long cores = 0;
    const char* brand_start = 0;
    unsigned long brand_len = 0;

    const char* p = buf;
    while (*p) {
        if (st_strncmp(p, "processor\t", 10) == 0) cores++;
        if (!brand_start && st_strncmp(p, "model name\t: ", 13) == 0) {
            brand_start = p + 13;
            const char* e = brand_start;
            while (*e && *e != '\n') e++;
            // trim trailing spaces
            while (e > brand_start && e[-1] == ' ') e--;
            brand_len = (unsigned long)(e - brand_start);
        }
        p = next_line(p);
    }

    if (brand_start && brand_len) {
        sys_write(1, brand_start, brand_len);
    } else {
        st_puts("unknown");
    }
    st_puts("  (");
    st_putn(cores ? cores : 1);
    st_puts(cores == 1 ? " core)" : " cores)");
}

static void emit_memory(void) {
    char buf[512];
    long n = read_file("/proc/meminfo", buf, sizeof(buf));
    if (n <= 0) { st_puts("?"); return; }

    unsigned long total_kb = 0, free_kb = 0;
    int have_total = 0, have_free = 0;
    const char* p = buf;
    while (*p && (!have_total || !have_free)) {
        if (!have_total && st_strncmp(p, "MemTotal:", 9) == 0) {
            const char* q = p + 9;
            while (*q == ' ' || *q == '\t') q++;
            total_kb = parse_ulong(q, 0);
            have_total = 1;
        } else if (!have_free && st_strncmp(p, "MemFree:", 8) == 0) {
            const char* q = p + 8;
            while (*q == ' ' || *q == '\t') q++;
            free_kb = parse_ulong(q, 0);
            have_free = 1;
        }
        p = next_line(p);
    }

    unsigned long used_kb = total_kb > free_kb ? total_kb - free_kb : 0;
    unsigned long used_mib  = used_kb  / 1024;
    unsigned long total_mib = total_kb / 1024;
    st_putn(used_mib);
    st_puts(" / ");
    st_putn(total_mib);
    st_puts(" MiB");
}

static void emit_terminal(void) {
    winsize_t ws = {0};
    if (sys_ioctl(1, TIOCGWINSZ, &ws) < 0 || ws.ws_col == 0) {
        st_puts("?");
        return;
    }
    st_putn(ws.ws_col);
    st_puts("x");
    st_putn(ws.ws_row);
}

// ---- info table ------------------------------------------------------------

typedef void (*emit_fn)(void);

typedef struct {
    const char* label;
    const char* literal;   // if non-NULL, just print this string
    emit_fn     emit;      // else call this to emit the value
} info_row_t;

static const info_row_t rows[] = {
    { "OS:       ", "TempleOS x86_64", 0 },
    { "Kernel:   ", 0,                 emit_kernel },
    { "Uptime:   ", 0,                 emit_uptime },
    { "CPU:      ", 0,                 emit_cpu },
    { "Memory:   ", 0,                 emit_memory },
    { "Shell:    ", "/bin/sh",         0 },
    { "Terminal: ", 0,                 emit_terminal },
};
#define INFO_LINES   (sizeof(rows)/sizeof(rows[0]))

// Info column starts on this logo line so the rows sit next to the face.
#define INFO_OFFSET  3

// ---- rendering -------------------------------------------------------------

static void put_padded_logo(const char* line) {
    st_puts(C_LOGO);
    unsigned long len = st_strlen(line);
    sys_write(1, line, len);
    st_puts(C_RESET);
    while (len < LOGO_PAD) { sys_write(1, " ", 1); len++; }
}

static void put_info(unsigned long idx) {
    const info_row_t* r = &rows[idx];
    st_puts("    ");           // gutter between logo and info
    st_puts(C_LABEL);
    st_puts(r->label);
    st_puts(C_RESET);
    if (r->literal) st_puts(r->literal);
    else            r->emit();
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;

    unsigned long total = LOGO_LINES;
    if (INFO_OFFSET + INFO_LINES > total) total = INFO_OFFSET + INFO_LINES;

    for (unsigned long i = 0; i < total; i++) {
        if (i < LOGO_LINES) {
            put_padded_logo(logo[i]);
        } else {
            for (unsigned long k = 0; k < LOGO_PAD; k++) sys_write(1, " ", 1);
        }

        unsigned long info_idx = (i >= INFO_OFFSET) ? (i - INFO_OFFSET) : (unsigned long)-1;
        if (info_idx != (unsigned long)-1 && info_idx < INFO_LINES) {
            put_info(info_idx);
        }
        sys_write(1, "\n", 1);
    }
    return 0;
}
