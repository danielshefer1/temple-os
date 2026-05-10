#include "ahci_driver.h"

static uint64_t cmd_list_size;

// Per-port state for interrupt-driven completion. See AhciHandler.
static ahci_port_state_t port_states[32];

// Reserve an unused slot on `port_no` and mark it in-flight. Returns -1 on
// no free slot. Caller must hold port_states[port_no].lock and have IF=0.
static int64_t ahci_reserve_slot_locked(uint8_t port_no) {
    hba_port_t* port = (hba_port_t*)(uintptr_t)&hba->ports[port_no];
    ahci_port_state_t* st = &port_states[port_no];
    uint32_t in_use = port->sact | port->ci | st->issued_mask;
    for (uint64_t i = 0; i < cmd_list_size; i++) {
        if (!(in_use & (1u << i))) {
            ahci_completion_t* c = &st->completions[i];
            c->done = false;
            c->error = false;
            c->waiter = NULL;
            st->issued_mask |= (1u << i);
            return (int64_t)i;
        }
    }
    return -1;
}

// Block (or poll, before scheduler is up) until the slot completes. Returns
// 0 on success, -1 on TFE.
static int64_t ahci_wait_completion(uint8_t port_no, int64_t slot) {
    ahci_port_state_t* st = &port_states[port_no];
    ahci_completion_t* c = &st->completions[slot];
    hba_port_t* port = (hba_port_t*)(uintptr_t)&hba->ports[port_no];

    cpu_local_t* cpu = this_cpu();
    bool can_block = cpu && cpu->current && check_interrupts();

    if (!can_block) {
        // Early-boot fallback: scheduler not yet attached, or interrupts
        // are disabled (the IRQ that would wake us cannot fire). Poll.
        while (port->ci & (1u << slot)) {
            if (port->is & (1u << 30)) {
                spin_lock(&st->lock);
                st->issued_mask &= ~(1u << slot);
                spin_unlock(&st->lock);
                return -1;
            }
            PauseHelper();
        }
        spin_lock(&st->lock);
        st->issued_mask &= ~(1u << slot);
        spin_unlock(&st->lock);
        return 0;
    }

    CliHelper();
    spin_lock(&c->guard);
    if (!c->done) {
        c->waiter = cpu->current;
        cpu->current->state = TASK_STATE_BLOCKED;
        spin_unlock(&c->guard);
        schedule();
    } else {
        spin_unlock(&c->guard);
    }
    StiHelper();

    return c->error ? -1 : 0;
}

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
            hba_port_t* port = (hba_port_t*)(uintptr_t)&hba->ports[i];
            InitPort(port);

            ports_count++;
        }
    }
    return ports_count;
}

void PrintPortsSig() {
    for (int i = 0; i < 32; i++) {
        if (hba->pi & (1 << i)) {
            hba_port_t* port = (hba_port_t*)(uintptr_t)&hba->ports[i];

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
    hba_port_t* port = (hba_port_t*)(uintptr_t)&hba->ports[port_no];
    ahci_port_state_t* st = &port_states[port_no];

    while (port->tfd & ((1 << 7) | (1 << 3))) PauseHelper();

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&st->lock);

    int64_t slot = ahci_reserve_slot_locked(port_no);
    if (slot == -1) {
        spin_unlock(&st->lock);
        if (ie) StiHelper();
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

    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)(&table->cfis);
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = sata ? 0xEC : 0xA1;
    fis->device = 0;

    header->cfl = sizeof(fis_reg_h2d_t) / 4;
    header->w = 0;
    header->prdtl = 1;
    header->prdbc = 0;

    port->ci = (1u << slot);
    spin_unlock(&st->lock);
    if (ie) StiHelper();

    if (ahci_wait_completion(port_no, slot) < 0) {
        kprintf("Disk Error during Identify!\n");
        return;
    }
    kprintf("Identify Complete!\n");
}

int64_t AhciFlush(hba_port_t* port) {
    uint8_t port_no = (uint8_t)(port - hba->ports);
    ahci_port_state_t* st = &port_states[port_no];

    while (port->tfd & ((1 << 7) | (1 << 3))) PauseHelper();

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&st->lock);

    int64_t slot = ahci_reserve_slot_locked(port_no);
    if (slot == -1) {
        spin_unlock(&st->lock);
        if (ie) StiHelper();
        kprintf("No free command slot found!\n");
        return 1;
    }

    hba_cmd_header_t* header = (hba_cmd_header_t*)((port->clb | ((uint64_t)port->clbu << 32)) + KERNEL_VIRTUAL);
    header += slot;

    hba_cmd_table_t* table = (hba_cmd_table_t*)((header->ctba | ((uint64_t)header->ctbau << 32)) + KERNEL_VIRTUAL);
    memset(table, 0, sizeof(hba_cmd_table_t));

    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)(&table->cfis);
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = 0xEA;

    header->cfl = sizeof(fis_reg_h2d_t) / 4;
    header->w = 0;
    header->prdtl = 0;
    header->prdbc = 0;

    port->ci = (1u << slot);
    spin_unlock(&st->lock);
    if (ie) StiHelper();

    if (ahci_wait_completion(port_no, slot) < 0) {
        kprintf("Disk Error during Flush!\n");
        return 1;
    }
    return 0;
}

