# Self-contained build of the user-mode programs that ship in /bin on
# data.img. Invoked from the root Makefile via `$(MAKE) -C user`, but
# also runs standalone (`cd user && make`) using the defaults below.

BUILD_DIR ?= ../build
USER_DIR  ?= $(BUILD_DIR)/user
DATA_IMG  ?= ../data.img
FONT_PSF  ?= ../assets/font.psf

CC64 = x86_64-elf-gcc
LD64 = x86_64-elf-ld

CFLAGS  = -m64 -static -fPIE -ffreestanding -nostdlib -nostartfiles \
          -fno-stack-protector -mno-red-zone -mno-sse -mno-mmx -mno-sse2 \
          -fno-asynchronous-unwind-tables -Wall -Wextra -O2 -I . \
          -MMD -MP
LDFLAGS = -m elf_x86_64 -static -pie -nostdlib -T hello_linker.ld \
          -z noexecstack --no-warn-rwx-segments

APPS  := $(patsubst %.c,%,$(notdir $(wildcard *.c)))
ELVES := $(addprefix $(USER_DIR)/,$(addsuffix .elf,$(APPS)))
DEPS  := $(addprefix $(USER_DIR)/,$(addsuffix .d,$(APPS)))

FONT_OBJ    := $(USER_DIR)/font.o
EXTRAS_term := $(FONT_OBJ)

.PHONY: all inject clean
all: $(ELVES)

$(USER_DIR):
	@mkdir -p $@

# Compile and link are separate so the .o is a real make target. The
# .d file emitted by -MMD names the .o, so make can use the recorded
# header deps to decide when to recompile.
$(USER_DIR)/%.o: %.c | $(USER_DIR)
	@echo "[USER] cc $<"
	@$(CC64) $(CFLAGS) -c $< -o $@

$(USER_DIR)/%.elf: $(USER_DIR)/%.o hello_linker.ld
	@echo "[USER] ld $@"
	@$(LD64) $(LDFLAGS) -o $@ $< $(EXTRAS_$*)

# Apps that link extra objects must list them as additional prereqs so
# make builds the extras before the link step.
$(USER_DIR)/term.elf: $(FONT_OBJ)

$(FONT_OBJ): $(FONT_PSF) | $(USER_DIR)
	@echo "[USER] embed $<"
	@cd $(dir $<) && objcopy -I binary -O elf64-x86-64 -B i386:x86-64 \
	    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
	    $(notdir $<) $(abspath $@)

inject: $(ELVES) | $(DATA_IMG)
	@echo "[USER] inject -> $(DATA_IMG):/bin/"
	@e2mkdir $(DATA_IMG):/bin 2>/dev/null || true
	@$(foreach a,$(APPS), \
	    e2cp -O 0 -G 0 -P 755 \
	        $(USER_DIR)/$(a).elf $(DATA_IMG):/bin/$(a) ;)

clean:
	rm -rf $(USER_DIR)

-include $(DEPS)
