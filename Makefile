# --- Makefile for x86 Bare-Metal OS using i686-elf- toolchain ---
.PHONY: all clean run debug kill-qemu

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
CFLAGS       = -m32 -nostdlib -nostartfiles -ffreestanding -Wall -Wextra -g -I.
ASFLAGS_BIN  = -f bin
ASFLAGS_ELF  = -f elf32 -g
LDFLAGS      = -m elf_i386 -T linker.ld
QEMU_FLAGS   = -m 4096 -serial stdio -drive format=raw,file=os.img

# ============================================================================
# SOURCE FILES
# ============================================================================
# C source files (add new .c files here)
C_SOURCES = kernel.c paging.c E820.c print_text.c
# Generated object files from C sources
C_OBJECTS = $(C_SOURCES:.c=.o)

# Assembly sources
ASM_SOURCES = stage3.asm
ASM_OBJECTS = $(ASM_SOURCES:.asm=.o)

# All object files needed for kernel
KERNEL_OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# ============================================================================
# OUTPUT FILES
# ============================================================================
STAGE1_BIN   = boot.bin
STAGE2_BIN   = boot2.bin
STAGE2_ELF   = stage2.elf
KERNEL_ELF   = kernel.elf
DISK_IMG     = os.img
PAYLOAD_BIN  = payload.bin

# ============================================================================
# BUILD RULES
# ============================================================================

all: $(DISK_IMG)

# --- Link kernel ELF ---
$(KERNEL_ELF): $(KERNEL_OBJECTS) linker.ld
	@echo "🔗 Linking $(KERNEL_ELF)..."
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJECTS)

# --- Compile C sources ---
%.o: %.c
	@echo "⚙️  Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# --- Assemble stage 3 (ELF) ---
%.o: %.asm
	@echo "💻 Assembling $< (ELF)..."
	$(AS) $(ASFLAGS_ELF) $< -o $@

# --- Assemble stage 2 (binary) ---
$(STAGE2_BIN): stage2.asm
	@echo "💾 Assembling Stage 2 (BIN)..."
	$(AS) $(ASFLAGS_BIN) $< -o $@

$(STAGE2_ELF): stage2.asm
	@echo "💻 Assembling Stage 2 (ELF)..."
	$(AS) $(ASFLAGS_ELF) $< -o $@

# --- Assemble stage 1 (MBR) ---
$(STAGE1_BIN): boot.asm
	@echo "💾 Assembling Stage 1 (MBR)..."
	$(AS) $(ASFLAGS_BIN) $< -o $@

# --- Create disk image ---
$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_ELF)
	@echo "📦 Creating disk image $(DISK_IMG)..."
	$(OBJCOPY) -O binary -j .stage3 -j .text -j .data -j .bss $(KERNEL_ELF) $(PAYLOAD_BIN)
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

kill-qemu:
	@echo "🔪 Killing QEMU..."
	@pkill -9 qemu-system-i386 || true

# ============================================================================
# CLEAN
# ============================================================================

clean:
	@echo "🧹 Cleaning up..."
	rm -f $(C_OBJECTS) $(ASM_OBJECTS) *.bin *.elf *.d $(DISK_IMG) $(PAYLOAD_BIN)
	@echo "✅ Clean complete."

# ============================================================================
# DEPENDENCIES (auto-generated header dependencies)
# ============================================================================

# Include auto-generated dependency files if they exist
-include $(C_SOURCES:.c=.d)

# Generate dependency files alongside object files
%.o: %.c
	@echo "⚙️  Compiling $<..."
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@