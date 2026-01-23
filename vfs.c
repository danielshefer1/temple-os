#include "vfs.h"

static vfs_dentry_t* root_dentry;

// Global FAT32 operations table
static vfs_ops_t fat32_ops = {
    .read = 0x00, // PlaceHolder for fat32_read
    .write = 0x00, // PlaceHolder for fat32_write
    .finddir = 0x00 // PlaceHolder for fat32_finddir
};

void InitVFS() {
    vfs_inode_t* root_inode;

    root_dentry = (vfs_dentry_t*) kmalloc(sizeof(vfs_dentry_t));
    root_inode = (vfs_inode_t*) kmalloc(sizeof(vfs_inode_t));
    root_dentry->inode = root_inode;
    *root_inode = (vfs_inode_t) {VFS_DIRECTORY, 0, 0, 0, 0, 1, (mutex_t){false, 0, 0}};
    root_dentry->ops = &fat32_ops;

    root_dentry->name = "/";
    root_dentry->parent = NULL;
    root_dentry->children = NULL;
    root_dentry->next = NULL;
    root_dentry->mount_root = NULL;
    
}

void PrintVFS_Root() {
    PrintVFS_Helper(root_dentry, -1);
}

void PrintVFS_Directory(vfs_dentry_t* dentry) {
    if (dentry->inode->type == VFS_FILE) {
        kerror("You passed a File to PrintVFSDirectory");
    }
    PrintVFS_Helper(dentry, -1);
}

void PrintVFS_Helper(vfs_dentry_t* dentry, int32_t tab_number) {
    if (dentry == NULL) {
        return;
    }
    PrintVFS_Dentry(dentry, tab_number);
    vfs_dentry_t* p;
    if (dentry->inode->type == MOUNT_POINT && dentry->mount_root != NULL) p = dentry->mount_root->children;
    else p = dentry->children;

    while (p != NULL) {
        PrintVFS_Helper(p, tab_number + 1);
        p = p->next;
    }

}

void PrintVFS_Dentry(vfs_dentry_t* dentry, int32_t tab_number) {
    if (*dentry->name == '/') return;
    for (int32_t i = 0; i < tab_number; i++) { putchar('\t', GREY_COLOR); }

    kprintf("%s: ", dentry->name);
    
    PrintVFS_Inode(dentry->inode);
}

void PrintVFS_Inode(vfs_inode_t* inode) {
    char type[10];
    memset(type, 0, sizeof(type));
    
    switch (inode->type) {
        case VFS_FILE:
            cpystr("FILE", type);
            break;
        case VFS_DIRECTORY:
            cpystr("DIR", type);
            break;
        case MOUNT_POINT:
            cpystr("MOUNT", type);
    }

    kprintf("T: %s, S: %d, Perm: %d, OID: %d, GID: %d, LC: %d\n",
          type, inode->size, inode->permissions, inode->owner_id,
          inode->group_id, inode->link_count);
}

vfs_dentry_t* VFS_Link(vfs_inode_t* inode, char* name, char* parent_name, vfs_dentry_t* start_dentry) {
    if (parent_name == NULL || *parent_name == '\0') {
        kerror("Parent doesn't exist\n");
    }
    vfs_dentry_t* parent;

    if (start_dentry == NULL) parent = FindDentry(root_dentry, parent_name);
    else parent = FindDentry(start_dentry, parent_name);

    if (parent->inode->type != VFS_DIRECTORY) {
        kerror("%s Is not a directory!", parent->inode->type);
    } 
    
    vfs_dentry_t* node = CreateDentry(inode, name);
    AddDentryToParent(parent, node);
    inode->link_count++; 
    return node;
}

