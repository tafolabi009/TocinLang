# A bootable kernel in Tocin

`kernel.to` is a real, freestanding **32-bit x86 kernel** written entirely in
Tocin. It boots via the Multiboot 1 protocol (GRUB, or QEMU's `-kernel`), prints
a banner to the VGA text console and the serial port, and demonstrates the
compiler's bare-metal features end to end.

It compiles with **no libc, no GC, and no Tocin runtime** — and links with a
bundled `ld.lld`, so producing the kernel needs **no system C toolchain**.

## What it exercises

| Feature | Where |
|---|---|
| **Cross-compilation** to a bare-metal target | `--target-triple i686-unknown-none --reloc static` |
| **Module-level assembly** (`asmModule`) | the Multiboot header, boot stack, and `_start` |
| **Inline asm with constraints** | `outb` / `inb` port I/O |
| **Volatile MMIO** (`volatileStore16`) | the VGA text buffer at `0xB8000` |
| **Raw memory** (`loadByte`) | byte-string output without a string runtime |
| **`interrupt` functions** | `timer_isr` — x86 interrupt calling convention + `iret` |
| **`naked` functions** | `spurious_stub` — no prologue/epilogue, pure asm |
| **Globals** | `vga_row`/`vga_col`/`ticks` live in `.bss` |

## Build

```sh
TOCIN=/path/to/tocin ./build.sh          # -> kernel.elf
```

or by hand:

```sh
tocin kernel.to --freestanding --target-triple i686-unknown-none \
      --reloc static --code-model small -o kernel.o
ld.lld -m elf_i386 -T linker.ld -o kernel.elf kernel.o
```

## Boot it

```sh
qemu-system-i386 -kernel kernel.elf -serial stdio
```

You should see `TOCIN OS` on the emulated VGA display **and** printed to your
terminal (serial). To boot on real hardware or via GRUB, drop `kernel.elf` into
a GRUB config with `multiboot /boot/kernel.elf`.

## How it boots

1. GRUB/QEMU scans the first 8 KiB of the file for the **Multiboot header**
   (magic `0x1BADB002`, `magic + flags + checksum == 0`) — emitted by the first
   `asmModule` into an allocatable `.multiboot` section that `linker.ld` places
   at the front of the image.
2. It loads the image at **1 MiB** and jumps to **`_start`** (the second
   `asmModule`), which points `%esp` at a 16 KiB `.bss` stack and `call`s
   `kmain`.
3. `kmain` is ordinary Tocin: it clears the VGA console, initializes the 16550
   UART, and prints the banner through the `emit` driver. When it returns,
   `_start` halts the CPU.

## What was verified here vs. what needs your QEMU

Built and checked in this repo (no emulator needed):

- The Multiboot header is present in the first 8 KiB, allocatable, and its
  checksum sums to zero — i.e. GRUB will accept it.
- The ELF is a 32-bit executable whose entry point is `_start`; `_start`
  disassembles to `mov $stack_top,%esp; call kmain; cli; hlt`.
- The drivers lower correctly: `out`/`in` port I/O, `mov %cx,0xB8000(...)` VGA
  stores, `timer_isr` ending in `iret`, `spurious_stub` as a bare `iret`.

Left to you (needs a live machine/emulator): the actual boot, on-screen output,
and interrupt delivery. Run the QEMU line above.

## Extending it

- **Interrupts**: `timer_isr` is a complete handler. To fire it, build an IDT
  (an array of gate descriptors in a global), point a gate at the handler, and
  `lidt` it (via `asm`), then unmask the PIC and `sti`.
- **A heap**: define `__tocin_alloc(n)` (a bump allocator over a `.bss` arena)
  and the `alloc()` builtin works in the kernel.
- **64-bit**: switch the triple to `x86_64-unknown-none --code-model kernel` and
  add a long-mode trampoline to `_start` (set up paging + GDT, enable long mode,
  far-jump to a 64-bit `kmain`).
