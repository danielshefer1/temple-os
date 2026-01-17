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
USER_LDFLAGS = -m elf_i386 -T user_linker.ld
QEMU_FLAGS   = -m 4096 -serial stdio -drive format=raw,file=$(BUILD_DIR)/os.img

# ============================================================================
# SOURCE FILES
# ============================================================================

C_SOURCES = bootstrapper.c paging_bootstrap.c E820.c vga.c kernel.c \
 slab_alloc.c paging.c math_ops.c buddy_alloc.c set_gdt.c isr_handler.c \
  set_idt.c timer.c keyboard.c global.c str_ops.c set_tss.c syscall_handler.c

USER_C_SOURCES = user_app.c

C_OBJECTS = $(addprefix $(BUILD_DIR)/, $(C_SOURCES:.c=.o))
USER_C_OBJECTS = $(addprefix $(BUILD_DIR)/, $(USER_C_SOURCES:.c=.o))

ASM_SOURCES = stage4.asm helpers.asm
USER_ASM_SOURCES = user_helpers.asm


ASM_OBJECTS = $(addprefix $(BUILD_DIR)/, $(ASM_SOURCES:.asm=.o))
USER_ASM_OBJECTS = $(addprefix $(BUILD_DIR)/, $(USER_ASM_SOURCES:.asm=.o))
# All object files needed for kernel
KERNEL_OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)
USER_OBJECTS = $(USER_ASM_OBJECTS) $(USER_C_OBJECTS)

# ============================================================================
# OUTPUT FILES
# ============================================================================
STAGE1_BIN   = $(BUILD_DIR)/boot.bin
STAGE2_BIN   = $(BUILD_DIR)/stage2.bin
STAGE3_BIN	 = $(BUILD_DIR)/stage3.bin
KERNEL_ELF   = $(BUILD_DIR)/kernel.elf
DISK_IMG     = $(BUILD_DIR)/os.img
PAYLOAD_BIN  = $(BUILD_DIR)/payload.bin
USER_ELF = $(BUILD_DIR)/user.elf
USER_BIN = $(BUILD_DIR)/user_payload.bin
FULL_PAYLOAD = $(BUILD_DIR)/full_payload.bin

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

$(USER_ELF): $(USER_OBJECTS) user_linker.ld | $(BUILD_DIR)
	@echo "🔗 Linking $(USER_ELF)..."
	$(LD) $(USER_LDFLAGS) -o $@ $(USER_C_OBJECTS) $(USER_ASM_OBJECTS)
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

# --- Assemble stage 3 (binary) ---
$(STAGE3_BIN): stage3.asm | $(BUILD_DIR)
	@echo "💾 Assembling Stage 3 (BIN)..."
	$(AS) $(ASFLAGS_BIN) $< -o $@

# --- Assemble stage 1 (MBR) ---
$(STAGE1_BIN): boot.asm | $(BUILD_DIR)
	@echo "💾 Assembling Stage 1 (MBR)..."
	$(AS) $(ASFLAGS_BIN) $< -o $@

# --- Create disk image ---
$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(STAGE3_BIN) $(KERNEL_ELF) $(USER_ELF) | $(BUILD_DIR)
	@echo "📦 Creating disk image $(DISK_IMG)..."
	$(OBJCOPY) -O binary -j .stage4 -j .bootstrap -j .helpers -j .text -j .data $(KERNEL_ELF) $(PAYLOAD_BIN)
	$(OBJCOPY) -O binary -j .text -j .data -j .bss $(USER_ELF) $(USER_BIN)

	truncate -s %512 $(PAYLOAD_BIN)
	truncate -s %4096 $(USER_BIN)

	cat $(PAYLOAD_BIN) $(USER_BIN) > $(FULL_PAYLOAD)

    # Write to disk image
	dd if=$(STAGE1_BIN) of=$@ bs=512 count=1 conv=notrunc 2>/dev/null
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	dd if=$(STAGE3_BIN) of=$@ bs=512 seek=5 conv=notrunc 2>/dev/null
	dd if=$(FULL_PAYLOAD) of=$@ bs=512 seek=9 conv=notrunc
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

debug-user: $(DISK_IMG) $(USER_ELF)
	@echo "🐛 Starting QEMU with GDB server..."
	qemu-system-i386 $(QEMU_FLAGS) -s -S &
	gdb $(USER_ELF) \
		-tui \
		-ex "target remote localhost:1234" \
		-ex "set architecture i386" \
		-ex "break main" \
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