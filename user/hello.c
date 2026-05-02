#include "syscall_inline.h"

static const char msg[] = "hi from user\n";
// Mutable pointer in .data forces R_X86_64_RELATIVE under -static-pie,
// exercising the loader's PIE relocation pass.
__attribute__((used)) const char* volatile msg_ptr = msg;

static unsigned long my_strlen(const char* s) {
    unsigned long n = 0;
    while (s[n]) n++;
    return n;
}

void _start(void) {
    const char* m = msg_ptr;
    sys_write(STDOUT_FILENO, m, my_strlen(m));
    sys_exit();
}
