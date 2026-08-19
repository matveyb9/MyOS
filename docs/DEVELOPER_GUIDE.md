# MyOS Developer Guide

> **Language:** [English](DEVELOPER_GUIDE.md) | [Русский](DEVELOPER_GUIDE_RU.md)




This document describes the current development line **`gui/bringup`** of MyOS. It is intended for developers, systems programmers and readers who need a map of the source tree and the technical invariants. The stable console boundary remains the immutable tag `v0.12.1-console`; current GUI, SDK and MYPFS004 changes are not merged into it without a separate release decision.

> `main` and `console-stable` keep the console scope. GUI experiments, MYPFS004 and the user-program platform are developed only in `gui/bringup` and must not change the console release automatically.

## 1. Build and test contract

### Host dependencies

| Tool | Purpose |
|---|---|
| `gcc`, `ld`, `nasm`, `make` | Build the freestanding C11 kernel, user ELF and ASM. |
| `xorriso`, `mtools` | Create ISO and FAT EFI partition. |
| `sgdisk` | Create GPT raw image. |
| `qemu-system-x86_64` | BIOS and UEFI regression. |
| OVMF | UEFI firmware for QEMU. |

### Make targets

| Target | Output / purpose |
|---|---|
| `make` or `make all` | Builds the hybrid BIOS/UEFI ISO `myos.iso`. |
| `make img` | Recreates the 128 MiB raw GPT image `myos.img`. |
| `make run` | BIOS ISO test in headless serial mode. |
| `make run-graphic` | BIOS ISO test with a framebuffer window. |
| `make run-uefi` | UEFI ISO test in headless serial mode. |
| `make run-uefi-graphic` | UEFI ISO test with a framebuffer window. |
| `make smoke` | Headless BIOS and UEFI raw-image boot smoke: checks firmware marker, persistent AHCI mount and automatic `[myos]$` entry. |
| `make regression` | Creates a disposable raw-image copy; validates BIOS GUI note edit/save, console-editor text/source persistence, legacy native branches, `input` exact-match/fallback behavior, RTC `HH:MM:SS` output and rejection cases, then UEFI readback, persisted input/time package execution and GUI enter/exit. |
| `make release-check` | Requires a clean Git tree, rebuilds ISO/IMG, runs smoke/regression and prints the source commit plus artifact SHA-256; does not tag or publish. |
| `make debug` | Starts QEMU paused with a GDB server on TCP 1234. |
| `make inspect` | Prints ELF headers and sections. |

The raw image is the authoritative storage test target because AHCI read/write and persistent files require a Q35 IDE-attached raw disk:

```bash
make all img
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c -serial stdio -display none
```

For UEFI, provide mutable OVMF variables and boot the same raw image:

```bash
cp /usr/share/OVMF/OVMF_VARS_4M.fd /tmp/myos-vars.fd
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/tmp/myos-vars.fd \
  -drive if=ide,format=raw,file=myos.img \
  -boot c -serial stdio -display none
```

## 2. Repository map

| Path | Responsibility |
|---|---|
| `boot/` | Higher-half linker script and Limine boot menu/configuration. |
| `include/` | Kernel/user ABI contracts: scheduling, paging, syscalls, VFS, pipes, AHCI and architecture helpers. |
| `kernel/main.c` | Limine requests, bootstrap ordering, diagnostics and handoff to the kernel shell. |
| `kernel/arch/x86_64/` | GDT, IDT, TSS, APIC, syscall entry and low-level NASM primitives. |
| `kernel/console/` | COM1 mirror, framebuffer text console and kernel shell. |
| `kernel/mm/` | PMM, four-level page tables, user address spaces and kernel heap. |
| `kernel/sched/` | Round-robin scheduler, task lifecycle, context switch and input waits. |
| `kernel/sys/` | SYSCALL/SYSRET dispatcher, user-copy validation and ABI enforcement. |
| `kernel/loader/` | newc CPIO reader, ELF64 loading and bounded process spawn. |
| `kernel/fs/` | Read-only initramfs VFS, tmpfs and persistent-file backend. |
| `kernel/ipc/` | Bounded one-way pipe table and endpoint lifetime. |
| `kernel/drivers/` | PIT, PS/2 keyboard, RTC, PIC, PCI and AHCI. |
| `user/` | Ring-3 shell, freestanding user programs and initramfs payload. |
| `tools/mkcpio.py` | Deterministic user-program/initramfs packaging helper. |

## 3. Bootstrap and execution model

The boot path is deliberately simple and linear.

```text
Limine
  -> kernel_entry (NASM)
  -> kmain
  -> serial + framebuffer console
  -> GDT / IDT / TSS / syscall MSRs
  -> PMM + paging + heap
  -> ACPI / PIC / PIT / PS2 / RTC / PCI / AHCI
  -> initramfs + VFS + persistent mount
  -> scheduler and kernel workers
  -> three-second auto-init countdown
  -> `K` cancellation to kernel shell, or `/init` user process
  -> ring-3 user shell
```

