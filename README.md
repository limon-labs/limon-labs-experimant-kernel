# limon-labs-experimant-kernel

Simple x86 hobby kernel — an educational Multiboot2‑compliant 32‑bit (i386 protected mode) kernel. Purpose: to learn and experiment with basic kernel subsystems (boot stub, GDT/IDT, PIC, PIT, physical/virtual memory, PS/2 keyboard, VGA output, a small kmalloc and a minimal interactive shell).

Intended as a learning/hobby project — not production‑grade. Code is written for clarity.

## Key features
- Multiboot2 header and boot assembly (boot.asm)
- 32‑bit protected mode kernel entry and basic runtime
- GDT and IDT setup, ISR/IRQ stubs and PIC remapping
- PIT timer and PS/2 keyboard driver
- Physical memory manager (PMM) and virtual memory/paging (VMM)
- Simple kernel heap (kmalloc) and kmalloc stats
- VGA screen output (kprintf) and a tiny interactive kernel shell

## Stack
- Languages: C (primary), x86 Assembly
- Target: i386 (32‑bit), Multiboot2
- Build: Makefile + GNU toolchain (cross or host with -m32)
- Runtime tooling: QEMU (qemu-system-i386), GRUB (grub-mkrescue) — ISO booted via GRUB

## Repository layout (short)
- Makefile       — build rules, run/debug/clean targets
- boot.asm       — Multiboot2 header, BSS zeroing, asm helpers, ISR/IRQ stubs
- linker.ld      — linker script (layout and symbols like __kernel_end)
- kernel.c       — kernel_main(), subsystem init and minimal shell
- kernel.h       — master kernel header: types, macros and APIs
- memory.c       — physical/virtual memory implementation
- screen.c       — VGA and kprintf implementation
- README.md      — this file

How it fits together:
- boot.asm provides the Multiboot2 header, clears BSS, installs a GDT and then calls kernel_main (C).
- kernel_main (kernel.c) initializes subsystems in a typical order: screen → parse multiboot tags → GDT → IDT → PIC → PIT → PMM → VMM → heap → keyboard → enable interrupts → interactive shell.
- linker.ld defines section placement and exports symbols used by the kernel (e.g., __kernel_end), which are used to place the heap.

## Quick start (shortest path)
Required tools (per the Makefile):
- nasm
- i686-elf-gcc, i686-elf-ld, i686-elf-objcopy, i686-elf-objdump (or host gcc/ld + -m32)
- grub-mkrescue, xorriso
- qemu-system-i386

Common commands:
```bash
# build everything (kernel.elf + os-image.iso)
make

# run in QEMU
make run

# run in QEMU with GDB server on :1234
make debug

# clean build artifacts
make clean
```

Note: The Makefile contains a fallback that uses host gcc/ld if the cross‑toolchain isn't found; however, host toolchains may need appropriate 32‑bit support installed.

## Makefile path mismatch — quick fix
If `make` fails to find sources, it's because the Makefile expects sources under `boot/` and `kernel/` directories (e.g. `boot/boot.asm`, `kernel/kernel.c`), while the repository currently places the files at the repository root (`boot.asm`, `kernel.c`, `screen.c`, `memory.c`).

Two quick ways to fix this:

Option A — update the Makefile (recommended if you want to keep files at repo root).
Replace these lines in the Makefile:
```makefile
ASM_SRCS  := boot/boot.asm
C_SRCS    := kernel/kernel.c \
             kernel/screen.c  \
             kernel/memory.c
```
with:
```makefile
ASM_SRCS  := boot.asm
C_SRCS    := kernel.c \
             screen.c \
             memory.c
```
And replace the pattern rules:
```makefile
$(BUILD_DIR)/boot/%.o: boot/%.asm
    ...
$(BUILD_DIR)/kernel/%.o: kernel/%.c
    ...
```
with generic rules that compile root-level sources:
```makefile
$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "  AS    $<"
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "  CC    $<"
	@$(CC) $(CFLAGS) -c $< -o $@
```

Option B — move files into the expected subdirectories.
Create `boot/` and `kernel/` directories and move files:
```bash
mkdir -p boot kernel
git mv boot.asm boot/boot.asm
git mv kernel.c kernel/kernel.c
git mv screen.c kernel/screen.c
git mv memory.c kernel/memory.c
git commit -m "Move kernel sources into kernel/ and boot/"
```
Either option will restore consistency with the Makefile.

If you prefer, I can make the Makefile modification and open a PR, or create the small patch for you to apply.

## Debugging tips
- If you see no VGA output in QEMU, verify `grub.cfg` and that the ISO was created correctly. The Makefile will generate a simple grub.cfg if missing.
- Debugging with GDB: run `make debug`, then on the host `gdb kernel.elf` → `target remote :1234` → `continue`.
- If cross‑toolchain isn't installed, the Makefile will fall back to host gcc, but ensure your host supports building 32‑bit binaries (libc/multiarch headers).

## Status & contribution
- Status: core subsystems (boot, memory, screen, basic drivers and shell) are implemented. Next steps could include user mode support, syscalls, filesystem, and device drivers.
- Contributing: fork → branch → commit → PR. If you want, I can prepare a PR that either updates the Makefile or moves files into the expected directories.

## License
No LICENSE file is present in the repository. Consider adding one (MIT/BSD/GPL, etc.). I can add a LICENSE file if you tell me which license you prefer.

---

If you'd like, I can now:
- commit this README.md to the repository (create a branch + PR),
- apply the Makefile fix and open a PR,
- or walk through building and running it locally with QEMU and GDB.
