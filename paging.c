#include "paging.h"

static uintptr_t* pd;
static uintptr_t* pt;
static uint64_t curr_page;
static uint64_t curr_table;

void flush_tlb() {

}
void InitPaging() {

}

void DisableIdentityMapping() {

}
uint64_t PageDirAddrV() {
    return 0;
}

void map_page_to_virt(uint64_t virt, uint64_t phy, uint64_t flags) {


}

uint64_t AddKernelPageTable() {
    return 0;
}
uint64_t AddUserPageTable(uint64_t table_idx) {
    return 0;
}

uint64_t AddMMIOPageTable(uint64_t table_idx) {
    return 0;
}



void FillUserPageTable(uint64_t table_idx, uint64_t start_page, uint64_t num_pages) {

}

void FillMMIOPageTable(uint64_t table_idx, uint64_t start_page, uint64_t num_pages, uint64_t offset) {

}

void FillIdentityPageTable(uint64_t table_idx, uint64_t start_page, uint64_t num_pages) {

}

void FillPageDirectoryUser(void* addr, uint64_t size) {

}

void FillPageDirectoryMMIO(void* addr, uint64_t size) {

}

void FillPageDirectoryPCI(void* addr, uint64_t size) {

}

void FillPageDirectoryIdentityMapping(void* addr, uint64_t size) {

}


void RemovePageTables(uint64_t start_table, uint64_t end_table) {

}

void RemovePages(uint64_t table_idx, uint64_t start_page, uint64_t num_pages) {

}

uint64_t AddKernelPages(uint64_t num_pages) {
    return 0;
}

uint64_t AddStack() {
    return 0;
}

void AddGuardPage(uint64_t Tidx, uint64_t Pidx) {

}

uintptr_t getPageDirectory() {
    return 0;
}