Limine supplies the framebuffer, memory map, firmware information, RSDP and initramfs module. MyOS keeps a higher-half kernel around `0xffffffff80000000`, uses the HHDM supplied by Limine during bootstrap and owns a four-level PML4 for its mappings. User programs run in separate address spaces and enter the kernel through `SYSCALL/SYSRET`.

### Privilege and task model

| Item | Current value / policy |
|---|---|
| Target architecture | x86_64 only; no 32-bit compatibility target. |
| Task slots | 16 total scheduler slots. |
| Kernel stack | 64 KiB per task. |
| Scheduling | PIT IRQ0 at 100 Hz; round-robin READY task selection. |
| User mapping range | `0x0000000000001000`–`0x00007FFFFFFFFFFF`. |
| Kernel heap | 1 GiB virtual reservation at `0xFFFF900000000000`. |
| Process states | UNUSED, READY, RUNNING, SLEEPING, ZOMBIE, WAITING and INPUT. |

The scheduler updates TSS RSP0 and activates the task address space on each context switch. `wait`, `sleep`, console input and pipe reads block through scheduler state rather than busy-waiting.

## 4. Syscall boundary

`include/syscall.h` is the shared user/kernel ABI. The dispatcher in `kernel/sys/syscall.c` validates descriptor values, limits and user address ranges before copying. User buffers are copied through page-aware `copy_from_user` and `copy_to_user`; direct user pointers are not trusted.

The release includes write/read, process lifecycle, task info, VFS read/enumeration, RTC/uptime, bounded spawn arguments, tmpfs/persistent-file operations, pipe operations, reboot and poweroff. The most important limits are:

| Limit | Value |
|---|---:|
| Generic write syscall payload | 512 bytes |
| Spawn path | 112 bytes including terminator capacity |
| Spawn argument storage | 128 bytes |
| Unified VFS read/write chunk | 256 bytes |
| Legacy tmpfs/persistent compatibility write chunk | 128 bytes |
| Pipe channels | 4 |
| Pipe capacity per channel | 256 bytes |

The native assembler uses the existing blocking `MYOS_SYS_READ` syscall to receive its one byte and `MYOS_SYS_RTC_TIME` to read `myos_rtc_time`; this milestone does not add a new syscall number. Its generated ELF has an RX image at `0x400000` and a fixed 32-byte RW scratch mapping at `0x401000`. Syscall entry does not preserve general argument registers across dispatch, so the emitter reloads its scratch pointer after every syscall before it reuses that storage.

The source of truth is always the structures and constants in `include/syscall.h`, not this table, when changing the ABI.

## 5. Filesystem and storage design

### VFS layers

The VFS lookup order and implementation are in `kernel/fs/vfs.c`.

| Namespace | Backend | Lifetime |
|---|---|---|
| `/system/core/` | Read-only newc CPIO | Built into the image. |
| `/system/data/`, `/system/config/`, `/apps/`, `/users/myos/` | MYPFS004 persistent hierarchy over guarded AHCI data LBAs | Survives reboot of the same `myos.img`. |
| `/temp/` | In-memory bounded tmpfs | Lost at reboot. |
| `/system/live/` | Kernel-generated virtual projection | Current boot only; read-only. |

MYPFS004 has 128 persistent object records, dynamic multi-extent regular-file allocation, six extents per file and an 8 MiB per-file ceiling. Empty files reserve no payload. Growth is batched at 128 sectors (64 KiB); offset-based VFS calls stream large files through 256-byte user ABI chunks. See [FILESYSTEM_SPEC.md](FILESYSTEM_SPEC.md) and [MYPFS004_STORAGE.md](MYPFS004_STORAGE.md) for the public and on-disk contracts.

### Raw image invariant

`myos.img` is a 128 MiB GPT disk image with three partitions.

| Partition | LBA range | Type / purpose |
|---|---:|---|
| 1 | 34–2047 | BIOS boot partition for Limine. |
| 2 | 2048–67583 | EFI FAT partition containing boot files. |
| 3 | 67584–262110 | Isolated MyOS data partition. |

`ahci_write_data_sector()` accepts only data-partition LBAs. MYPFS004 uses two superblocks, 32 record sectors, 48 bitmap sectors, data blocks, a 512-sector migration staging tail and final journal sector. AHCI commands allocate four DMA frames; every read and write exit path must release them. Any change to image layout, AHCI guard constants or persistent metadata must update all contracts together and repeat BIOS/UEFI tests.

## 6. Input, console and user shell

COM1 output is mirrored to the framebuffer text console. The keyboard driver handles PS/2 Set 1 US QWERTY characters and wakes tasks in `INPUT` state. After bootstrap, `kernel/console/shell.c` waits three seconds for `K` from PS/2 or COM1: no cancellation starts `/init` automatically; `K` retains the diagnostic kernel shell, where `init` still launches the same user shell manually. If `/init` is unavailable or automatic loading fails, the kernel reports the condition and remains in the kernel shell without retry looping.

