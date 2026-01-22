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
    PrintVFSHelper(root);
}

void PrintVFSDirectory(VFSNode* node) {
    if (node->attr.type == VFS_FILE) {
        kerror("You passed a File to PrintVFSDirectory");
    }
    PrintVFSHelper(node);
}

void PrintVFSHelper(VFSNode* node) {
    if (node == NULL) {
        return;
    }
    PrintVFSNode(node);
    VFSNode* p = node->children;
    while (p != NULL) {
        PrintVFSHelper(p);
        p = p->next;
    }

}

void PrintVFSNode(VFSNode* node) {
    kprintf("Name: %s\n", node->name);
    PrintVFSAttr(&node->attr);
    node = node->next;
}

void PrintVFSAttr(VFSAttr* attr) {
    kprintf("Type: %d\t Size: %d\t Permissions: %d\t Owner ID: %d\t Group ID: %d\t Link Count: %d\n",
          attr->type, attr->size, attr->permissions, attr->owner_id,
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
    name = GetUntilSlash(name, buffer);
    VFSNode* p = parent;
    while (buffer[0] != '\0') {
        while (p != NULL) {
            if (strcmp(buffer, p->name) == 0) {
                return p;
            }
            p = p->next;   
        }
        if (p == NULL) {
            kerror("Couldn't Find in the Current Directory");
        }
        name = GetUntilSlash(name, buffer);
        if (p->attr.type == VFS_FILE) {
            kerror("%s is a File, Not a Directory", p->name);
        }
        p = p->children;
    }
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
