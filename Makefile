AS          := nasm
CC          := i686-elf-gcc
LD          := i686-elf-ld
OBJCOPY     := i686-elf-objcopy
OBJDUMP     := i686-elf-objdump

ifeq ($(shell which $(CC) 2>/dev/null),)
    CC      := gcc
    LD      := ld
    OBJCOPY := objcopy
    OBJDUMP := objdump
    LDFLAGS_EXTRA := -melf_i386
endif

ASFLAGS   := -f elf32 -g
CFLAGS    := -m32 -std=c11 \
             -ffreestanding -fno-stack-protector -fno-builtin \
             -fno-pie -fno-pic \
             -Wall -Wextra \
             -O2 \
             -I./include \
             -I./include/kernel

LDFLAGS   := -T linker/x86/linker.ld -m elf_i386 $(LDFLAGS_EXTRA) \
             --nmagic

BUILD_DIR := build

BOOT_SRCS := boot/x86/boot_sector.asm \
             boot/x86/second_stage.asm

ASM_SRCS  := boot/x86/kernel_entry.asm \
             arch/x86/isr/isr.asm \
             arch/x86/irq/irq.asm

C_SRCS    := kernel/core/kernel.c \
             kernel/core/init.c \
             kernel/core/panic.c \
             kernel/core/logging.c \
             kernel/core/diagnostics.c \
             kernel/core/lib.c \
             kernel/memory/pmm/pmm.c \
             kernel/memory/vmm/vmm.c \
             kernel/memory/heap/heap.c \
             kernel/scheduler/scheduler.c \
             kernel/sync/mutex.c \
             kernel/sync/spinlock.c \
             kernel/sync/semaphore.c \
             kernel/time/time.c \
             arch/x86/gdt/gdt.c \
             arch/x86/idt/idt.c \
             arch/x86/isr/isr.c \
             arch/x86/irq/irq.c \
             arch/x86/cpu/cpu.c \
             hal/cpu/io.c \
             hal/interrupt/pic.c \
             hal/timer/pit.c \
             drivers/display/screen.c \
             drivers/keyboard/keyboard.c \
             cis/core/cis.c

BOOT_SECTOR_BIN   := build/boot_sector.bin
SECOND_STAGE_BIN  := build/second_stage.bin

ASM_OBJS  := $(patsubst %.asm,$(BUILD_DIR)/%.o,$(ASM_SRCS))
C_OBJS    := $(patsubst %.c,$(BUILD_DIR)/%.o,$(C_SRCS))
ALL_OBJS  := $(ASM_OBJS) $(C_OBJS)

KERNEL_ELF := kernel.elf
KERNEL_BIN := kernel.bin
DISK_IMAGE := ahk-os.img

.PHONY: all
all: $(DISK_IMAGE)
	@echo ""
	@echo "  =========================================="
	@echo "  |     AHK OS Production Build!           |"
	@echo "  |     Self-Healing Kernel (CIS)          |"
	@echo "  =========================================="
	@echo ""
	@echo "  Kernel ELF       : $(KERNEL_ELF)"
	@echo "  Boot Sector      : $(BOOT_SECTOR_BIN)"
	@echo "  Second Stage     : $(SECOND_STAGE_BIN)"
	@echo "  Disk Image       : $(DISK_IMAGE)"
	@echo ""
	@echo "  Run with:  make run"

$(KERNEL_ELF): $(ALL_OBJS) linker/x86/linker.ld
	@echo "  LD    $@"
	@$(LD) $(LDFLAGS) -o $@ $(ALL_OBJS)

$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "  BIN   $@"
	@$(OBJCOPY) -O binary $< $@

$(BOOT_SECTOR_BIN): boot/x86/boot_sector.asm
	@mkdir -p $(dir $@)
	@echo "  BOOT  $@"
	@nasm -f bin $< -o $@

$(SECOND_STAGE_BIN): boot/x86/second_stage.asm
	@mkdir -p $(dir $@)
	@echo "  STAGE $@"
	@nasm -f bin $< -o $@

$(DISK_IMAGE): $(BOOT_SECTOR_BIN) $(SECOND_STAGE_BIN) $(KERNEL_BIN)
	@echo "  IMG   $@"
	@dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	@dd if=$(BOOT_SECTOR_BIN) of=$@ conv=notrunc 2>/dev/null
	@dd if=$(SECOND_STAGE_BIN) of=$@ seek=1 conv=notrunc 2>/dev/null
	@dd if=$(KERNEL_BIN) of=$@ seek=21 conv=notrunc 2>/dev/null

$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "  AS    $<"
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@

.PHONY: run
run: $(DISK_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMAGE),if=floppy -m 256M -serial stdio -no-reboot -no-shutdown

.PHONY: debug
debug: $(DISK_IMAGE)
	qemu-system-i386 -drive format=raw,file=$(DISK_IMAGE),if=floppy -m 256M -serial stdio -no-reboot -no-shutdown -s -S &
	@echo "  GDB   connect with:  gdb $(KERNEL_ELF)"
	@echo "        then:          target remote :1234"
	@echo "        and:           continue"

.PHONY: clean
clean:
	@rm -rf $(BUILD_DIR)
	@rm -f  $(KERNEL_ELF) $(KERNEL_BIN) $(BOOT_SECTOR_BIN) $(SECOND_STAGE_BIN) $(DISK_IMAGE)
