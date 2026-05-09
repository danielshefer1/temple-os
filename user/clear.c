#include "libu.h"

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    sys_write(1, "\x1b[2J\x1b[H", 7);
    return 0;
}
