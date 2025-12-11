# --- Makefile for x86 Bare-Metal OS using i686-elf- toolchain ---

.PHONY: all clean run build_kernel

# Define cross-compiler variables
CC = i686-elf-gcc
AS = nasm
LD = i686-elf-ld
OBJCOPY = i686-elf-objcopy

# Compiler Flags for C kernel (32-bit, bare-metal environment)
CFLAGS = -m32 -nostdlib -nostartfiles -ffreestanding -Wall -Wextra -g -I.
# Assembler Flags for Stage 1/2 (flat binary)
ASFLAGS_BIN = -f bin
# Assembler Flags for Stage 3 (ELF object file)
ASFLAGS_ELF = -f elf
# Linker Flags (using the custom linker script)
LDFLAGS = -m elf_i386 -T linker.ld

# Output files
STAGE1_BIN = boot.bin
STAGE2_BIN = boot2.bin
STAGE3_OBJ = stage3.o
KERNEL_OBJ = kernel.o
KERNEL_ELF = kernel.elf
DISK_IMG = os.img
PAYLOAD_BIN = payload.bin

# --- Main Target ---
all: $(DISK_IMG)

# --- 1. KERNEL ELF LINKING ---
# Links stage3.o and kernel.o, resolving symbols and applying the memory map from linker.ld.
$(KERNEL_ELF): $(STAGE3_OBJ) $(KERNEL_OBJ) linker.ld
	@echo "🔗 Linking $(KERNEL_ELF)..."
	$(LD) $(LDFLAGS) -o $@ $(filter %.o,$^)

# --- 2. COMPILE C KERNEL ---
$(KERNEL_OBJ): kernel.c
	@echo "⚙️ Compiling C kernel..."
	$(CC) $(CFLAGS) -c $< -o $@

# --- 3. COMPILE STAGE 3 ASM ---
# Stage 3 is compiled to an ELF object for linker use.
$(STAGE3_OBJ): stage3.asm
	@echo "💻 Assembling Stage 3 (ELF)..."
	$(AS) $(ASFLAGS_ELF) $< -o $@

# --- 4. COMPILE STAGE 2 ASM ---
# Stage 2 is a flat binary (4 sectors).
$(STAGE2_BIN): stage2.asm
	@echo "💾 Assembling Stage 2 (BIN)..."
	$(AS) $(ASFLAGS_BIN) $< -o $@

# --- 5. COMPILE STAGE 1 ASM ---
# Stage 1 (boot.asm) is assumed to be 1 sector (MBR).
$(STAGE1_BIN): boot.asm
	@echo "💾 Assembling Stage 1 (MBR)..."
	$(AS) $(ASFLAGS_BIN) $< -o $@

# --- 6. CREATE DISK IMAGE ---
$(DISK_IMG): $(STAGE1_BIN) $(STAGE2_BIN) $(KERNEL_ELF)
	@echo "📦 Creating disk image $(DISK_IMG)..."
	
	# 1. Extract the raw binary payload (Stage 3 + Kernel data sections)
	# This strips the ELF metadata, giving us the contiguous data block.
	$(OBJCOPY) -O binary -j .text -j .data -j .bss $(KERNEL_ELF) $(PAYLOAD_BIN)
	
	# 2. Write Stage 1 (1 sector) to the disk start (Sector 1, seek=0)
	dd if=$(STAGE1_BIN) of=$@ bs=512 count=1 conv=notrunc
	
	# 3. Append Stage 2 (4 sectors) immediately after. (Sectors 2-5, seek=1)
	dd if=$(STAGE2_BIN) of=$@ bs=512 seek=1 conv=notrunc
	
	# 4. Append the raw payload (Stage 3/Kernel) starting at Sector 6 (seek=5)
	# This is the data that Stage 2 reads starting at disk sector 6 into memory 0x8600.
	dd if=$(PAYLOAD_BIN) of=$@ bs=512 seek=5 conv=notrunc
	
	@rm -f $(PAYLOAD_BIN) 
	@echo "✅ Disk image created successfully!" 

# --- RUN QEMU ---
run: $(DISK_IMG)
	@echo "▶️ Starting QEMU (i386)..."
	qemu-system-i386 -drive format=raw,file=$(DISK_IMG) -serial stdio

# --- CLEAN UP ---
clean:
	@echo "🧹 Cleaning up..."
	rm -f *.bin *.o *.elf $(DISK_IMG)
	@echo "✅ Clean complete." 