# ============================================================================
# Toolchains
# ============================================================================
CC64 = x86_64-elf-gcc
LD64 = x86_64-elf-ld
AS   = nasm

# ============================================================================
# Paths and Filenames
# ============================================================================
BUILD_DIR = build
K_OBJ_DIR = $(BUILD_DIR)/kernel
ISO_ROOT  = $(BUILD_DIR)/iso_root

KERNEL_ELF     = $(BUILD_DIR)/kernel.elf
ISO_IMG        = $(BUILD_DIR)/os.iso
DATA_IMG       = data.img
TRAMPOLINE_BIN = $(BUILD_DIR)/trampoline.bin

LIMINE_DIR = boot/limine
LIMINE_BIN = $(LIMINE_DIR)/limine

# ============================================================================
# Flags
# ============================================================================
COMMON_CFLAGS = -nostdlib -nostartfiles -ffreestanding -Wall -Wextra -g -fno-pic -fno-pie
INCDIRS = -I ./allocaters -I ./boot -I ./boot/limine -I ./drivers -I ./file_system \
          -I ./init -I ./interrupts -I ./loader -I ./multi -I ./paging -I ./tables -I ./user \
          -I ./util -I ./wrappers

K_CFLAGS  = $(COMMON_CFLAGS) -m64 -mcmodel=kernel -mno-red-zone -mno-sse -mno-mmx -mno-sse2 $(INCDIRS)
K_LDFLAGS = -m elf_x86_64 -T linker64.ld

ASFLAGS_ELF64 = -f elf64
ASFLAGS_BIN   = -f bin

QEMU_COMMON_FLAGS = -m 16G -cpu host,+topoext -accel kvm -smp 12 -machine q35 \
                    -drive index=1,format=raw,file=$(DATA_IMG) \
                    -rtc clock=host,driftfix=slew \
                    -serial stdio

# ============================================================================
# Source & Object Definitions
# ============================================================================
KERNEL_C_SRCS = drivers/E820.c drivers/vga.c drivers/tty.c drivers/devfs.c \
                drivers/mem_devs.c drivers/ram_block.c drivers/disk_devs.c init/kernel.c init/limine_entry.c \
                allocaters/slab_alloc.c paging/paging.c util/math.c allocaters/buddy_alloc.c \
                tables/set_gdt.c interrupts/isr_handler.c tables/set_idt.c wrappers/timer.c \
                wrappers/keyboard.c util/global.c util/string.c interrupts/syscall_handler.c \
                file_system/vfs.c file_system/dcache.c drivers/acpi.c util/memory.c \
                drivers/apic.c interrupts/irq_handler.c util/utility.c multi/ap_start.c \
                multi/ap_main.c drivers/pci.c drivers/ahci_driver.c file_system/mbr.c \
                file_system/ext2_sb_ops.c drivers/rtc.c drivers/fadt.c \
                file_system/ext2_ino_ops.c file_system/blocks_buffer.c \
                file_system/ext2_helpers.c file_system/ext2_file_ops.c \
                file_system/vfs_sb.c file_system/vfs_inode.c file_system/vfs_file.c \
                file_system/vfs_dentry.c file_system/vfs_mount.c file_system/vfs_path.c \
                file_system/vfs_path_ops.c \
                interrupts/fd_table.c interrupts/vfs_syscalls.c \
                multi/cpu_local.c init/user_launch.c multi/scheduler.c multi/mutex.c \
                paging/pml4_clone.c loader/elf64.c multi/user_task.c \
                multi/fork.c multi/signal.c multi/waitpid.c

KERNEL_ASM_SRCS = util/helpers.asm multi/trampoline_wrapper.asm interrupts/syscall_entry.asm \
                  interrupts/user_enter.asm multi/switch.asm multi/user_trampoline.asm

K_OBJS = $(addprefix $(K_OBJ_DIR)/, $(KERNEL_C_SRCS:.c=.o) $(KERNEL_ASM_SRCS:.asm=.o))

# ============================================================================
# Build Rules
# ============================================================================
USER_DIR     = $(BUILD_DIR)/user
USER_HELLO   = $(USER_DIR)/hello.elf
USER_CFLAGS  = -m64 -static -fPIE -ffreestanding -nostdlib -nostartfiles \
               -fno-stack-protector -mno-red-zone -mno-sse -mno-mmx -mno-sse2 \
               -fno-asynchronous-unwind-tables -Wall -Wextra -O2 -I ./user
USER_LDFLAGS = -m elf_x86_64 -static -pie -nostdlib -T user/hello_linker.ld

all: $(ISO_IMG) $(DATA_IMG) user-img

$(DATA_IMG):
	@echo "Creating persistent data disk..."
	@dd if=/dev/zero of=$(DATA_IMG) bs=1G count=1
	@mke2fs -t ext2 -L "TEMPLE_OS_ROOT" $(DATA_IMG)
	@echo "$(DATA_IMG) is ready."

# --- User Programs ---
$(USER_DIR):
	@mkdir -p $@

