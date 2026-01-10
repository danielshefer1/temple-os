# --- Makefile for x86 Bare-Metal OS using i686-elf- toolchain ---

.PHONY: all clean run debug kill-qemu

# ============================================================================
# DIRECTORY CONFIGURATION
# ============================================================================
BUILD_DIR = build

# ============================================================================
# TOOLCHAIN CONFIGURATION
# ============================================================================
CC      = i686-elf-gcc
AS      = nasm
LD      = i686-elf-ld
OBJCOPY = i686-elf-objcopy

# ============================================================================
# FLAGS
# ============================================================================
CFLAGS       = -m32 -nostdlib -nostartfiles -ffreestanding -Wall -Wextra -g -I -fno-pic -fno-pie
ASFLAGS_BIN  = -f bin
ASFLAGS_ELF  = -f elf32 -g
LDFLAGS      = -m elf_i386 -T linker.ld
QEMU_FLAGS   = -m 4096 -serial stdio -drive format=raw,file=$(BUILD_DIR)/os.img

# ============================================================================
# SOURCE FILES
# ============================================================================

# C source files (add new .c files here)
C_SOURCES = bootstrapper.c paging_bootstrap.c E820.c print_text.c kernel.c slab_alloc.c paging.c math_ops.c buddy_alloc.c
# Generated object files from C sources (now in build dir)
C_OBJECTS = $(addprefix $(BUILD_DIR)/, $(C_SOURCES:.c=.o))
# Assembly sources
ASM_SOURCES = stage3.asm
ASM_OBJECTS = $(addprefix $(BUILD_DIR)/, $(ASM_SOURCES:.asm=.o))

# All object files needed for kernel
KERNEL_OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# ============================================================================
# OUTPUT FILES
# ============================================================================
STAGE1_BIN   = $(BUILD_DIR)/boot.bin
STAGE2_BIN   = $(BUILD_DIR)/boot2.bin
STAGE2_ELF   = $(BUILD_DIR)/stage2.elf
KERNEL_ELF   = $(BUILD_DIR)/kernel.elf
DISK_IMG     = $(BUILD_DIR)/os.img
PAYLOAD_BIN  = $(BUILD_DIR)/payload.bin

# ============================================================================
# BUILD RULES
# ============================================================================
all: $(DISK_IMG)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# --- Link kernel ELF ---
$(KERNEL_ELF): $(KERNEL_OBJECTS) linker.ld | $(BUILD_DIR)
	@echo "🔗 Linking $(KERNEL_ELF)..."
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS)

# --- Compile C sources ---
-include $(addprefix $(BUILD_DIR)/, $(C_SOURCES:.c=.d))

# Generate dependency files alongside object files
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "⚙️  Compiling $<..."
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# --- Assemble stage 3 (ELF) ---
$(BUILD_DIR)/%.o: %.asm | $(BUILD_DIR)
	@echo "💻 Assembling $< (ELF)..."
	$(AS) $(ASFLAGS_ELF) $< -o $@

# --- Assemble stage 2 (binary) ---
$(STAGE2_BIN): stage2.asm | $(BUILD_DIR)
	@echo "💾 Assembling Stage 2 (BIN)..."
	$(AS) $(ASFLAGS_BIN) $< -o $@

$(STAGE2_ELF): stage2.asm | $(BUILD_DIR)
	@echo "💻 Assembling Stage 2 (ELF)..."
	$(AS) $(ASFLAGS_ELF) $< -o $@

# --- Assemble stage 1 (MBR) ---
$(STAGE1_BIN): boot.asm | $(BUILD_DIR)
	@echo "💾 Assembling Stage 1 (MBR)..."
	$(AS) $(ASFLAGS_BIN) $< -o $@

# --- Create disk image ---
$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_ELF) | $(BUILD_DIR)
	@echo "📦 Creating disk image $(DISK_IMG)..."
	$(OBJCOPY) -O binary -j .stage3 -j .bootstrap -j .text -j .data -j .bss $(KERNEL_ELF) $(PAYLOAD_BIN)

	dd if=/dev/zero of=$@ bs=512 count=55 2>/dev/null
	dd if=$(STAGE1_BIN) of=$@ bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=$(PAYLOAD_BIN) of=$@ bs=512 seek=5 iflag=fullblock count=50 2>/dev/null
	@echo "✅ Disk image created successfully!"

# ============================================================================
# RUN & DEBUG TARGETS
# ============================================================================
run: $(DISK_IMG)
	@echo "▶️  Starting QEMU (i386)..."
	qemu-system-i386 $(QEMU_FLAGS)

debug: $(DISK_IMG) $(KERNEL_ELF)
	@echo "🐛 Starting QEMU with GDB server..."
	qemu-system-i386 $(QEMU_FLAGS) -s -S &
	gdb $(KERNEL_ELF) \
		-tui \
		-ex "target remote localhost:1234" \
		-ex "set architecture i386" \
		-ex "break kmain" \
		-ex "layout src" \
		-ex "continue"

debug-bootstrap: $(DISK_IMG) $(KERNEL_ELF)
	@echo "🐛 Starting QEMU with GDB server..."
	qemu-system-i386 $(QEMU_FLAGS) -s -S &
	gdb $(KERNEL_ELF) \
		-tui \
		-ex "target remote localhost:1234" \
		-ex "set architecture i386" \
		-ex "break bootstrap_kmain" \
		-ex "layout src" \
		-ex "continue"

debug-stage3: $(DISK_IMG) $(KERNEL_ELF)
	@echo "🐛 Starting QEMU with GDB server..."
	qemu-system-i386 $(QEMU_FLAGS) -s -S &
	gdb $(KERNEL_ELF) \
		-tui \
		-ex "target remote localhost:1234" \
		-ex "set architecture i386" \
		-ex "break stage3_entry" \
		-ex "layout asm" \
		-ex "continue"

kill-qemu:
	@echo "🔪 Killing QEMU..."
	@pkill -9 qemu-system-i386 || true

# ============================================================================
# CLEAN
# ============================================================================
clean:
	@echo "🧹 Cleaning up..."
	rm -rf $(BUILD_DIR)
	@echo "✅ Clean complete."

# ============================================================================
# DEPENDENCIES (auto-generated header dependencies)
# ============================================================================
# Include auto-generated dependency files if they exist