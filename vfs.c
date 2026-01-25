#include "vfs.h"

static dentry_t* root_dentry;

// Global FAT32 operations table
static vfs_ops_t fat32_ops = {
    .read = 0x00, // PlaceHolder for fat32_read
    .write = 0x00, // PlaceHolder for fat32_write
    .finddir = 0x00 // PlaceHolder for fat32_finddir
};

void InitVFS() {
    inode_t* root_inode;
    
    root_dentry = (dentry_t*) kmalloc(sizeof(dentry_t));
    root_inode = (inode_t*) kmalloc(sizeof(inode_t));
    root_dentry->inode = root_inode;
    *root_inode = (inode_t) {VFS_DIRECTORY, 0, 0, 0, 0, 1, (mutex_t){false, 0, 0}};
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

void PrintVFS_Directory(dentry_t* dentry) {
    if (dentry->inode->type == VFS_FILE) {
        kerror("You passed a File to PrintVFSDirectory");
    }
    PrintVFS_Helper(dentry, -1);
}

void PrintVFS_Helper(dentry_t* dentry, int32_t tab_number) {
    if (dentry == NULL) {
        return;
    }
    PrintVFS_Dentry(dentry, tab_number);
    dentry_t* p;
    if (dentry->inode->type == MOUNT_POINT && dentry->mount_root != NULL) p = dentry->mount_root->children;
    else p = dentry->children;

    while (p != NULL) {
        PrintVFS_Helper(p, tab_number + 1);
        p = p->next;
    }

}

void PrintVFS_Dentry(dentry_t* dentry, int32_t tab_number) {
    if (*dentry->name == '/') return;
    for (int32_t i = 0; i < tab_number; i++) { putchar('\t', GREY_COLOR); }

    char* name = dentry->name;
    kprintf("%s: ", name);
    
    switch (dentry->inode->type) {
        case VFS_FILE:
            PrintVFS_Inode(dentry->inode);
            break;
        case VFS_DIRECTORY:
            PrintVFS_Inode(dentry->inode);
            break;
        case MOUNT_POINT:
            kprintf("mount name - %s\n", dentry->mount_root->name);
            break;
        case SYS_LINK:
            kprintf("path - %s\n", dentry->syslink_name);
            break;
        default:
            kprintf("Unkown Dentry Type, type - %d\n", dentry->inode->type);
    } 
    
}

void PrintVFS_Inode(inode_t* inode) {
    char type[10];
    memset(type, 0, sizeof(type));

    GetTypeString(inode->type, type);

    kprintf("T: %s, S: %d, Perm: %d, OID: %d, GID: %d, LC: %d\n",
          type, inode->size, inode->permissions, inode->owner_id,
          inode->group_id, inode->link_count);
}

void GetTypeString(uint32_t type_idx, char* type) {
    switch (type_idx) {
        case VFS_FILE:
            cpystr("FILE", type);
            break;
        case VFS_DIRECTORY:
            cpystr("DIR", type);
            break;
        case MOUNT_POINT:
            cpystr("MOUNT", type);
            break;
        case SYS_LINK:
            cpystr("S-LINK", type);
            break;
        default:
            cpystr("UK", type);
    }
}

dentry_t* VFS_HardLink(char* name, char* parent_name, dentry_t* cwd, inode_t* inode, char* inode_path) {
    if (parent_name == NULL || *parent_name == '\0') {
        kerror("Parent doesn't exist\n");
    }
    dentry_t* parent, *p;

    if (inode == NULL) {
        p = FindDentry(cwd, inode_path);
        if (p != NULL) inode = p->inode;
        else return NULL;
    }

    if (inode->type != VFS_FILE) return NULL;

    if (cwd == NULL) parent = FindDentry(root_dentry, parent_name);
    else parent = FindDentry(cwd, parent_name);

    if (parent->inode->type != VFS_DIRECTORY) {
        kerror("%s Is not a directory!", parent->inode->type);
    } 
    
    dentry_t* node = CreateDentry(inode, name);
    AddDentryToParent(parent, node);
    inode->link_count++; 
    return node;
}

dentry_t* VFS_SysLink(char* name, char* parent_name, dentry_t* cwd, char* syslink_name) {
    if (!syslink_name) kerror("Nothing inside syslink_name");
    if (*syslink_name == '\0') kerror("Nothing inside syslink_name");
    if (*syslink_name != '/') kerror("syslink_name is not an absolute path!");

    dentry_t* dentry;

    dentry = VFS_CreateDentry(name, parent_name, SYS_LINK, cwd);
    dentry->syslink_name = syslink_name;

    return dentry;
}

dentry_t* VFS_CreateDentry(char* name, char* parent_name, uint32_t type, dentry_t* cwd) {
    if (parent_name == NULL || *parent_name == '\0') {
        kerror("Parent doesn't exist\n");
    }
    dentry_t* parent;

    if (cwd == NULL) parent = FindDentry(root_dentry, parent_name);
    else parent = FindDentry(cwd, parent_name);

    

    if (parent == NULL) kerror("Parent not Found!");
    if (parent->inode == NULL) kerror("Parent inode not found!");

    if (parent->inode->type != VFS_DIRECTORY) kerror("%s Is not a directory!", parent->name);

    inode_t* new_inode = kmalloc(sizeof(inode_t));

    *new_inode = (inode_t) {
        .owner_id = 0, // Temp, needs to match the proccess calling
        .group_id = 0, // Same here
        .mutex = (mutex_t) {false, 0, 0},
        .size = 0,
        .permissions = 0, // Temp, needs to match the proccess calling
        .type = type,
        .link_count = 1
    };
    
    dentry_t* node = CreateDentry(new_inode, name);
    node->children = NULL;
    node->next = NULL;
    AddDentryToParent(parent, node);
    return node;
}

dentry_t* VFS_Mount(char* name, char* parent_name, dentry_t* cwd, dentry_t* mounted_dir) {
    if (mounted_dir->inode->type != VFS_DIRECTORY) kerror("Mounted target %s is not a directory!", mounted_dir->name);

    dentry_t* mount_dentry = VFS_CreateDentry(name, parent_name, MOUNT_POINT, cwd);
    if (mount_dentry == NULL) kerror("Mount Point not initilazed properly!");

    mount_dentry->mount_root = mounted_dir;
    return mount_dentry;
}

dentry_t* CreateDentry(inode_t* inode, char* name)  {
    dentry_t* node = kmalloc(sizeof(dentry_t));
    node->inode = inode;
    node->ops = &fat32_ops;
    node->name = name;
    return node;
}

void AddDentryToParent(dentry_t* parent, dentry_t* node) {
    dCachePut(node);
    node->parent = parent;
    char* node_name = node->name;
    dentry_t* p = parent->children;

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

void VFS_RemoveDentryH(dentry_t* dentry) {
    if (dentry->children == NULL) {
        dentry->inode->link_count--;
        if (dentry->inode->link_count == 0) {
            dCacheRemove(dentry);
            kfree(dentry, sizeof(dentry_t));
        }
        return;
    }
    dentry_t* p = dentry->children;
    while (p != NULL) {
        dentry->children = p->next;
        VFS_RemoveDentryH(p);
        p = dentry->children;
    }


    dentry->inode->link_count--;
    if (dentry->inode->link_count == 0) {
        dCacheRemove(dentry);
        kfree(dentry, sizeof(dentry_t));
    }
}

void VFS_RemoveDentry(dentry_t* dentry) {
    
    RemoveDentryFromParent(dentry);

    VFS_RemoveDentryH(dentry);
}

void RemoveDentryFromParent(dentry_t* dentry) {
    dentry_t* parent_node = dentry->parent; 
    
    if (parent_node == NULL) return; 

    dentry_t* p = parent_node->children;

    if (p == dentry) {
        
        
        parent_node->children = p->next; 
        

        return;
    } 

    while (p->next != NULL) {
        if (p->next == dentry) {

            
            
            p->next = p->next->next; 
            
    
            return;
        }
        p = p->next;
    }

}



dentry_t* FindDentry(dentry_t* cwd, char* path) {
    if (!path) return NULL;

    dentry_t* current = (*path == '/') ? root_dentry : cwd;
    
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

        dentry_t* found = NULL;

        found = dCacheLookup(current, segment);

        if (!found) {
            found = (dentry_t*) SearchChildren(current->children, segment);

            if (found) {
                dCachePut(found);
            }
        }

        if (found == NULL) kerror("Not found in current directory!");

        if (found->mount_root != NULL && found->inode->type == MOUNT_POINT) {
            current = found->mount_root;
        } 
        else if (IsValidSysLink(current)) {
            current = FindDentry(NULL, found->syslink_name);
        }
        else {
            current = found;
        }
        
        
        if (*step != '\0' && current->inode->type != VFS_DIRECTORY) {
            kerror("Error: %s is not a directory\n", current->name);
        }
    }
    if (IsValidSysLink(current)) current = FindDentry(NULL, current->syslink_name);

    return current;
}

bool IsValidSysLink(dentry_t* dentry) {
    if (dentry == NULL) {return false;}

    if (dentry->inode == NULL) {return false;}

    if (dentry->syslink_name == NULL) {return false;}

    if (dentry->inode->type != SYS_LINK) {return false;}

    if (*dentry->syslink_name != '/') {return false;}

    return true;
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

dentry_t* SearchChildren(dentry_t* child, char* segment) {
    while (child != NULL) {
        if (strcmp(child->name, segment) == 0) {
            return child;
        }
        child = child->next;
    }
    return NULL;
}