$(USER_HELLO): user/hello.c user/syscall_inline.h user/hello_linker.ld | $(USER_DIR)
	@echo "[USER] Building $@"
	@$(CC64) $(USER_CFLAGS) -c user/hello.c -o $(USER_DIR)/hello.o
	@$(LD64) $(USER_LDFLAGS) -o $@ $(USER_DIR)/hello.o

# Install built user programs into data.img (idempotent; rm-then-write).
user-img: $(USER_HELLO) $(DATA_IMG)
	@echo "[USER] Installing $(USER_HELLO) -> /hello on $(DATA_IMG)"
	@debugfs -w -R "rm /hello" $(DATA_IMG) 2>/dev/null || true
	@debugfs -w -R "write $(USER_HELLO) hello" $(DATA_IMG)

# /dev is populated at runtime by drivers/disk_devs.c: it mkdirs /dev
# and mknods each char/block node (tty, null, zero, ram0, sda, sda*) once
# the root filesystem is mounted. First boot persists them onto data.img;
# later boots see EEXIST and reuse the existing entries.

# --- Kernel Rules ---
$(K_OBJ_DIR)/%.o: %.c | $(K_OBJ_DIR)
	@mkdir -p $(dir $@)
	@echo "[K64] Compiling $<"
	@$(CC64) $(K_CFLAGS) -MMD -MP -c $< -o $@

$(K_OBJ_DIR)/%.o: %.asm | $(K_OBJ_DIR) $(TRAMPOLINE_BIN)
	@mkdir -p $(dir $@)
	@echo "[K64] Assembling $<"
	@$(AS) $(ASFLAGS_ELF64) $< -o $@

$(KERNEL_ELF): $(K_OBJS) linker64.ld
	@echo "Linking Kernel ELF"
	@$(LD64) $(K_LDFLAGS) -o $@ $(K_OBJS)

$(TRAMPOLINE_BIN): multi/trampoline.asm | $(BUILD_DIR)
	@$(AS) $(ASFLAGS_BIN) $< -o $@

# --- Limine Helper ---
$(LIMINE_BIN):
	@$(MAKE) -s -C $(LIMINE_DIR)

# --- ISO Image ---
$(ISO_IMG): $(KERNEL_ELF) $(LIMINE_BIN) boot/limine.conf | $(BUILD_DIR)
	@echo "Building ISO image"
	@rm -rf $(ISO_ROOT)
	@mkdir -p $(ISO_ROOT)/boot/limine $(ISO_ROOT)/EFI/BOOT
	@cp $(KERNEL_ELF) $(ISO_ROOT)/boot/kernel.elf
	@cp boot/limine.conf $(ISO_ROOT)/boot/limine/limine.conf
	@cp $(LIMINE_DIR)/limine-bios.sys \
	    $(LIMINE_DIR)/limine-bios-cd.bin \
	    $(LIMINE_DIR)/limine-uefi-cd.bin \
	    $(ISO_ROOT)/boot/limine/
	@cp $(LIMINE_DIR)/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/
	@xorriso -as mkisofs -R -r -J \
	    -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
	    -apm-block-size 2048 \
	    --efi-boot boot/limine/limine-uefi-cd.bin \
	    -efi-boot-part --efi-boot-image --protective-msdos-label \
	    $(ISO_ROOT) -o $(ISO_IMG)
	@$(LIMINE_BIN) bios-install $(ISO_IMG)
	@echo "ISO ready: $(ISO_IMG)"

# ============================================================================
# Utilities
# ============================================================================
$(BUILD_DIR) $(K_OBJ_DIR):
	@mkdir -p $@

clean-raw:
	rm -rf $(BUILD_DIR)

clean-limine-raw:
	$(MAKE) -s -C $(LIMINE_DIR) clean

clean-data-raw:
	rm -f data.img

clean-all-raw: clean-raw clean-data-raw clean-limine-raw



clean:
	./docker-build.sh make clean-raw

clean-limine:
	./docker-build.sh make clean-limine-raw

clean-data:
	./docker-build.sh make clean-data-raw

clean-all:
	./docker-build.sh make clean-all-raw

run: $(ISO_IMG) $(DATA_IMG) user-img
	qemu-system-x86_64 $(QEMU_COMMON_FLAGS) -cdrom $(ISO_IMG)

run-uefi: $(ISO_IMG) $(DATA_IMG)
	qemu-system-x86_64 $(QEMU_COMMON_FLAGS) -bios /usr/share/edk2/x64/OVMF.4m.fd -cdrom $(ISO_IMG)

debug: $(ISO_IMG) $(KERNEL_ELF) $(DATA_IMG)
	@echo "Serial on tcp:127.0.0.1:4444 — attach from another terminal with: nc localhost 4444"
	qemu-system-x86_64 $(filter-out -serial stdio,$(QEMU_COMMON_FLAGS)) \
		-serial tcp:127.0.0.1:4444,server,nowait -cdrom $(ISO_IMG) -s -S &
	gdb $(KERNEL_ELF) \
		-ex "target remote localhost:1234" \
		-ex "set pagination off" \
		-ex "set architecture x86-64" \
		-ex "layout src" \
		-ex "hbreak kmain" \
		-ex "continue"

# Include dependencies
-include $(K_OBJS:.o=.d)
