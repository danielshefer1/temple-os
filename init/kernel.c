#include "kernel.h"
#include "user_launch.h"

void kmain() {
    start();

    run_user_program();

    end();
}