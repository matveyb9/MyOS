# Architecture decision: 32-bit support for MyOS

> **Language:** [English](architecture-decision-32bit.md) | [Русский](architecture-decision-32bit_RU.md)


## Context

MyOS is already an x86_64 kernel: Limine transfers control into long mode, the kernel uses the System V x86_64 ABI, 64-bit GDT/IDT stubs, four-level page tables, CR3 and higher-half virtual addresses. A full i686 build is not a compile-time flag: it is a separate architecture port with its own boot/entry path, ABI, protected-mode paging/PAE, interrupt frame and test matrix.

## External signals 2025–2026

| Observation | Implication for MyOS |
|---|---|
| Modern mainstream desktop software targets 64-bit CPUs and UEFI. | The primary target platform for the new PC kernel should remain x86_64. [1] |
| Debian continues to publish i386 as a **partial** port and describes it as support for IA-32 processors. | 32-bit has not disappeared entirely: it is justified for legacy hardware, compatibility and education, but it does not define MyOS’s main path. [2] |
| Modern Linux/x86 keeps documentation for the 32-bit boot protocol. | Compatibility support exists, however its presence in mature Linux does not reduce the cost of a separate port for a small kernel. [3] |

## Decision

**Do not add 32-bit support now.** The mainline of MyOS remains x86_64. This aligns with the already implemented MMU, framebuffer, UEFI/BIOS images and future plans for user space and a GUI.

| Variant | Benefit | Cost now | Decision |
|---|---|---|---|
| x86_64-only mainline | Modern PCs, UEFI, more memory, a single ABI and a single MMU model. | Will not boot on i686-only hardware. | Selected. |
| Full i686 kernel port now | Older PCs, practical work with protected mode, a clear architectural comparison. | Practically duplicates the low-level tree: boot, paging, GDT/IDT, syscall ABI, driver boundaries, CI/QEMU. Will slow completion of the first usable release. | Defer. |
| Educational i386 bootstrap later | Provides understanding of real/protected mode and is suitable for teaching. | Should not block the main x86_64 roadmap. | Possible after a stable release as a separate lab/branch. |
| 32-bit user compatibility on x86_64 | Useful only when user processes exist and there is a concrete goal to run 32-bit programs. | Requires compat ABI/syscalls, ELF loader policy and testing. | Do not consider until ring 3 and ELF. |

## Triggers for reconsideration

The decision should only be revisited if a concrete reason appears: targeted i686-only hardware; a requirement to run MyOS on an old PC; an educational module on 32-bit protected mode; or a need to run 32-bit user programs. Until then it is more useful not to create a second kernel port, and to maintain a short documentation comparison of x86 protected mode and x86_64 long mode.

## References

[1]: https://www.microsoft.com/en-us/windows/windows-11-specifications "Windows 11 Specifications"
[2]: https://www.debian.org/ports/ "Debian — Ports"
[3]: https://docs.kernel.org/arch/x86/index.html "Linux kernel documentation — x86-specific documentation"
