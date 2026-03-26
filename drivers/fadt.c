#include "fadt.h"

void EnableAcpi(fadt_t* fadt) {
    if (!(inw(fadt->pm1a_cnt_blk) & 1)) {
        if (fadt->smi_cmd != 0 && fadt->acpi_enable != 0) {
            outb(fadt->smi_cmd, fadt->acpi_enable);
            
            while (!(inw(fadt->pm1a_cnt_blk) & 1));
        }
    }
}

void AcpiShutdown(fadt_t* fadt) {
    uint16_t SLP_TYP_S5 = 0; // IMPORTANT - If I have time I will change later to be dynamic, rn I need to finish the project - IMPORTANT
    
    uint16_t SLP_EN = (1 << 13);

    uint16_t shutdown_command = (SLP_TYP_S5 << 10) | SLP_EN;

    outw(fadt->pm1a_cnt_blk, shutdown_command);

    if (fadt->pm1b_cnt_blk != 0) {
        outw(fadt->pm1b_cnt_blk, shutdown_command);
    }
    
    while(true) {HltHelper();}
}

uint8_t GetCenturyRegInternal(fadt_t* fadt) {
    return fadt->century;
}