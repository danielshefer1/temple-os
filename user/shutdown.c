// /bin/shutdown — flush the root filesystem and power off via ACPI S5.

#include "std/std.h"

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    sys_shutdown();
    st_puts("shutdown: failed\n");
    return 1;
}
