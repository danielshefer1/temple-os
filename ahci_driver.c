#include "ahci_driver.h"

static uint64_t cmd_list_size;

void AhciReset() {
    if (hba == NULL) {
        kprintf("Haven't found AHCI controller yet!\n");
        return;
    }
    hba->ghc |= (1 << 31); 
    
    hba->ghc |= (1 << 0);
    
    uint8_t cpu_id = get_cpuid();
    uint64_t ticks = timer_ticks[cpu_id];

    while (hba->ghc & 0x01 && (timer_ticks[cpu_id] - ticks < BIOS_TIMEOUT)) {
        PauseHelper();
    }
    if (hba->ghc & 0x01) {
        kprintf("Failed to reset AHCI controller within timeout!\n");
    } else {
        kprintf("Successfully reset AHCI controller.\n");
    }
    
    hba->ghc |= (1 << 31);
    hba->is = hba->is;
}

void TakeAhciOwnership() {
    if (hba == NULL) {
        kprintf("Haven't found AHCI controller yet!\n");
        return;
    }
    if (!(hba->cap2 & 1)) {
        kprintf("AHCI controller doesn't support OS ownership change!\n");
        return;
    }
    hba->bohc |= 1;
    uint64_t cpu_id = get_cpuid();
    uint64_t ticks = timer_ticks[cpu_id];
    while ((hba->bohc & 1) && (hba->bohc & (1 << 4)) && (timer_ticks[cpu_id] - ticks < BIOS_TIMEOUT)) {
        PauseHelper();
    }
    if (hba->bohc & 1 || hba->bohc & (1 << 4)) {
        kprintf("Failed to take ownership of AHCI controller within timeout!\n");
    } else {
        kprintf("Successfully took ownership of AHCI controller.\n");
    }
}

uint8_t AhciActivatePorts() {
    if (hba == NULL) {
        kprintf("Haven't found AHCI controller yet!\n");
        return 0;
    }
    uint8_t ports_count = 0;
    for (int i = 0; i < 32; i++) {
        if (hba->pi & (1 << i)) {
            hba_port_t* port = &hba->ports[i];
            InitPort(port);

            ports_count++;
        }
    }
    return ports_count;
}

void PrintPortsSig() {
    for (int i = 0; i < 32; i++) {
        if (hba->pi & (1 << i)) {
            hba_port_t* port = &hba->ports[i];

            uint8_t det = port->ssts & 0x0F;

            if (det != 0x03) continue;
            uint32_t port_sig = port->sig;

            switch (port_sig) {
                case SATA_SIG_ATAPI:
                    kprintf("Port %d: Found CD-ROM (SATAPI)\n", i);
                    break;
                case SATA_SIG_ATA:
                    kprintf("Port %d: Found Hard Drive (SATA)\n", i);
                    break;
                default:
                    kprintf("Port %d: Unknown device (Sig: %x)\n", i, port_sig);
            }
        }
    }
}

void InitPort(hba_port_t* port) {
    if (cmd_list_size == 0) {
        return;
    }
    volatile hba_cmd_header_t* cmd_list = (hba_cmd_header_t*) AddNonCachableKernelPages(CMD_LIST_PAGES(cmd_list_size));

    memset((void*)cmd_list, 0, CMD_LIST_PAGES(cmd_list_size) * PAGE_SIZE);
    port->cmd &= ~(1 << 0); 
    port->cmd &= ~(1 << 4);

    port->clb =  (uint64_t)KERNEL_VIRT_TO_PHYS(cmd_list) & 0xFFFFFFFF;
    port->clbu = (uint64_t)KERNEL_VIRT_TO_PHYS(cmd_list) >> 32;

    uint64_t fis_addr = (uint64_t)cmd_list + CMD_LIST_HEADER_SIZE * cmd_list_size - KERNEL_VIRTUAL;
    port->fb = fis_addr & 0xFFFFFFFF;
    port->fbu = fis_addr >> 32;

    for (uint64_t i = 0; i < cmd_list_size; i++) {
        uint64_t cmd_table_addr = (uint64_t)cmd_list - KERNEL_VIRTUAL + (RECV_FIS_SIZE + CMD_LIST_HEADER_SIZE * cmd_list_size + CMD_TABLE_SIZE * i);
        cmd_list[i].ctba = cmd_table_addr & 0xFFFFFFFF;
        cmd_list[i].ctbau = cmd_table_addr >> 32;

        cmd_list[i].prdtl = 0;
        cmd_list[i].cfl = 0;
    }

    port->is = 0xFFFFFFFF;
    port->ie = 0xFFFFFFFF;
    port->serr = 0xFFFFFFFF;

    uint8_t cpu_id = get_cpuid();
    uint64_t ticks = timer_ticks[cpu_id];

    while (port->cmd & (1 << 15) && (port->cmd & (1 << 14)) && (timer_ticks[cpu_id] - ticks < BIOS_TIMEOUT)) {
        PauseHelper();
    }
    if (port->cmd & (1 << 15) || port->cmd & (1 << 14)) {
        kprintf("Port is hung, failed to initialize!\n");
        return;
    }

    port->cmd |= (1 << 4); 
    port->cmd |= (1 << 0);

    while (!(port->cmd & (1 << 15)) && !(port->cmd & (1 << 14)) && (timer_ticks[cpu_id] - ticks < BIOS_TIMEOUT)) {
        PauseHelper();
    }
    if (!(port->cmd & (1 << 15)) || !(port->cmd & (1 << 14))) {
        kprintf("Port is hung after enabling, failed to initialize!\n");
        return;
    }

    ticks = timer_ticks[cpu_id];
    while ((port->tfd & ((1 << 7) | (1 << 3))) && (timer_ticks[cpu_id] - ticks < BIOS_TIMEOUT)) {
        PauseHelper();
    }
    if (port->tfd & ((1 << 7) | (1 << 3))) {
        kprintf("Port is hung after enabling, failed to initialize!\n");
        return;
    }
}

