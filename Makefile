# ============================================================================
# Toolchains
# ============================================================================
CC32 = i686-elf-gcc
LD32 = i686-elf-ld
CC64 = x86_64-elf-gcc
LD64 = x86_64-elf-ld
AS   = nasm
OBJCOPY32 = i686-elf-objcopy
OBJCOPY64 = x86_64-elf-objcopy

# ============================================================================
# Paths and Filenames
# ============================================================================
BUILD_DIR = build
K_OBJ_DIR = $(BUILD_DIR)/kernel
B_OBJ_DIR = $(BUILD_DIR)/bootstrap

DISK_IMG  = $(BUILD_DIR)/os.img
KERNEL_ELF = $(BUILD_DIR)/kernel.elf
BOOTSTRAP_ELF = $(BUILD_DIR)/bootstrap.elf

PAYLOAD = $(BUILD_DIR)/payload.bin
TRAMPOLINE_BIN = $(BUILD_DIR)/trampoline.bin
LONG_MODE_INIT_BIN = $(BUILD_DIR)/long_mode_init.bin

DATA_IMG = data.img

# ============================================================================
# Flags
# ============================================================================
COMMON_CFLAGS = -nostdlib -nostartfiles -ffreestanding -Wall -Wextra -g -fno-pic -fno-pie
INCDIRS = -I ./allocaters -I ./boot -I ./drivers -I ./file_system -I ./init -I ./interrupts -I ./multi -I ./paging -I ./tables -I ./user -I ./util -I ./wrappers

K_CFLAGS = $(COMMON_CFLAGS) -m64 -mcmodel=kernel -mno-red-zone -mno-sse -mno-mmx -mno-sse2 $(INCDIRS)
B_CFLAGS = $(COMMON_CFLAGS) -m32

K_LDFLAGS = -m elf_x86_64 -T linker64.ld
B_LDFLAGS = -m elf_i386   -T linker32.ld

ASFLAGS_ELF32 = -f elf32
ASFLAGS_ELF64 = -f elf64
ASFLAGS_BIN   = -f bin

QEMU_FLAGS = -m 16G -cpu host,+topoext -accel kvm -smp cores=6,threads=2 -machine q35 \
			 -drive format=raw,file=$(DISK_IMG),cache=directsync -serial stdio \
			 -drive index=1,format=raw,file=$(DATA_IMG) \
			 -rtc clock=host,driftfix=slew \
			 #-device qemu-xhci,id=xhci \
			 -device usb-kbd,bus=xhci.0 \
			 -device usb-mouse,bus=xhci.0 \


# ============================================================================
# Source & Object Definitions
# ============================================================================
KERNEL_C_SRCS = drivers/E820.c drivers/vga.c init/kernel.c allocaters/slab_alloc.c paging/paging.c util/math.c allocaters/buddy_alloc.c \
                tables/set_gdt.c interrupts/isr_handler.c tables/set_idt.c wrappers/timer.c wrappers/keyboard.c util/global.c \
                util/string.c interrupts/syscall_handler.c file_system/vfs.c file_system/dcache.c drivers/acpi.c \
                util/memory.c drivers/apic.c interrupts/irq_handler.c util/utility.c multi/ap_start.c multi/ap_main.c drivers/pci.c \
				drivers/ahci_driver.c file_system/mbr.c file_system/ext2_sb_ops.c drivers/rtc.c drivers/fadt.c file_system/ext2_ino_ops.c \
				file_system/blocks_buffer.c file_system/ext2_helpers.c file_system/ext2_file_ops.c \
				file_system/vfs_sb.c file_system/vfs_inode.c file_system/vfs_file.c \
				file_system/vfs_dentry.c file_system/vfs_mount.c file_system/vfs_path.c \
				file_system/vfs_path_ops.c \
				interrupts/fd_table.c interrupts/vfs_syscalls.c
KERNEL_ASM_SRCS = util/helpers.asm multi/trampoline_wrapper.asm

BOOTSTRAP_C_SRCS = boot/bootstrapper.c boot/paging_bootstrap.c
BOOTSTRAP_ASM_SRCS = boot/stage4.asm

OFFSETS = offsets.inc

# Generate object paths
K_OBJS = $(addprefix $(K_OBJ_DIR)/, $(KERNEL_C_SRCS:.c=.o) $(KERNEL_ASM_SRCS:.asm=.o))
B_OBJS = $(addprefix $(B_OBJ_DIR)/, $(BOOTSTRAP_C_SRCS:.c=.o) $(BOOTSTRAP_ASM_SRCS:.asm=.o))


# ============================================================================
# Build Rules
# ============================================================================
all: $(DISK_IMG) $(DATA_IMG)

$(DATA_IMG):
	@echo "🗄️ Creating persistent data disk..."
	@dd if=/dev/zero of=$(DATA_IMG) bs=1G count=1
	@mke2fs -t ext2 -L "TEMPLE_OS_ROOT" $(DATA_IMG)
	@echo "✅ $(DATA_IMG) is ready."

# --- Kernel Rules ---
$(K_OBJ_DIR)/%.o: %.c | $(K_OBJ_DIR)
	@mkdir -p $(dir $@)
	@echo "⚙️  [K64] Compiling $<"
	@$(CC64) $(K_CFLAGS) -MMD -MP -c $< -o $@