int64_t AhciFlushWrapper(block_device_t* dev) {
    return AhciFlush((hba_port_t*)dev->device_specific_ptr);
}

bool AhciCommand(hba_port_t* port, uint64_t lba, uint16_t count, uint64_t buffer_phys, bool write) {
    if (count == 0) {
        kprintf("Count must be greater than 0!\n");
        return false;
    }

    uint8_t port_no = (uint8_t)(port - hba->ports);
    ahci_port_state_t* st = &port_states[port_no];

    uint8_t cpu_id = get_cpuid();
    uint64_t ticks = timer_ticks[cpu_id];
    while ((port->tfd & (0x80 | 0x08)) && (timer_ticks[cpu_id] - ticks < BIOS_TIMEOUT)) {
        PauseHelper();
    }

    bool ie = check_interrupts();
    CliHelper();
    spin_lock(&st->lock);

    int64_t slot = ahci_reserve_slot_locked(port_no);
    if (slot == -1) {
        spin_unlock(&st->lock);
        if (ie) StiHelper();
        kprintf("No free command slot found!\n");
        return false;
    }

    hba_cmd_header_t* header = (hba_cmd_header_t*)((port->clb | ((uint64_t)port->clbu << 32)) + KERNEL_VIRTUAL);
    header += slot;

    hba_cmd_table_t* table = (hba_cmd_table_t*)((header->ctba | ((uint64_t)header->ctbau << 32)) + KERNEL_VIRTUAL);
    memset(table, 0, sizeof(hba_cmd_table_t));

    table->prdt_entries[0].dba = buffer_phys & 0xFFFFFFFF;
    table->prdt_entries[0].dbau = buffer_phys >> 32;
    table->prdt_entries[0].dw3 = (count * 512 - 1) | (1U << 31);

    fis_reg_h2d_t* fis = (fis_reg_h2d_t*)(&table->cfis);
    fis->fis_type = 0x27;
    fis->c = 1;
    fis->command = write ? 0x35: 0x25;

    fis->lba0 = (uint8_t)lba;
    fis->lba1 = (uint8_t)(lba >> 8);
    fis->lba2 = (uint8_t)(lba >> 16);
    fis->device = 1 << 6;

    fis->lba3 = (uint8_t)(lba >> 24);
    fis->lba4 = (uint8_t)(lba >> 32);
    fis->lba5 = (uint8_t)(lba >> 40);

    fis->countl = count & 0xFF;
    fis->counth = (count >> 8) & 0xFF;

    header->cfl = 5;
    header->w = write ? 1 : 0;
    header->prdtl = 1;

    port->ci = (1u << slot);
    spin_unlock(&st->lock);
    if (ie) StiHelper();

    return ahci_wait_completion(port_no, slot) == 0;
}

bool AhciRead(hba_port_t* port, uint64_t lba, uint16_t count, uint64_t buffer_phys) {
    return AhciCommand(port, lba, count, buffer_phys, false);
}

int64_t AhciReadWrapper(block_device_t* dev, uint64_t lba, uint32_t count, void* buffer) {
    hba_port_t* port = (hba_port_t*)dev->device_specific_ptr;
    bool result = AhciRead(port, lba, count, (uint64_t) buffer);

    if (result) return 0;
    return 1;
}

