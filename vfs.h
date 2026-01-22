#pragma once

#include "includes.h"
#include "types.h"
#include "defintions.h"
#include "slab_alloc.h"
#include "vga.h"
#include "str_ops.h"

void InitVFS();
void PrintVFSNode(VFSNode* node);
void PrintVFSRoot();
void PrintVFSAttr(VFSAttr* attr);
void AddVFSNode(VFSAttr* attr, char* name, char* parent_name);
VFSNode* FindNode(VFSNode* parent, char* name);
VFSNode* CreateNode(VFSAttr* attr, char* name);
void AddNodeToParent(VFSNode* parent, VFSNode* node);
char* GetUntilSlash(char* name, char* buffer);
void PrintVFSHelper(VFSNode* node);