$(K_OBJ_DIR)/%.o: %.asm | $(K_OBJ_DIR) $(TRAMPOLINE_BIN)
	@echo "💻 [K64] Assembling $<"
	@$(AS) $(ASFLAGS_ELF64) $< -o $@

$(KERNEL_ELF): $(K_OBJS)
	@echo "🔗 Linking Kernel ELF"
	@$(LD64) $(K_LDFLAGS) -o $@ $(K_OBJS)

# --- Bootstrap Rules ---
$(B_OBJ_DIR)/%.o: %.c | $(B_OBJ_DIR)
	@mkdir -p $(dir $@)
	@echo "⚙️  [B32] Compiling $<"
	@$(CC32) $(B_CFLAGS) -MMD -MP -c $< -o $@

$(B_OBJ_DIR)/%.o: %.asm | $(B_OBJ_DIR) $(OFFSETS) $(LONG_MODE_INIT_BIN)
	@echo "💻 [B32] Assembling $<"
	@$(AS) $(ASFLAGS_ELF32) $< -o $@

$(BOOTSTRAP_ELF): $(B_OBJS)
	@echo "🔗 Linking Bootstrap ELF"
	@$(LD32) $(B_LDFLAGS) -o $@ $(B_OBJS)

$(OFFSETS) : $(KERNEL_ELF) $(B_OBJ_DIR)
	@ offsets.bash

$(TRAMPOLINE_BIN) : $(BUILD_DIR)
	@mkdir -p $(dir $(TRAMPOLINE_BIN))
	@$(AS) $(ASFLAGS_BIN) multi/trampoline.asm -o $(TRAMPOLINE_BIN)

$(LONG_MODE_INIT_BIN) : $(BUILD_DIR)
	@$(AS) $(ASFLAGS_BIN) boot/long_mode_init.asm -o $(LONG_MODE_INIT_BIN)

# --- Image Generation ---
$(DISK_IMG): $(KERNEL_ELF) $(BOOTSTRAP_ELF) boot/boot.asm boot/stage2.asm boot/stage3.asm multi/trampoline.asm | $(BUILD_DIR)
	@echo "📦 Constructing Disk Image"
	@$(AS) $(ASFLAGS_BIN) boot/boot.asm -o $(BUILD_DIR)/boot.bin
	@$(AS) $(ASFLAGS_BIN) boot/stage2.asm -o $(BUILD_DIR)/stage2.bin
	@$(AS) $(ASFLAGS_BIN) boot/stage3.asm -o $(BUILD_DIR)/stage3.bin

	@$(OBJCOPY64) -O binary -R .bss  $(KERNEL_ELF) $(BUILD_DIR)/kernel.bin
	@$(OBJCOPY32) -O binary \
    	-j .stage4 -j .text -j .data -j .bss \
    	--set-section-flags .bss=alloc,load,contents \
    	$(BOOTSTRAP_ELF) $(BUILD_DIR)/bootstrap.bin

	truncate -s %512 build/bootstrap.bin
	truncate -s %512 build/kernel.bin

	cat $(BUILD_DIR)/bootstrap.bin $(BUILD_DIR)/kernel.bin > $(PAYLOAD)

	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=20
	dd if=$(BUILD_DIR)/boot.bin of=$(DISK_IMG) bs=512 seek=0 conv=notrunc
	dd if=$(BUILD_DIR)/stage2.bin of=$(DISK_IMG) bs=512 seek=1 conv=notrunc
	dd if=$(BUILD_DIR)/stage3.bin of=$(DISK_IMG) bs=512 seek=5 conv=notrunc
	dd if=$(PAYLOAD) of=$(DISK_IMG) bs=512 seek=9 conv=notrunc
	@echo "✅ Disk image created successfully!"

# ============================================================================
# Utilities
# ============================================================================
$(BUILD_DIR) $(K_OBJ_DIR) $(B_OBJ_DIR):
	@mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

clean-data:
	rm -rf data.img

clean-all:
	rm -rf $(BUILD_DIR)
	rm -rf data.img

run: $(DISK_IMG) $(DATA_IMG)
	qemu-system-x86_64 $(QEMU_FLAGS)

debug: $(DISK_IMG) $(KERNEL_ELF) $(DATA_IMG)
	qemu-system-x86_64 $(QEMU_FLAGS) -s -S &
	gdb $(KERNEL_ELF) \
		-ex "target remote localhost:1234" \
		-ex "set pagination off" \
		-ex "set architecture x86-64" \
		-ex "layout src" \
		-ex "hbreak kmain" \
		-ex "continue"

debug-bootstrap: $(DISK_IMG) $(BOOTSTRAP_ELF) $(DATA_IMG)
	qemu-system-x86_64 $(QEMU_FLAGS) -s -S & \
	gdb -ex "set architecture i386:x86-64" \
		-tui \
		-ex "layout src" \
	    -ex "target remote localhost:1234" \
	    -ex "symbol-file $(BOOTSTRAP_ELF)" \
	    -ex "hbreak bootstrap_kmain" \
		-ex "continue"

# Include dependencies
-include $(K_OBJS:.o=.d)
-include $(B_OBJS:.o=.d)
