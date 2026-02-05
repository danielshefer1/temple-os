#include "bootstrap.h"
#include "types.h"

void bootstrap_kmain() {
    InitPaging();
    enable_long_mode_and_jump(GetPML4());
}