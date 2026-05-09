#include "libu.h"

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    u_puts("builtin commands:\n");
    u_puts("  help          - this message\n");
    u_puts("  clear         - clear the screen\n");
    u_puts("  pwd           - print working directory\n");
    u_puts("  cd <path>     - change directory (shell builtin)\n");
    u_puts("  ls [path]     - list directory entries\n");
    u_puts("  cat <file...> - print files to stdout\n");
    u_puts("  echo <args>   - print args\n");
    u_puts("  hello         - run the M8/M9 self-test program\n");
    u_puts("  exit          - exit the shell\n");
    return 0;
}
