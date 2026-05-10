#pragma once

#include "std/syscalls.h"

// SysV-aware _start: pulls argc/argv off the initial stack and tail-calls
// main(argc, argv). After main returns, the exit code is fed to
// EXIT_SYSCALL.
//
// Each user program is its own ELF, so each gets its own _start. Programs
// that need to skip this (e.g. init.c, term.c, hello.c which write their
// own _start) include "std/std.h" with ST_NO_START defined first.

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