int64_t FindFreeSlotInCmdList(hba_port_t* port) {
    uint32_t slots = (port->sact | port->ci);
    
    for (uint64_t i = 0; i < cmd_list_size; i++) {
        if ((slots & (1 << i)) == 0) {
            return i;
        }
    }
    return -1; 
}

void AhciSendIdentify(uint8_t port_no, uint16_t* buffer_phys, bool sata) {
    hba_port_t* port = &hba->ports[port_no];
    

    int64_t slot = FindFreeSlotInCmdList(port);

    if (slot == -1) {
        kprintf("No free command slot found!\n");
        return;
    }

    hba_cmd_header_t* header = (hba_cmd_header_t*)((port->clb | ((uint64_t)port->clbu << 32)) + KERNEL_VIRTUAL);
    header += slot;
    
    hba_cmd_table_t* table = (hba_cmd_table_t*)((header->ctba | ((uint64_t)header->ctbau << 32)) + KERNEL_VIRTUAL);
    memset(table, 0, sizeof(hba_cmd_table_t));

    
    table->prdt_entries[0].dba = (uint64_t)buffer_phys & 0xFFFFFFFF;
    table->prdt_entries[0].dbau = (uint64_t)buffer_phys >> 32;

    table->prdt_entries[0].dw3 |= 511; 
    table->prdt_entries[0].dw3 |= (1 << 31);     

    // 4. Build the FIS (The "Question")
    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)(&table->cfis);
    fis->fis_type = 0x27; 
    fis->c = 1;           
    fis->command = sata ? 0xEC : 0xA1;  // IDENTIFY DEVICE command
    fis->device = 0;      

    header->cfl = sizeof(fis_reg_h2d_t) / 4; 
    header->w = 0;                           
    header->prdtl = 1;                       
    header->prdbc = 0;                       // Reset byte counter

    // 6. Wait for port to be ready
    while (port->tfd & ((1 << 7) | (1 << 3))) PauseHelper();

    // 7. Issue the command!
    port->ci = (1 << slot);

    // 8. Wait for completion
    while (1) {
        // If slot bit is cleared, command is done
        if (!(port->ci & (1 << slot))) break;
        
        // Check for errors
        if (port->is & (1 << 30)) { // Task File Error bit
            kprintf("Disk Error during Identify!\n");
            return;
        }
    }
    kprintf("Identify Complete!\n");
}

bool AhciCommand(uint64_t port_no, uint64_t lba, uint16_t count, uint64_t buffer_phys, bool write) {
    if (count == 0) {
        kprintf("Count must be greater than 0!\n");
        return false;
    }

    volatile hba_port_t* port = &hba->ports[port_no];
    int64_t slot = FindFreeSlotInCmdList(port); 
    if (slot == -1) {
        kprintf("No free command slot found!\n");
        return false;
    }

    uint8_t cpu_id = get_cpuid();
    uint64_t ticks = timer_ticks[cpu_id];

    while ((port->tfd & (0x80 | 0x08)) && (timer_ticks[cpu_id] - ticks < BIOS_TIMEOUT)) {
        PauseHelper();
    }

    hba_cmd_header_t* header = (hba_cmd_header_t*)((port->clb | ((uint64_t)port->clbu << 32)) + KERNEL_VIRTUAL);
    header += slot;

    hba_cmd_table_t* table = (hba_cmd_table_t*)((header->ctba | ((uint64_t)header->ctbau << 32)) + KERNEL_VIRTUAL);
    memset(table, 0, sizeof(hba_cmd_table_t));

    // 1. PRDT setup (for 'count' sectors)
    table->prdt_entries[0].dba = buffer_phys & 0xFFFFFFFF;
    table->prdt_entries[0].dbau = buffer_phys >> 32;
    table->prdt_entries[0].dw3 = (count * 512 - 1) | (1U << 31); 

    // 2. FIS setup for LBA48
    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)(&table->cfis);
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = write ? 0x35: 0x25; // READ DMA EXT

    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 1 << 6; // LBA mode bit

    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);

    fis->countl = count & 0xFF;
    fis->counth = (count >> 8) & 0xFF;

    // 3. Issue and Poll (or wait for ISR)
    header->cfl = 5;
    header->w = write ? 1 : 0;
    header->prdtl = 1;

    port->ci = (1 << slot);
    while (port->ci & (1 << slot)); 

    return true;
}

