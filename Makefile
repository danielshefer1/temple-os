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
KERNEL_C_SRCS = drivers/E820.c drivers/vga.c drivers/fb.c drivers/fb_console.c drivers/vt.c drivers/tty.c drivers/tty_ldisc.c drivers/pty.c drivers/fb_dev.c drivers/kbd_dev.c drivers/devfs.c \
                drivers/mem_devs.c drivers/ram_block.c drivers/disk_devs.c drivers/procfs.c init/kernel.c init/limine_entry.c \
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
                file_system/vfs_path_ops.c file_system/pipe.c \
                interrupts/fd_table.c interrupts/vfs_syscalls.c \
                multi/cpu_local.c init/user_launch.c multi/scheduler.c multi/mutex.c \
                paging/pml4_clone.c loader/elf64.c multi/user_task.c \
                multi/fork.c multi/signal.c multi/waitpid.c

KERNEL_ASM_SRCS = util/helpers.asm multi/trampoline_wrapper.asm interrupts/syscall_entry.asm \
                  interrupts/user_enter.asm multi/switch.asm multi/user_trampoline.asm

FONT_PSF  = assets/font.psf
FONT_OBJ  = $(K_OBJ_DIR)/font.o

K_OBJS = $(addprefix $(K_OBJ_DIR)/, $(KERNEL_C_SRCS:.c=.o) $(KERNEL_ASM_SRCS:.asm=.o)) $(FONT_OBJ)

# ============================================================================
# Build Rules
# ============================================================================
USER_DIR     = $(BUILD_DIR)/user
USER_HELLO   = $(USER_DIR)/hello.elf
USER_TERM    = $(USER_DIR)/term.elf
USER_INIT    = $(USER_DIR)/init.elf
USER_SH      = $(USER_DIR)/sh.elf
USER_HELP    = $(USER_DIR)/help.elf
USER_CLEAR   = $(USER_DIR)/clear.elf
USER_PWD     = $(USER_DIR)/pwd.elf
USER_LS      = $(USER_DIR)/ls.elf
USER_CAT     = $(USER_DIR)/cat.elf
USER_ECHO    = $(USER_DIR)/echo.elf
USER_FONT_OBJ = $(USER_DIR)/font.o

# All "small" user programs that share the same build recipe (no font
# blob, single .c file under user/, libu.h-based runtime).
USER_SMALL = $(USER_HELLO) $(USER_INIT) $(USER_SH) $(USER_HELP) \
             $(USER_CLEAR) $(USER_PWD) $(USER_LS) $(USER_CAT) $(USER_ECHO)
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

# Pattern rule for the simple user programs. Each .elf links from a single
# user/<name>.c that includes user/libu.h (which provides the SysV-aware
# _start that pulls argc/argv off the stack and tail-calls main).
$(USER_DIR)/%.elf: user/%.c user/syscall_inline.h user/libu.h user/sys/wait.h user/sys/dirent.h user/hello_linker.ld | $(USER_DIR)
	@echo "[USER] Building $@"
	@$(CC64) $(USER_CFLAGS) -c $< -o $(USER_DIR)/$*.o
	@$(LD64) $(USER_LDFLAGS) -o $@ $(USER_DIR)/$*.o

# Embed assets/font.psf as a binary blob into user/term so the userspace
# rasterizer can use the same glyph table as the kernel fb_console.
$(USER_FONT_OBJ): $(FONT_PSF) | $(USER_DIR)
	@echo "[USER] Embedding $<"
	@cd $(dir $<) && objcopy -I binary -O elf64-x86-64 -B i386:x86-64 \
	    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
	    $(notdir $<) $(abspath $@)

# /bin/term has its own rule because it links the embedded font blob.
$(USER_TERM): user/term.c user/syscall_inline.h user/hello_linker.ld $(USER_FONT_OBJ) | $(USER_DIR)
	@echo "[USER] Building $@"
	@$(CC64) $(USER_CFLAGS) -c user/term.c -o $(USER_DIR)/term.o
	@$(LD64) $(USER_LDFLAGS) -o $@ $(USER_DIR)/term.o $(USER_FONT_OBJ)

# Install built user programs into data.img (idempotent; rm-then-write).
USER_INSTALL = $(USER_TERM) $(USER_SMALL)

define INSTALL_BIN
	@echo "[USER] Installing $(1) -> /bin/$(2) on $(DATA_IMG)"
	@debugfs -w -R "rm /bin/$(2)" $(DATA_IMG) 2>/dev/null || true
	@debugfs -w -R "write $(1) /bin/$(2)" $(DATA_IMG)
endef

user-img: $(USER_INSTALL) $(DATA_IMG)
	@debugfs -w -R "mkdir /bin" $(DATA_IMG) 2>/dev/null || true
	$(call INSTALL_BIN,$(USER_HELLO),hello)
	$(call INSTALL_BIN,$(USER_TERM),term)
	$(call INSTALL_BIN,$(USER_INIT),init)
	$(call INSTALL_BIN,$(USER_SH),sh)
	$(call INSTALL_BIN,$(USER_HELP),help)
	$(call INSTALL_BIN,$(USER_CLEAR),clear)
	$(call INSTALL_BIN,$(USER_PWD),pwd)
	$(call INSTALL_BIN,$(USER_LS),ls)
	$(call INSTALL_BIN,$(USER_CAT),cat)
	$(call INSTALL_BIN,$(USER_ECHO),echo)

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

$(FONT_OBJ): $(FONT_PSF) | $(K_OBJ_DIR)
	@mkdir -p $(dir $@)
	@echo "[K64] Embedding $<"
	@cd $(dir $<) && objcopy -I binary -O elf64-x86-64 -B i386:x86-64 \
	    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
	    $(notdir $<) $(abspath $@)

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
