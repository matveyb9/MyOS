# Architectural Decision: 32-bit Support for MyOS

<p align="center">
  <a href="architecture-decision-32bit_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>


## Context

MyOS is already an x86_64 kernel: Limine hands control over in long mode, the kernel uses the System V x86_64 ABI, 64-bit GDT/IDT stubs, four-level page tables, CR3 and higher-half virtual addresses. A full i686 version is not a compilation flag: it is a separate architecture port with its own boot/entry path, ABI, protected-mode paging/PAE, interrupt frame and test matrix.

## External signals 2025–2026

| Observation | Implication for MyOS |
|---|---|
| Modern mainstream desktop software targets 64-bit CPUs and UEFI. | The primary target platform for the new PC kernel should remain x86_64. [1] |
| Debian continues to release i386 as a **partial** port and describes it as support for IA-32 processors. | 32-bit has not disappeared entirely: it is justified for legacy hardware, compatibility and education, but does not define MyOS's main path. [2] |
| Modern Linux/x86 keeps documentation for the 32-bit boot protocol. | Compatibility support exists, however its presence in mature Linux does not reduce the cost of a separate port for a small kernel. [3] |

## Decision

**Do not add 32-bit support now.** The MyOS mainline remains x86_64. This aligns with the already implemented MMU, framebuffer, UEFI/BIOS images and future plans for user space and GUI.

| Option | Benefit | Immediate cost | Decision |
|---|---|---|---|
| x86_64-only mainline | Modern PCs, UEFI, more memory, a single ABI and one MMU model. | Will not boot on i686-only hardware. | Selected. |
| Full i686 kernel port now | Old PCs, practical experience with protected mode, clear comparison of architectures. | Practically duplicates the low-level tree: boot, paging, GDT/IDT, syscall ABI, driver boundaries, CI/QEMU. Will slow completion of the first usable release. | Defer. |
| Educational i386 bootstrap later | Provides understanding of real/protected mode and suits educational goals. | Should not hold back the main x86_64 roadmap. | Possible after a stable release as a separate lab/branch. |
| 32-bit user compatibility on x86_64 | Useful only once user processes exist and there is a concrete goal to run 32-bit programs. | Requires compat ABI/syscalls, ELF loader policy, and testing. | Do not consider until ring 3 and ELF. |

## Triggers for Reconsideration

The decision should be revisited only if a concrete reason appears: target i686-only hardware; a requirement to run MyOS on an old PC; an educational module on 32-bit protected mode; or the need to run 32-bit user programs. Until then, it is more useful not to create a second kernel port, and to maintain in the documentation a brief comparison of x86 protected mode and x86_64 long mode.

## References

[1]: https://www.microsoft.com/en-us/windows/windows-11-specifications "Windows 11 Specifications"
[2]: https://www.debian.org/ports/ "Debian — Ports"
[3]: https://docs.kernel.org/arch/x86/index.html "Linux kernel documentation — x86-specific documentation"
