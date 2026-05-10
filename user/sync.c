#include "std/std.h"

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    long r = sys_sync();
    if (r < 0) {
        st_puts("sync: failed\n");
        return 1;
    }
    return 0;
}
