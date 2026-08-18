# MyOS Console 0.12.0-dev

**MyOS** is an experimental x86_64 operating system written from scratch in freestanding C11 and x86_64 NASM. It uses Limine only as the current bootloader; the kernel, memory manager, scheduler, syscall boundary, shell, file systems and drivers are MyOS code.

> This branch is the completed **console OS** milestone. Its immutable release point is the annotated Git tag `v0.12.0-console`. GUI work is intentionally separate and is not part of this branch or release.

## Start here

The project has two current manuals with different audiences. New users should begin with the user guide; contributors and people studying the implementation should use the developer guide.

| Document | Audience | Purpose |
|---|---|---|
| [User guide](docs/USER_GUIDE_RU.md) | Anyone who wants to build, boot and use MyOS | Plain-language steps for QEMU, USB testing, shell commands and file storage. |
| [Developer guide](docs/DEVELOPER_GUIDE_RU.md) | Developers and systems-programming learners | Architecture, source tree, build/test workflow, interfaces and technical limits. |
| [Documentation index](docs/README.md) | All readers | Current-document index and status of older development notes. |

## What the console release provides

| Area | Current capability |
|---|---|
| Boot | Limine boot in BIOS and UEFI/OVMF QEMU; hybrid `myos.iso` and raw GPT `myos.img`. |
| Kernel | GDT, IDT, TSS, PIC/APIC virtual wire, PIT, PS/2 keyboard, RTC, ACPI S5 poweroff and PCI discovery. |
| Memory | PMM, four-level paging, higher-half kernel, HHDM, heap, per-process address spaces and user-buffer validation. |
| Processes | Ring 3 ELF processes, round-robin scheduler, wait/kill/sleep, process states and task inspection. |
| Shell | Commands, program arguments, environment variables, history with Up/Down and deterministic Tab completion. |
| Files and storage | Read-only initramfs, bounded `tmp/` files and persistent `disk/` files backed by an isolated AHCI data partition. |
| Utilities | `hello`, `sleeper`, `orphaner`, `safety`, `argshow`, `calc`, `pipewrite`, `piperead`, `wc`, `grep` and `edit`. |

## Quick QEMU launch

The recommended command for the complete console experience, including persistent `disk/` files, is to boot `myos.img` as an IDE drive:

```bash
cd /home/ubuntu/myos
make all img
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

At the kernel prompt enter `init`, then use the user shell. A simple first session is:

```text
help
uname
touch disk/note
write disk/note Hello MyOS
cat disk/note
```

For complete launch, UEFI and physical-USB instructions, read the [user guide](docs/USER_GUIDE_RU.md).

## Release artifacts

| Artifact | Intended use | Persistent `disk/` storage |
|---|---|---:|
| `myos.iso` | QEMU CD/ISO testing and read-only boot demonstrations | No |
| `myos.img` | QEMU disk boot, USB test media and physical-PC testing | Yes |

> Writing an image with `dd` erases the entire selected device. Use only a dedicated test USB device and verify its device name with `lsblk` before writing.

## Repository layout

| Path | Contents |
|---|---|
| `boot/` | Linker scripts and Limine configuration. |
| `include/` | Shared kernel and user-space interfaces. |
| `kernel/` | Architecture code, console, memory, drivers, scheduler, loader, filesystem and syscalls. |
| `user/` | Ring-3 shell and user utilities packaged into initramfs. |
| `docs/` | Current user/developer manuals and older development notes. |
| `tools/` | Host-side build helpers. |

## Git branches and release point

| Reference | Meaning |
|---|---|
| `main` | Active console-maintenance branch, rooted at `v0.12.0-console`; it may contain approved documentation and console-only fixes after the tag. |
| `console-stable` | Strict baseline branch that remains at the final console milestone unless an explicitly tested maintenance fix is selected. |
| `v0.12.0-console` | Immutable annotated tag for the completed console release. |
| `gui/bringup` | Separate experimental GUI branch; not included in the console release. |

## Scope and current limits

MyOS is an educational experimental operating system, not a secure general-purpose desktop OS. It does not currently provide networking, USB HID support, SMP, demand paging, a general-purpose filesystem, application isolation comparable to a production OS, Secure Boot support, or a completed GUI in this console release.

The current project documentation replaces the obsolete top-level status that described versions `0.1.0`–`0.7.0-dev` as if they were current. Earlier subsystem notes are kept as historical development records and are identified in the [documentation index](docs/README.md).

## License

A project license has not yet been selected. Do not redistribute the project as a licensed release until a license file is added.
