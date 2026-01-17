#include "user_app.h"

__attribute__((section(".text.entry")))
void main() {
    char test[] = "Is this working";
    write(test);
    hlt_syscall();
}