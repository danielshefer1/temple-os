#include "std/std.h"
#include "std/fcntl.h"
#include "std/stdio.h"

// Must match drivers/mouse_dev_types.h.
typedef struct {
    short    dx;
    short    dy;
    unsigned char buttons;
    unsigned char _pad[3];
} mouse_event_t;

#define BATCH 64
#define POLL_MS 100

static const char* btn_name(int bit) {
    switch (bit) {
        case 0: return "left";
        case 1: return "right";
        case 2: return "middle";
        default: return "?";
    }
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    long fd = sys_open("/dev/mouse", O_RDONLY, 0);
    if (fd < 0) {
        st_puts("mouse: open /dev/mouse failed\n");
        return 1;
    }

    long x = 0, y = 0;
    unsigned char prev_btns = 0;
    mouse_event_t evs[BATCH];

    st_puts("mouse: reading from /dev/mouse — move or click\n");

    while (1) {
        // Blocks until at least one event is queued. After it returns,
        // sleep briefly so more events can accumulate, then drain them
        // all in one go before printing — keeps the output rate bounded
        // even when the mouse is moving fast.
        long n = sys_read(fd, evs, sizeof(evs));
        if (n <= 0) {
            st_puts("mouse: read error\n");
            break;
        }
        sys_sleep(POLL_MS);
        long m = sys_read(fd, evs + n / (long)sizeof(mouse_event_t),
                          sizeof(evs) - (unsigned long)n);
        // Second read may return 0 if no new events arrived; that's fine.
        long total_bytes = n + (m > 0 ? m : 0);
        long count = total_bytes / (long)sizeof(mouse_event_t);

        for (long i = 0; i < count; i++) {
            x += evs[i].dx;
            y += evs[i].dy;

            unsigned char b = evs[i].buttons;
            unsigned char changed = b ^ prev_btns;
            for (int bit = 0; bit < 3; bit++) {
                if (changed & (1u << bit)) {
                    st_puts((b & (1u << bit)) ? "  [press]   "
                                              : "  [release] ");
                    st_puts(btn_name(bit));
                    st_puts("\n");
                }
            }
            prev_btns = b;
        }

        st_puts("pos x=");
        st_putd(x);
        st_puts(" y=");
        st_putd(y);
        st_puts("  btns=");
        st_puts((prev_btns & 1) ? "L" : "-");
        st_puts((prev_btns & 2) ? "R" : "-");
        st_puts((prev_btns & 4) ? "M" : "-");
        st_puts("\n");
    }

    sys_close(fd);
    return 0;
}