vfs_dentry_t* VFS_CreateDentry(char* name, char* parent_name, uint32_t type, vfs_dentry_t* start_dentry) {
    if (parent_name == NULL || *parent_name == '\0') {
        kerror("Parent doesn't exist\n");
    }
    vfs_dentry_t* parent;

    if (start_dentry == NULL) parent = FindDentry(root_dentry, parent_name);
    else parent = FindDentry(start_dentry, parent_name);
     
    if (parent->inode->type != VFS_DIRECTORY) kerror("%s Is a file, not a directory!", parent->name);

    vfs_inode_t* new_inode = kmalloc(sizeof(vfs_inode_t));
    *new_inode = (vfs_inode_t) {
        .owner_id = 0, // Temp, needs to match the proccess calling
        .group_id = 0, // Same here
        .mutex = (mutex_t) {false, 0, 0},
        .size = 0,
        .permissions = 0, // Temp, needs to match the proccess calling
        .type = type,
        .link_count = 1
    };
    
    vfs_dentry_t* node = CreateDentry(new_inode, name);
    node->parent = parent;
    node->children = NULL;
    node->next = NULL;
    AddDentryToParent(parent, node);
    return node;
}

vfs_dentry_t* VFS_Mount(char* name, char* parent_name, vfs_dentry_t* start_dentry, vfs_dentry_t* mounted_dir) {
    if (mounted_dir->inode->type != VFS_DIRECTORY) kerror("Mounted target %s is not a directory!", mounted_dir->name);

    vfs_dentry_t* mount_dentry = VFS_CreateDentry(name, parent_name, MOUNT_POINT, start_dentry);
    if (mount_dentry == NULL) kerror("Mount Point not initilazed properly!");

    mount_dentry->mount_root = mounted_dir;
    return mount_dentry;
}

vfs_dentry_t* CreateDentry(vfs_inode_t* inode, char* name) {
    vfs_dentry_t* node = kmalloc(sizeof(vfs_dentry_t));
    node->inode = inode;
    root_dentry->ops = &fat32_ops;
    node->name = name;
    return node;
}

void AddDentryToParent(vfs_dentry_t* parent, vfs_dentry_t* node) {
    char* node_name = node->name;
    vfs_dentry_t* p = parent->children;
    if (p == NULL) {
        parent->children = node;
        node->next = NULL;
        return;
    }

    while (p->next != NULL) {
        if (strcmp(node_name, p->next->name) < 0) {
            node->next = p->next;
            p->next = node;
            return;
        }
        p = p->next;
    }
    p->next = node;
    node->next = NULL;
}

vfs_dentry_t* FindDentry(vfs_dentry_t* start_dentry, char* path) {
    if (!path) return NULL;

    vfs_dentry_t* current = (*path == '/') ? root_dentry : start_dentry;
    
    char segment[MAX_FILE_NAME_SIZE]; 
    const char* step = path;

    while ((step = (char*) GetNextSegment(step, segment, sizeof(segment))) != NULL) {
        
        if (strcmp(segment, ".") == 0) continue;

        if (strcmp(segment, "..") == 0) {
            if (current->parent != NULL) {
                current = current->parent;
            }
            continue;
        }

        vfs_dentry_t* found = NULL;

        found = dCacheLookup(current, segment);

        if (!found) {
            found = (vfs_dentry_t*) SearchChildren(current->children, segment);

            if (found) {
                dCachePut(found);
            }
        }

        if (found->mount_root != NULL && found->inode->type == MOUNT_POINT) {
            current = found->mount_root;
        } else {
            current = found;
        }
        
        
        if (*step != '\0' && current->inode->type != VFS_DIRECTORY) {
            kprintf("Error: %s is not a directory\n", current->name);
            return NULL;
        }
    }

    return current;
}

const char* GetNextSegment(const char* path, char* buffer, uint32_t max_len) {
    while (*path == '/') path++;

    if (*path == '\0') return NULL;

    uint32_t i = 0;
    while (path[i] != '/' && path[i] != '\0') {
        if (i < max_len - 1) {
            buffer[i] = path[i];
            i++;
        } else {
            break; 
        }
    }

    buffer[i] = '\0'; 

    return &path[i];
}

vfs_dentry_t* SearchChildren(vfs_dentry_t* child, char* segment) {
    while (child != NULL) {
        if (strcmp(child->name, segment) == 0) {
            return child;
        }
        child = child->next;
    }
    return NULL;
}