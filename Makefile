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

# ============================================================================
# Flags
# ============================================================================
# -mno-red-zone is mandatory for 64-bit kernels to prevent stack corruption
COMMON_CFLAGS = -nostdlib -nostartfiles -ffreestanding -Wall -Wextra -g -fno-pic -fno-pie
K_CFLAGS = $(COMMON_CFLAGS) -m64 -mcmodel=kernel -mno-red-zone
B_CFLAGS = $(COMMON_CFLAGS) -m32

K_LDFLAGS = -m elf_x86_64 -T linker64.ld
B_LDFLAGS = -m elf_i386   -T linker32.ld

ASFLAGS_ELF32 = -f elf32
ASFLAGS_ELF64 = -f elf64
ASFLAGS_BIN   = -f bin

QEMU_FLAGS = -m 16G -cpu host -accel kvm -smp cores=6,threads=2 -machine q35 \
             -drive format=raw,file=$(DISK_IMG) -serial stdio

# ============================================================================
# Source & Object Definitions
# ============================================================================
KERNEL_C_SRCS = E820.c vga.c kernel.c slab_alloc.c paging.c math.c buddy_alloc.c \
                set_gdt.c isr_handler.c set_idt.c timer.c keyboard.c global.c \
                string.c set_tss.c syscall_handler.c vfs.c dcache.c acpi.c \
                memory.c apic.c irq_handler.c utility.c ap_start.c ap_main.c pci.c
KERNEL_ASM_SRCS = helpers.asm trampoline_wrapper.asm

BOOTSTRAP_C_SRCS = bootstrapper.c paging_bootstrap.c
BOOTSTRAP_ASM_SRCS = stage4.asm

# Generate object paths
K_OBJS = $(addprefix $(K_OBJ_DIR)/, $(KERNEL_C_SRCS:.c=.o) $(KERNEL_ASM_SRCS:.asm=.o))
B_OBJS = $(addprefix $(B_OBJ_DIR)/, $(BOOTSTRAP_C_SRCS:.c=.o) $(BOOTSTRAP_ASM_SRCS:.asm=.o))

# ============================================================================
# Build Rules
# ============================================================================
all: $(DISK_IMG)

# --- Kernel Rules ---
$(K_OBJ_DIR)/%.o: %.c | $(K_OBJ_DIR)
	@echo "⚙️  [K64] Compiling $<"
	@$(CC64) $(K_CFLAGS) -MMD -MP -c $< -o $@

$(K_OBJ_DIR)/%.o: %.asm | $(K_OBJ_DIR)
	@echo "💻 [K64] Assembling $<"
	@$(AS) $(ASFLAGS_ELF64) $< -o $@

$(KERNEL_ELF): $(K_OBJS)
	@echo "🔗 Linking Kernel ELF"
	@$(LD64) $(K_LDFLAGS) -o $@ $(K_OBJS)

# --- Bootstrap Rules ---
$(B_OBJ_DIR)/%.o: %.c | $(B_OBJ_DIR)
	@echo "⚙️  [B32] Compiling $<"
	@$(CC32) $(B_CFLAGS) -MMD -MP -c $< -o $@

$(B_OBJ_DIR)/%.o: %.asm | $(B_OBJ_DIR) $(KERNEL_ELF)
	@echo "💻 [B32] Assembling $<"
	@$(AS) $(ASFLAGS_ELF32) $< -o $@

$(BOOTSTRAP_ELF): $(B_OBJS)
	@echo "🔗 Linking Bootstrap ELF"
	@$(LD32) $(B_LDFLAGS) -o $@ $(B_OBJS)

# --- Image Generation ---
$(DISK_IMG): $(KERNEL_ELF) $(BOOTSTRAP_ELF) boot.asm stage2.asm stage3.asm trampoline.asm | $(BUILD_DIR)
	@echo "📦 Constructing Disk Image"
	@$(AS) $(ASFLAGS_BIN) boot.asm -o $(BUILD_DIR)/boot.bin
	@$(AS) $(ASFLAGS_BIN) stage2.asm -o $(BUILD_DIR)/stage2.bin
	@$(AS) $(ASFLAGS_BIN) stage3.asm -o $(BUILD_DIR)/stage3.bin
	@$(AS) $(ASFLAGS_BIN) trampoline.asm -o $(BUILD_DIR)/trampoline.bin
	@$(OBJCOPY64) -O binary $(KERNEL_ELF) $(BUILD_DIR)/kernel.bin
	@$(OBJCOPY32) -O binary $(BOOTSTRAP_ELF) $(BUILD_DIR)/bootstrap.bin
	

	cat $(BUILD_DIR)/bootstrap.bin $(BUILD_DIR)/kernel.bin > $(PAYLOAD)

	dd if=/dev/zero of=$(DISK_IMG) bs=1M count=20
	dd if=$(BUILD_DIR)/boot.bin of=$(DISK_IMG) bs=512 seek=0 conv=notrunc
	dd if=$(BUILD_DIR)/stage2.bin of=$(DISK_IMG) bs=512 seek=1 conv=notrunc
	dd if=$(BUILD_DIR)/stage3.bin of=$(DISK_IMG) bs=512 seek=5 conv=notrunc
	dd if=$(PAYLOAD) of=$(DISK_IMG) bs=512 seek=9 conv=notrunc


# ============================================================================
# Utilities
# ============================================================================
$(BUILD_DIR) $(K_OBJ_DIR) $(B_OBJ_DIR):
	@mkdir -p $@

clean:
	rm -rf $(BUILD_DIR)

run: $(DISK_IMG)
	qemu-system-x86_64 $(QEMU_FLAGS)

debug: $(DISK_IMG) $(KERNEL_ELF)
	qemu-system-x86_64 $(QEMU_FLAGS) -s -S & 
	gdb $(KERNEL_ELF) \
		-ex "target remote localhost:1234" \
		-ex "set pagination off" \
		-ex "set architecture i386" \
		-ex "layout src" \
		-ex "break kmain" \
		-ex "continue"

debug-bootstrap: $(DISK_IMG) $(BOOTSTRAP_ELF)
	qemu-system-x86_64 $(QEMU_FLAGS) -s -S & 
	gdb $(BOOTSTRAP_ELF) \
		-ex "target remote localhost:1234" \
		-ex "set pagination off" \
		-ex "set architecture i386" \
		-ex "layout src" \
		-ex "break bootstrap_kmain" \
		-ex "continue"

# Include dependencies
-include $(K_OBJS:.o=.d)
-include $(B_OBJS:.o=.d)