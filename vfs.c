#include "vfs.h"

static VFSNode* root;

void InitVFS() {
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
    root->parents = NULL;
    root->children = NULL;
    root->next = NULL;
}

void PrintVFS() {
    VFSNode* p = root;
    while (p != NULL) {
        kprintf("Name: %s\n", p->name);
        PrintVFSAttr(&p->attr);
        p = p->next;
    }
}

void PrintVFSNode(VFSNode* node) {
    while (node != NULL) {
        kprintf("Name: %s\n", node->name);
        PrintVFSAttr(&node->attr);
        node = node->next;
    }
}

void PrintVFSAttr(VFSAttr* attr) {
    kprintf("Type: %d\t Size: %d\t Permissions: %d\t Owner ID: %d\t Group ID: z%d\t Link Count: %d\n",
          attr->type, attr->size, attr->permissions, attr->owner_id,
          attr->group_id, attr->link_count);
}

void AddVFSNode(uint32_t type, uint32_t perm, uint32_t owner_id, uint32_t group_id, char* name, char* parent_name) {
    if (parent_name == NULL) {
        kerror("Parent is NULL\n");
    }
    VFSNode* parent = FindNode(root, parent_name);

    VFSNode* node = CreateNode(type, perm, owner_id, group_id, name);

    node->parent = parent;
    node->children = NULL;
    node->next = NULL;
    AddNodeToParent(parent, node);
}

VFSNode* CreateNode(uint32_t type, uint32_t perm, uint32_t owner_id, uint32_t group_id, char* name) {
    VFSNode* node = kmalloc(sizeof(VFSNode));
    VFSAttr* node_attr = &node->attr;
    node_attr->type = type;
    node_attr->size = 0;
    node_attr->permissions = perm;
    node_attr->owner_id = owner_id;
    node_attr->group_id = group_id;
    node_attr->link_count = 0;
    node_attr->lock = false;

    node->name = name;

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
    VFSNode* p = parent->children;
    while (name != '\0') {
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
        p = p->children;
    }
    return NULL;
}

char* GetUntilSlash(char* name, char* buffer) {
    int i = 0;
    while (name[i] != '/' || name != '\0') {
        buffer[i] = name[i];
        i++;
    }
    buffer[i] = '\0';
    return &name[i];
}