The user shell provides deterministic history navigation and unique-prefix Tab completion. Its command list and command semantics are the source of truth for end-user documentation. Changes to `command_help()`, `execute_command()` or a user program should be reflected in `docs/USER_GUIDE.md` and `README.md`.

## 7. Validation baseline

Before committing a console change, at minimum perform:

| Test | Expected result |
|---|---|
| `make all img` | Strict `-Werror` build and both artifacts complete. |
| `make smoke` | Reproducible raw-image BIOS and UEFI markers pass: expected firmware, persistent AHCI mount and automatic `[myos]$` entry. |
| `make regression` | Disposable-image BIOS GUI note editing, editor text/source workflow, legacy zero/nonzero branches, native `input` exact-match/fallback paths, valid RTC `HH:MM:SS` output and rejected targets pass; UEFI reads persisted text, runs persisted packages including the input/time program, and returns cleanly from GUI. |
| `make release-check` | Clean source tree, clean rebuild, `make smoke`, `make regression`, source commit and SHA-256 artifacts all pass; no tag or remote publication occurs. |
| BIOS raw image | Limine boot, automatic `/init` after three seconds, then user shell. |
| BIOS cancellation | `K` during countdown keeps kernel shell; manual `init` reaches user shell. |
| UEFI raw image | Equivalent automatic startup and user shell through OVMF. |
| Fallback check | Missing or failed `/init` leaves diagnostic kernel shell without retry loop. |
| Process check | `run hello`, `spawn sleeper 1`, `ps`, `wait` or `kill`. |
| Filesystem check | Create/write/read/remove an absolute-path `/temp/` file and a persistent `/users/myos/...` file; list `/system/live/processes`. |
| Persistence check | Reboot the same `myos.img`, then read a previous persistent absolute-path file or run an installed `/apps/<name>/main.elf`. |
| Large-file check | Stream a fragmented multi-extent file; remount and read every byte through bounded VFS requests. |
| Migration check | Boot deterministic MYPFS003 and MYPFS002 fixtures, then confirm durable `MYPFS004` superblock, cleared journal and second-mount payload readback. |
| IPC check | `pipe sample`; run `wc` or `grep` against a file. |

`make smoke` is a boot baseline. `make regression` extends it with GUI, persistent storage, the direct console-editor workflow and restricted native workflow evidence, but it deliberately uses a disposable image copy and therefore does not replace focused migration fixtures or a manual physical-PC check. `make release-check` is the local reproducibility gate before release discussion; it only produces evidence and never creates a tag or performs network publication. For storage code, test both firmware paths on **the same image**: write in BIOS, then read in UEFI. Never test raw AHCI writes on a host block device unless an isolated disposable test device is explicitly intended. The current release gate order is in [RELEASE_STABILIZATION.md](RELEASE_STABILIZATION.md).

## 8. Git workflow

| Reference | Rule |
|---|---|
| `v0.12.0-console` | Do not move or rewrite. It freezes the completed console OS. |
| `main` | Console release branch. Restrict changes to release fixes, documentation and explicitly approved console maintenance. |
| `console-stable` | Optional maintenance line at the same baseline. Cherry-pick tested fixes deliberately. |
| `gui/bringup` | Separate GUI experimentation. Do not merge it into `main` unless a future GUI release is explicitly approved. |

A normal GitHub publication should push `main`, `console-stable` and the annotated console tag. Build artifacts `myos.iso` and `myos.img` are release attachments, not Git-tracked source files.

## 9. Known technical limits

This is a console milestone, not a production OS. Current non-goals include networking, USB HID, SMP, IOAPIC routing, NVMe, demand paging, dynamic linker, Unix ABI compatibility, package management, full filesystem semantics, Secure Boot and production security hardening. AHCI is deliberately limited to one bounded sector operation and the known isolated data range.

The native build, bounded control-flow, input/time and general text-editor milestones are complete: `asm` emits an x86_64 `ET_EXEC` with a fixed private scratch segment from `.mya` source; shell `build` provides the project workflow; `input`, `time`, `set <0..255>`, `label name:`, `jump name`, `jump_if_zero name`, `jump_if_nonzero name` and `jump_if <0..255> name` compile to bounded forward-only code. Direct `edit <absolute-file>` provides cursor-based multi-line editing for ordinary files and `.mya` source, with a 4 KiB all-in-memory document limit; its contract is in [TEXT_EDITOR.md](TEXT_EDITOR.md). Future native work must preserve the established storage, ABI and control-flow limits; no C frontend or general linker is planned before those limits are stabilized. Do not merge GUI, MYPFS004 or native-toolchain work into `main` or `console-stable` without an explicit release decision.

## 10. Documentation maintenance

Documentation changes are part of feature maintenance. Any change to build/run behavior, public shell behavior, ABI, storage layout, host support, branch policy or safety guidance must update the corresponding documentation in the same commit. The authoritative checklist is [DOCUMENTATION_POLICY.md](DOCUMENTATION_POLICY.md).
