#include "libu.h"

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    char buf[256];
    long n = sys_getcwd(buf, sizeof(buf));
    if (n <= 0) { u_puts("pwd: error\n"); return 1; }
    sys_write(1, buf, (unsigned long)n - 1);  // drop trailing NUL
    sys_write(1, "\n", 1);
    return 0;
}
