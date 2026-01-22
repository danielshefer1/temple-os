#include "vfs.h"

static VFSNode* root;

void InitVFS() {
    bool sti = check_interrupts();
    CliHelper();
    root = kmalloc(sizeof(VFSNode));
    VFSAttr* root_attr = &root->attr;
    root_attr->type = VFS_DIRECTORY;
    root_attr->size = 0;
    root_attr->permissions = 0;
    root_attr->owner_id = 0;
    root_attr->group_id = 0;
    root_attr->link_count = 0;
    root_attr->lock = false;

    root->name = "/";
    root->parent = NULL;
    root->children = NULL;
    root->next = NULL;
    
    if (sti) StiHelper();
}

void PrintVFSRoot() {
    PrintVFSHelper(root, 0);
}

void PrintVFSDirectory(VFSNode* node) {
    if (node->attr.type == VFS_FILE) {
        kerror("You passed a File to PrintVFSDirectory");
    }
    PrintVFSHelper(node, 0);
}

void PrintVFSHelper(VFSNode* node, uint32_t tab_number) {
    if (node == NULL) {
        return;
    }
    PrintVFSNode(node, tab_number);
    VFSNode* p = node->children;
    while (p != NULL) {
        PrintVFSHelper(p, tab_number + 1);
        p = p->next;
    }

}

void PrintVFSNode(VFSNode* node, uint32_t tab_number) {
    for (uint32_t i = 0; i < tab_number; i++) { putchar('\t', GREY_COLOR); }
    kprintf("Name: %s\t", node->name);
    PrintVFSAttr(&node->attr);
    node = node->next;
}

void PrintVFSAttr(VFSAttr* attr) {
    char type[10];
    memset(type, 0, sizeof(type));
    
    switch (attr->type) {
        case VFS_FILE:
            cpystr("FILE", type);
            break;
        case VFS_DIRECTORY:
            cpystr("DIR", type);
            break;
    }

    kprintf("T: %s, S: %d, Perm: %d, OID: %d, GID: %d, LC: %d\n",
          type, attr->size, attr->permissions, attr->owner_id,
          attr->group_id, attr->link_count);
}

void AddVFSNode(VFSAttr* attr, char* name, char* parent_name) {
    if (parent_name == NULL) {
        kerror("Parent is NULL\n");
    }
    VFSNode* parent = FindNode(root, parent_name);

    bool sti = check_interrupts();
    CliHelper();
    VFSNode* node = CreateNode(attr, name);

    node->parent = parent;
    node->children = NULL;
    node->next = NULL;
    AddNodeToParent(parent, node);
    if (sti) StiHelper();
}

VFSNode* CreateNode(VFSAttr* attr, char* name) {
    VFSNode* node = kmalloc(sizeof(VFSNode));
    VFSAttr* node_attr = &node->attr;
    node_attr->type = attr->type;
    node_attr->size = attr->size;
    node_attr->permissions = attr->permissions;
    node_attr->owner_id = attr->owner_id;
    node_attr->group_id = attr->group_id;
    node_attr->link_count = attr->group_id;
    node_attr->lock = attr->lock;

    node->name = name;
    return node;
}

void AddNodeToParent(VFSNode* parent, VFSNode* node) {
    char* node_name = node->name;
    VFSNode* p = parent->children;
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

VFSNode* FindNode(VFSNode* parent, char* name) {
    char buffer[50];
    int32_t len = strlen(name);
    name = GetUntilSlash(name, buffer);
    len -= strlen(buffer);
    VFSNode* p = parent;

    while (p != NULL) {
        while (p != NULL) {
            if (strcmp(buffer, p->name) == 0) {
                break;
            }
            p = p->next;   
        }
        if (len < 0) kerror("Oops, Something went wrong...");

        if (p == NULL) {
            kerror("Couldn't Find in the Current Directory");
        }
        if (len == 0) return p;

        name = GetUntilSlash(name, buffer);
        len -= strlen(buffer);
        int32_t isbufferroot = strcmp("/", buffer);
        if ((isbufferroot == 1 || isbufferroot == -1) && len != 0) len -= 1;

        if (p->attr.type == VFS_FILE) {
            kerror("%s is a File, Not a Directory", p->name);
        }
        p = p->children;
    }
    kerror("Couldn't Find in the Current Directory");
    return NULL;
}

char* GetUntilSlash(char* name, char* buffer) {
    if (*name == '/') {
        buffer[0] = '/';
        buffer[1] = '\0';
        return name + 1;
    } 
    uint32_t i = 0;
    while (name[i] != '/' && name[i] != '\0') {
        buffer[i] = name[i];
        i++;
    }
    buffer[i] = '\0';
    return &name[i] + 1;
}
