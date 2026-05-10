#include "std/std.h"

static const char* type_name(unsigned long t) {
    switch (t) {
        case S_IFREG:  return "regular";
        case S_IFDIR:  return "directory";
        case S_IFLNK:  return "symlink";
        case S_IFCHR:  return "character device";
        case S_IFBLK:  return "block device";
        case S_IFIFO:  return "fifo";
        case S_IFSOCK: return "socket";
        default:       return "unknown";
    }
}

static void print_field(const char* label, unsigned long v) {
    st_puts(label);
    st_putn(v);
    sys_write(1, "\n", 1);
}

int main(int argc, char** argv) {
    if (argc != 2) {
        st_puts("usage: stat <path>\n");
        return 1;
    }
    stat_t s;
    long r = sys_stat(argv[1], &s);
    if (r < 0) {
        st_puts("stat: ");
        st_puts(argv[1]);
        st_puts(": failed\n");
        return 1;
    }

    st_puts("File:  ");
    st_puts(argv[1]);
    sys_write(1, "\n", 1);

    st_puts("Type:  ");
    st_puts(type_name(s.st_type));
    sys_write(1, "\n", 1);

    st_puts("Mode:  0");
    st_puto(s.st_mode);
    sys_write(1, "\n", 1);

    print_field("Size:  ", s.st_size);
    print_field("Links: ", s.st_nlinks);
    print_field("Uid:   ", s.st_uid);
    print_field("Gid:   ", s.st_gid);
    if (s.st_type == S_IFCHR || s.st_type == S_IFBLK) {
        print_field("Dev:   ", s.st_dev_id);
    }
    print_field("Atime: ", s.st_atime);
    print_field("Mtime: ", s.st_mtime);
    print_field("Ctime: ", s.st_ctime);
    return 0;
}
