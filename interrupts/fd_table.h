#pragma once

#include "types.h"

int64_t  fd_alloc   (file_t* f);     // returns fd or -EMFILE
file_t*  fd_lookup  (int64_t fd);    // NULL on bad fd / unused slot
file_t*  fd_release (int64_t fd);    // detaches fd; returns the file_t (for caller to vfs_file_put), NULL on bad fd
