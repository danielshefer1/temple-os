#pragma once

// Tiny userspace runtime: a SysV-compliant _start that pulls argc/argv
// off the initial stack and tail-calls main(argc, argv), plus a handful
// of string/print helpers shared by the small command binaries under
// user/.
//
// One libu.h per user program (each program is its own ELF, so each
// gets its own _start). Programs that don't want argv can simply ignore
// the parameters in main.

#include "syscall_inline.h"

extern int main(int argc, char** argv);

__attribute__((naked, used))
void _start(void) {
    asm volatile(
        "movq (%%rsp), %%rdi\n"     // argc
        "leaq 8(%%rsp), %%rsi\n"    // argv (pointer to argv[0])
        "andq $-16, %%rsp\n"        // 16-byte align before call
        "call main\n"
        "movq %%rax, %%rbx\n"       // exit code into rbx (sys_exit arg)
        "movq $1, %%rax\n"          // EXIT_SYSCALL
        "syscall\n"
        "1: jmp 1b\n"               // unreachable
        ::: );
}

static unsigned long u_strlen(const char* s) {
    unsigned long n = 0; while (s[n]) n++; return n;
}

static int u_strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static void u_puts_fd(long fd, const char* s) {
    sys_write(fd, s, u_strlen(s));
}

static void u_puts(const char* s) { u_puts_fd(1, s); }

static void u_putn_fd(long fd, unsigned long v) {
    char tmp[24]; int t = 0;
    if (v == 0) tmp[t++] = '0';
    else { while (v) { tmp[t++] = (char)('0' + (v % 10)); v /= 10; } }
    char out[24]; int n = 0;
    while (t) out[n++] = tmp[--t];
    sys_write(fd, out, n);
}

static void u_putn(unsigned long v) { u_putn_fd(1, v); }
