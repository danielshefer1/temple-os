#include "user_launch.h"
#include "vfs.h"
#include "vfs_file.h"
#include "vfs_path_ops.h"
#include "buddy_alloc.h"
#include "paging.h"
#include "defintions.h"
#include "extern.h"
#include "vga.h"
#include "string.h"

#define USER_CODE_VA  0x40000000UL
#define USER_STACK_VA 0x40001000UL
#define USER_STACK_TOP (USER_STACK_VA + PAGE_SIZE)

void run_user_program(void) {
    file_t* f = NULL;
    if (vfs_open_path("/user_program.bin", 0, 0, &f) < 0 || f == NULL) {
        kprintf("run_user_program: failed to open /user_program.bin\n");
        return;
    }

    void* code_page  = RequestBuddy(PAGE_SIZE, false);
    void* stack_page = RequestBuddy(PAGE_SIZE, false);
    if (code_page == NULL || stack_page == NULL) {
        kprintf("run_user_program: out of memory allocating user pages\n");
        if (code_page)  FreeBuddy(code_page, false);
        if (stack_page) FreeBuddy(stack_page, false);
        vfs_close(f);
        return;
    }

    map_page_to_virt(USER_CODE_VA, (uint64_t)code_page,
                     PRESENT_PAGE | RW_PAGE | USER_PAGE, false);
    map_page_to_virt(USER_STACK_VA, (uint64_t)stack_page,
                     PRESENT_PAGE | RW_PAGE | USER_PAGE | NX_PAGE, false);

    memset((void*)USER_CODE_VA, 0, PAGE_SIZE);
    memset((void*)USER_STACK_VA, 0, PAGE_SIZE);

    int64_t n = vfs_read(f, (void*)USER_CODE_VA, PAGE_SIZE);
    vfs_close(f);
    if (n <= 0) {
        kprintf("run_user_program: vfs_read returned %d\n", n);
        return;
    }

    
    flush_tlb();

    kprintf("entering user mode at 0x%x...\n", USER_CODE_VA);
    enter_user_mode(USER_CODE_VA, USER_STACK_TOP - 16);
}
