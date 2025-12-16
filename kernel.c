#include <stdint.h>
#include <stdarg.h>
#include "print_text.h"
#include "E820.h"

void kmain() {
    clear_screen();
    
    E820Info *e820_info = init_E820((uintptr_t)E820_ADDRESS);
    print_memory_map(e820_info);
}