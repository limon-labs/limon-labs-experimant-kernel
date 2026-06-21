# =============================================================
#  Makefile  —  MyOS kernel build system
#
#  Targets:
#    make          → build everything (kernel.elf + os-image.iso)
#    make run      → launch in QEMU
#    make debug    → launch QEMU with GDB server on :1234
#    make clean    → remove all build artefacts
#    make dump     → disassemble kernel.elf
#
#  Requirements:
#    nasm, i686-elf-gcc (or gcc with -m32), i686-elf-ld,
#    grub-mkrescue, xorriso, qemu-system-i386
# =============================================================

# ── Toolchain ─────────────────────────────────────────────────
AS        := nasm
CC        := i686-elf-gcc
LD        := i686-elf-ld
OBJCOPY   := i686-elf-objcopy
OBJDUMP   := i686-elf-objdump
GRUB_MKRESCUE := grub-mkrescue

# Fallback: if cross-compiler not found, use host gcc with -m32
ifeq ($(shell which $(CC) 2>/dev/null),)
    CC      := gcc
    LD      := ld
    OBJCOPY := objcopy
    OBJDUMP := objdump
    LDFLAGS_EXTRA := -melf_i386
endif

# ── Flags ─────────────────────────────────────────────────────
ASFLAGS   := -f elf32 -g
CFLAGS    := -m32 -std=c11 \
             -ffreestanding -fno-stack-protector -fno-builtin \
             -fno-pie -fno-pic \
             -Wall -Wextra -Werror \
             -O2 \
             -I./include

LDFLAGS   := -T linker.ld -m elf_i386 $(LDFLAGS_EXTRA) \
             --nmagic

# ── Directories ───────────────────────────────────────────────
BUILD_DIR := build
ISO_DIR   := iso
ISO_BOOT  := $(ISO_DIR)/boot
ISO_GRUB  := $(ISO_BOOT)/grub

# ── Sources → objects ─────────────────────────────────────────
ASM_SRCS  := boot/boot.asm
C_SRCS    := kernel/kernel.c \
             kernel/screen.c  \
             kernel/memory.c

ASM_OBJS  := $(patsubst %.asm,$(BUILD_DIR)/%.o,$(ASM_SRCS))
C_OBJS    := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))

ALL_OBJS  := $(ASM_OBJS) $(C_OBJS)

# ── Output artefacts ──────────────────────────────────────────
KERNEL_ELF := kernel.elf
KERNEL_BIN := kernel.bin
ISO_IMAGE  := os-image.iso

# ══════════════════════════════════════════════════════════════
# Default target
# ══════════════════════════════════════════════════════════════
.PHONY: all
all: $(ISO_IMAGE)
	@echo ""
	@echo "  Build complete!"
	@echo "  ELF  : $(KERNEL_ELF)"
	@echo "  BIN  : $(KERNEL_BIN)"
	@echo "  ISO  : $(ISO_IMAGE)"
	@echo ""
	@echo "  Run with:  make run"

# ── Link kernel ───────────────────────────────────────────────
$(KERNEL_ELF): $(ALL_OBJS) linker.ld
	@echo "  LD    $@"
	@$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)

# ── Raw binary (optional) ─────────────────────────────────────
$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "  BIN   $@"
	@$(OBJCOPY) -O binary $< $@

# ── GRUB bootable ISO ─────────────────────────────────────────
$(ISO_IMAGE): $(KERNEL_ELF) grub.cfg
	@echo "  ISO   $@"
	@mkdir -p $(ISO_GRUB)
	@cp $(KERNEL_ELF) $(ISO_BOOT)/kernel.elf
	@cp grub.cfg      $(ISO_GRUB)/grub.cfg
	@$(GRUB_MKRESCUE) -o $@ $(ISO_DIR) 2>/dev/null
	@rm -rf $(ISO_DIR)

# ── Assemble boot.asm ─────────────────────────────────────────
$(BUILD_DIR)/boot/%.o: boot/%.asm
	@mkdir -p $(dir $@)
	@echo "  AS    $<"
	@$(AS) $(ASFLAGS) $< -o $@

# ── Compile C sources ─────────────────────────────────────────
$(BUILD_DIR)/kernel/%.o: kernel/%.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

# ── Generate grub.cfg on the fly if missing ───────────────────
grub.cfg:
	@echo "  CFG   $@"
	@printf 'set default=0\nset timeout=3\nmenuentry "MyOS" {\n    multiboot2 /boot/kernel.elf\n    boot\n}\n' > $@

# ── QEMU ──────────────────────────────────────────────────────
.PHONY: run
run: $(ISO_IMAGE)
	@echo "  QEMU  $(ISO_IMAGE)"
	qemu-system-i386 \
	    -cdrom $(ISO_IMAGE) \
	    -m 128M \
	    -serial stdio \
	    -no-reboot \
	    -no-shutdown

.PHONY: debug
debug: $(ISO_IMAGE)
	@echo "  QEMU  debug — GDB on localhost:1234"
	qemu-system-i386 \
	    -cdrom $(ISO_IMAGE) \
	    -m 128M \
	    -serial stdio \
	    -no-reboot \
	    -no-shutdown \
	    -s -S &
	@echo "  GDB   connect with:  gdb $(KERNEL_ELF)"
	@echo "        then:          target remote :1234"
	@echo "        and:           continue"

# ── Disassembly dump ──────────────────────────────────────────
.PHONY: dump
dump: $(KERNEL_ELF)
	$(OBJDUMP) -d -M intel $(KERNEL_ELF) | less

.PHONY: symbols
symbols: $(KERNEL_ELF)
	$(OBJDUMP) -t $(KERNEL_ELF) | sort

# ── Size report ───────────────────────────────────────────────
.PHONY: size
size: $(KERNEL_ELF)
	@i686-elf-size $(KERNEL_ELF)

# ── Clean ─────────────────────────────────────────────────────
.PHONY: clean
clean:
	@echo "  CLEAN"
	@rm -rf $(BUILD_DIR) $(ISO_DIR)
	@rm -f  $(KERNEL_ELF) $(KERNEL_BIN) $(ISO_IMAGE) grub.cfg

# ── Dependency auto-generation ────────────────────────────────
-include $(ALL_OBJS:.o=.d)

$(BUILD_DIR)/kernel/%.d: kernel/%.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -MM -MT $(@:.d=.o) $< > $@
