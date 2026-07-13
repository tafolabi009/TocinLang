# Kernel & bare-metal development

Tocin can produce freestanding objects for OS, kernel, and embedded work — no
libc, no GC, no runtime — and cross-compile them for a bare-metal target. This
page covers the toolchain features; for a complete, bootable kernel that uses
all of them, see [`examples/kernel/`](../examples/kernel/).

## Freestanding mode

`--freestanding` emits a relocatable object that references only the symbols you
choose to use. It disables everything that needs a host: `print`/`println`,
strings, collections, file I/O, channels/goroutines, and the GC (so `alloc()`
lowers to a call to a `__tocin_alloc` **you** provide). What remains is the
systems core: arithmetic, control flow, functions, raw memory, volatile MMIO,
and inline assembly. `--run` is rejected in this mode.

```sh
tocin kernel.to --freestanding -o kernel.o
```

## Cross-compilation

Object emission honors an explicit target, so you can build for a bare-metal
triple from any host:

| Flag | Purpose |
|---|---|
| `--target-triple <t>` | Target LLVM triple, e.g. `x86_64-unknown-none`, `i686-unknown-none`, `aarch64-unknown-none`, `riscv64-unknown-none-elf` |
| `--cpu <name>` | Target CPU (`generic`, `x86-64`, `cortex-a53`, …) |
| `--target-features <f>` | Comma-separated features, e.g. `-mmx,-sse,-sse2,+soft-float` to keep the kernel off the FPU/SSE |
| `--code-model <m>` | `tiny` \| `small` \| `kernel` \| `medium` \| `large` (kernel code in the top 2 GiB wants `kernel`) |
| `--reloc <m>` | `static` \| `pic` \| `dynamic-no-pic` (kernels usually want `static`) |
| `--no-red-zone` | Disable the SysV red zone — **required** for any code reachable from an interrupt |

```sh
tocin kernel.to --freestanding \
      --target-triple x86_64-unknown-none \
      --code-model kernel --reloc static --no-red-zone \
      -o kernel.o
```

## Module-level assembly

`asmModule("...")` emits assembly verbatim into the module, outside any function
— for things that must exist at the top level: a Multiboot header, the `_start`
entry point, a boot stack, GDT-flush stubs. Multiple calls accrete in source
order. Newlines are `\n` inside the string literal.

```tocin
// Multiboot 1 header in its own allocatable section.
asmModule(".set MAGIC, 0x1BADB002\n.set FLAGS, 0x3\n.section .multiboot, \"a\"\n.align 4\n.long MAGIC\n.long FLAGS\n.long -(MAGIC + FLAGS)");

// Entry point: set up a stack and jump into Tocin.
asmModule(".section .text\n.global _start\n_start:\n  mov $stack_top, %esp\n  call kmain\n  cli\n1: hlt\n  jmp 1b");
```

## Interrupt handlers

`interrupt def` uses the x86 interrupt calling convention: the compiler emits
the correct ISR prologue/epilogue and returns with `iret`. The CPU-pushed frame
(RIP, CS, RFLAGS, RSP, SS) is passed as an integer address you read with
`loadInt`. A second parameter, if present, is the hardware error code.

```tocin
interrupt def page_fault(frame: int, err: int) {
    let faulting_rip = loadInt(frame, 0);
    // ... handle ...
}

interrupt def timer(frame: int) {
    outb(0x20, 0x20);            // EOI to the PIC
}
```

## Naked functions

`naked def` emits no compiler prologue/epilogue — the body is pure assembly.
Use it for IDT entry stubs, context-switch trampolines, or anything that must
control the whole stack frame. The body is responsible for its own return
(`iret`, `ret`, or a jump).

```tocin
naked def isr_stub() {
    asm("cli");
    asm("push %rax");
    // ... save state, call a Tocin handler, restore, iretq ...
    asm("iretq");
}
```

## Volatile MMIO and port I/O

Memory-mapped device registers must not be optimized away or reordered — use the
volatile accessors, which survive `-O3`:

```tocin
volatileStore32(mmio_base, 0x10, value);   // write a device register
let status = volatileLoad32(mmio_base, 0x00);
fence();                                    // full memory barrier
```

Port I/O (x86) goes through constrained inline asm:

```tocin
def outb(port: int, val: int) { asm("outb %al, %dx", "{ax},{dx}", val, port); }
def inb(port: int) -> int      { return asm("inb %dx, %al", "={ax},{dx}", port); }
```

## Typed device registers: `mmio struct`

Manual `volatileStore32(base, offset, v)` calls are error-prone — the offsets
are magic numbers. An **`mmio struct`** names a memory-mapped register block as
a struct with sized fields (`u8`/`u16`/`u32`/`u64`, laid out in C order), and
lowers **every field access to a volatile load/store** at the physical base.
Get a typed handle from an address with `mmioAt(addr)`:

```tocin
mmio struct Uart {
    thr: u32;      // offset 0x00
    ier: u32;      // offset 0x04
    iir_fcr: u32;  // offset 0x08
    lcr: u32;      // offset 0x0C
    lsr: u32;      // offset 0x14  (0x10 is mcr, elided here for brevity)
}

def putc(base: int, ch: int) {
    let u: Uart = mmioAt(base);        // typed view at a physical address
    while (u.lsr & 0x20) == 0 { }      // volatile read — polls the real register
    u.thr = ch;                        // volatile write
}
```

`u.thr = ch` emits `store volatile i32`, and `u.lsr` emits `load volatile i32`,
each at the field's true offset — no elision, no reordering, no manual offset
arithmetic. `mmio struct` works in `--freestanding` (it needs no runtime), and
the sized fields interoperate with default `int` (i64) values in arithmetic and
comparisons. Because LLVM integers are sign-agnostic, `u8`/`u16`/`u32`/`u64`
are width aliases of `i8`/`i16`/`i32`/`i64`; mixed-width integer operands are
sign-extended to the wider type. A full driver is in
[`examples/kernel/mmio_uart.to`](../examples/kernel/mmio_uart.to).

## Raw memory and globals

`alloc` / `free` / `memcpy` / `memset` / `ptrAdd` and the width-typed
`loadByte`/`storeByte`/`loadInt`/`storeInt` operate on integer addresses.
Global variables give you static storage for kernel tables (an IDT, a GDT, page
tables) — zero-initialized globals land in `.bss`. In freestanding mode `alloc`
calls a `__tocin_alloc(n)` you supply (e.g. a bump allocator over a `.bss`
arena).

## Linking without a system toolchain

Tocin's installed packages bundle `ld.lld`, so you can link a kernel with no
gcc/binutils:

```sh
ld.lld -m elf_i386 -T linker.ld -o kernel.elf kernel.o
```

See [`examples/kernel/`](../examples/kernel/) for a working `linker.ld`,
`build.sh`, and a kernel that boots under QEMU (`qemu-system-i386 -kernel
kernel.elf -serial stdio`).

## What Tocin gives you, and what's still on you

**Provided:** freestanding objects, cross-target codegen (triple/CPU/features/
code-model/reloc/red-zone), `naked`/`interrupt` functions, module-level asm,
constrained inline asm, volatile MMIO with typed `mmio struct` register blocks
(sized `u8`/`u16`/`u32`/`u64` fields), raw memory, a bundled linker, and the C
ABI (`extern def`) so Tocin objects link into C/asm and vice-versa.

**Still your job** (as in C): the boot trampoline, paging/long-mode setup for
64-bit, the IDT/GDT tables and `lidt`/`lgdt`, the linker script, and an
allocator. Tocin has the primitives to express all of these; they are not
generated for you.