bool AhciRead(uint64_t port_no, uint64_t lba, uint16_t count, uint64_t buffer_phys) {
    return AhciCommand(port_no, lba, count, buffer_phys, false);
}

bool AhciWrite(uint64_t port_no, uint64_t lba, uint16_t count, uint64_t buffer_phys) {
    return AhciCommand(port_no, lba, count, buffer_phys, true);
}

void PrintAhciModel(uint16_t* buffer) {
    char model[41];
    for (int i = 0; i < 20; i++) {
        uint16_t word = buffer[27 + i];
        model[i * 2] = (char)(word >> 8); 
        model[i * 2 + 1] = (char)(word & 0xFF); 
    }
    model[40] = '\0';
    kprintf("Drive Model: %s\n", model);
}

void PrintDriveInfo(uint16_t* buffer) {
    kprintf("Total Size: %d MiB\n", disk_size / MB);

    PrintAhciModel(buffer);
    
    uint16_t rotation_rate = buffer[217];

    if (rotation_rate == 1) {
        kprintf("Drive is SSD (Non-rotational)\n");
    } else {
        kprintf("Drive is HDD (Rotational), Rotation Rate: %d RPM\n", rotation_rate);
    }
}

void GetAhciDriveInfo() {
    uint16_t* buffer = (uint16_t*) AddNonCachableKernelPages(1);
    bool sata;

    for (int i = 0; i < 32; i++) {
        if (hba->pi & (1 << i)) {
            memset(buffer, 0, 512);
            uint8_t det = hba->ports[i].ssts & 0x0F;

            if (det != 0x03) continue;
            uint32_t port_sig = hba->ports[i].sig;

            switch (port_sig) {

            case SATA_SIG_ATA:
                sata = true;
                break;
            case SATA_SIG_ATAPI:
                sata = false;
                break;
            default:
                kprintf("Port %d: Unknown device (Sig: %x)\n", i, port_sig);
                continue;
            }
            AhciSendIdentify(i, (uint16_t*)KERNEL_VIRT_TO_PHYS((uint64_t)buffer), sata);

            uint64_t total_sectors = (uint64_t)buffer[100] | 
                            ((uint64_t)buffer[101] << 16) | 
                            ((uint64_t)buffer[102] << 32) | 
                            ((uint64_t)buffer[103] << 48);
            uint64_t total_size_mb = (total_sectors * 512) / MB;
            disk_size = total_sectors * 512;

            PrintDriveInfo(buffer);
        }
    }
}


void ParseMbr(uint8_t* buffer) {
    mbr_t* mbr = (mbr_t*)buffer;

    // 1. Check Signature first
    if (mbr->signature != 0xAA55) {
        kprintf("Error: MBR Signature mismatch (Expected 0xAA55, got 0x%x)\n", mbr->signature);
        return;
    }

    kprintf("--- MBR Partition Table ---\n");

    for (int i = 0; i < 4; i++) {
        mbr_partition_t* part = &mbr->partitions[i];

        if (part->sector_count == 0) continue;

        kprintf("Partition #%d:\n", i);
        kprintf("  Bootable: %s\n", (part->attributes & 0x80) ? "Yes" : "No");
        kprintf("  Type: %x\n", part->partition_type);
        kprintf("  Start LBA: %d\n", part->lba_start);
        kprintf("  Sectors: %d\n", part->sector_count);
        
        uint64_t size_mb = ((uint64_t)part->sector_count * 512) / (1024 * 1024);
        kprintf("  Size: %d MB\n", (uint32_t)size_mb);
    }
}

void AhciInit() {
    cmd_list_size = ((hba->cap >> 8) & 0x1F) + 1;
    AhciReset();
    TakeAhciOwnership();
    AhciActivatePorts();

    hba->ghc |= (1 << 1);
}