bool AhciWrite(hba_port_t* port, uint64_t lba, uint16_t count, uint64_t buffer_phys) {
    return AhciCommand(port, lba, count, buffer_phys, true);
}

int64_t AhciWriteWrapper(block_device_t* dev, uint64_t lba, uint32_t count, void* buffer) {
    hba_port_t* port = (hba_port_t*)dev->device_specific_ptr;
    bool result = AhciWrite(port, lba, count, (uint64_t) buffer);

    if (result) return 0;
    return 1;
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

    block_device_t* dev;
    block_device_node_t* dev_node;

    char device_name[] = "sda";

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

            if (total_sectors == 0) continue;

            uint32_t logical_sector_size = 512;
            uint16_t word106 = buffer[106];
            
            if ((word106 & 0xC000) == 0x4000) {
                if (word106 & (1 << 12)) {
                    logical_sector_size = (uint32_t)buffer[117] | ((uint32_t)buffer[118] << 16);
                }
            }

            dev = (block_device_t*) kmalloc(sizeof(block_device_t));
            dev_node = (block_device_node_t*) kmalloc(sizeof(block_device_node_t));

            dev->device_specific_ptr = (void*)(uintptr_t)&hba->ports[i];
            dev->sector_size = logical_sector_size;
            dev->total_sectors = total_sectors;
            cpystr(device_name, dev->name);
            device_name[2]++;

            dev->read = AhciReadWrapper;
            dev->write = AhciWriteWrapper;
            dev->flush = AhciFlushWrapper;

            dev_node->value = dev;
            dev_node->next = devices_head;
            devices_head = dev_node;

            //PrintDriveInfo(buffer);
        }
    }
    RemoveKernelPages((uint64_t)buffer, 1);
}

void PrintAhciDevices() {
    block_device_node_t* p = devices_head;
    block_device_t* dev;
    uint64_t idx = 0;

    while (p != NULL) {
        dev = p->value;
        kprintf("Device #%d:\n", idx);
        kprintf("Name: %s\t", dev->name);
        kprintf("Total Sectors: %d, Sectors Size: %d\n", dev->total_sectors, dev->sector_size);

        p = p->next;
        idx++;
    }
}

void AhciHandler() {
    uint32_t interrupt_status = hba->is;

    for (int i = 0; i < 32; i++) {
        if (!(interrupt_status & (1 << i))) continue;
        hba_port_t* port = (hba_port_t*)(uintptr_t)&hba->ports[i];
        ahci_port_state_t* st = &port_states[i];

        uint32_t port_is = port->is;
        bool tfe = (port_is & (1 << 30)) != 0;

        if (tfe) {
            uint32_t tfd = port->tfd;
            uint8_t error_reg = (tfd >> 8) & 0xFF;
            kprintf("AHCI TFES on port %d, Error Register: %x\n", i, error_reg);
        }

        // Snapshot which of our in-flight slots have completed (CI cleared
        // by the device). Clear them from issued_mask under the port lock
        // so submitters and the IRQ agree on slot ownership.
        spin_lock(&st->lock);
        uint32_t completed = st->issued_mask & ~port->ci;
        st->issued_mask &= ~completed;
        spin_unlock(&st->lock);

        for (uint8_t s = 0; s < AHCI_MAX_SLOTS; s++) {
            if (!(completed & (1u << s))) continue;
            ahci_completion_t* c = &st->completions[s];
            spin_lock(&c->guard);
            c->done = true;
            c->error = tfe;
            task_t* w = c->waiter;
            c->waiter = NULL;
            spin_unlock(&c->guard);
            if (w) {
                w->state = TASK_STATE_READY;
                rq_enqueue_external(w);
            }
        }

        // Write-1-to-clear the latched port IS, then the global per-port bit.
        port->is = port_is;
        hba->is = (1u << i);
    }
}

void AhciInit() {
    cmd_list_size = ((hba->cap >> 8) & 0x1F) + 1;
    AhciReset();
    TakeAhciOwnership();
    AhciActivatePorts();

    hba->ghc |= (1 << 1);
    GetAhciDriveInfo();
}