#include "kernel.h"

void kmain() {
    start();

    file_t* root_f;
    vfs_open_path("/", 0, 0, &root_f);

    char input[50];
    memset(input, 0, sizeof(input));
    kprintf("input a cmd: ");
    kscanf("%s", input);
    while (strcmp(input, "q") != 0) {
        if (strcmp(input, "ls") == 0) {
            vfs_close(root_f);
            vfs_open_path("/", 0, 0, &root_f);
            vfs_ls(root_f);
        }
        else if (memcmp(input, "touch", 6)) {
            vfs_create_path(&input[6], 0644);
        }
        else {
            kprintf("not a recognized cmd!");
        }
        kprintf("\nnext: ");
        memset(input, 0, sizeof(input));
        kscanf("%s", input);
    }

    
    end();
}