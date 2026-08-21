# MyOS Developer Guide

<p align="center">
  <a href="DEVELOPER_GUIDE_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>




This document describes the current development line **`feature/gui`** of MyOS. It is intended for developers, systems programmers and readers who need a map of the source tree and the technical invariants. The stable console boundary remains the immutable tag `v0.12.1-console`; current GUI, SDK and MYPFS004 changes are not merged into it without a separate release decision.

> `main` and `console-stable` keep the console scope. GUI experiments, MYPFS004 and the user-program platform are developed only in `feature/gui` and must not change the console release automatically.

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
| `make regression` | Creates a disposable raw-image copy; validates QMP-injected PS/2 `Alt+Tab` focus, `Alt+F4` close of focused MONITOR, `Esc` viewer return, `Alt+F4` editor cancel-to-viewer and `Ctrl+Q` clean exit in BIOS and UEFI, then mouse activation of the compact `NOTES` and `FILES` tiles (including the full current-path title and FILES parent navigation), `SYSTEM`/`MONITOR` close controls, MONITOR title-bar raise, viewer close-to-home and editor cancel-to-viewer with PPM framebuffer transitions. It also validates the read-only `/system/live/` System Inventory tree and `sysinfo` in both firmware paths, runs the bounded native `tree` explorer, case-insensitive `find` search, two-line `head` view, two-line `tail` view, ASCII `sort` and `stat` type/size lookup against CPIO-backed logical-VFS paths (including shell help in BIOS), retains the `startgui home` alias, validates a 16 KiB GUI note load/save/readback through sixty-four 256-byte VFS chunks seeded from a deterministic initramfs fixture, a paced editor-authored 305-byte SDK `cp` copy across the VFS boundary, exact readback and no-overwrite behavior, console-editor source persistence, legacy native branches, empty and forwarded `args` output, `input` exact-match/fallback behavior, modular `add`/`sub`/`mul` byte arithmetic, safe unsigned `div`, private-slot `cmp`, RTC `HH:MM:SS` output and rejection cases. |
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
| `kernel/sys/` | SYSCALL/SYSRET dispatcher, user-copy validation, ABI enforcement and immutable bootstrap-state inventory handoff. |
| `kernel/loader/` | newc CPIO reader, ELF64 loading and bounded process spawn. |
| `kernel/fs/` | Read-only initramfs VFS, MYPFS/tmpfs backends and generated `/system/live/` System Inventory records. |
| `kernel/ipc/` | Bounded one-way pipe table and endpoint lifetime. |
| `kernel/drivers/` | PIT, PS/2 keyboard, RTC, PIC, PCI and AHCI. |
| `user/` | Ring-3 shell, freestanding user programs and initramfs payload. |
| `sdk/` | Public freestanding user ABI, build template, examples and the live SDK-built `cp` VFS developer tool. |
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
| Ring-3 user stack | 16 KiB: four mapped 4 KiB pages immediately below `INIT_STACK_TOP`, with one unmapped guard page below. |
| Scheduling | PIT IRQ0 at 100 Hz; round-robin READY task selection. |
| User mapping range | `0x0000000000001000`–`0x00007FFFFFFFFFFF`. |
| Kernel heap | 1 GiB virtual reservation at `0xFFFF900000000000`. |
| Process states | UNUSED, READY, RUNNING, SLEEPING, ZOMBIE, WAITING and INPUT. |

The loader allocates the four user-stack frames independently for `/init` and every spawned user program, reserves the lower unmapped guard first, and frees all mapped frames only on the corresponding pre-task failure path. Once a task is created, its address space owns the frames. The scheduler updates TSS RSP0 and activates the task address space on each context switch. `wait`, `sleep`, console input and pipe reads block through scheduler state rather than busy-waiting.

## 4. Syscall boundary

`include/syscall.h` is the shared user/kernel ABI. The dispatcher in `kernel/sys/syscall.c` validates descriptor values, limits and user address ranges before copying. Ordinary user buffers are copied through page-aware `copy_from_user` and `copy_to_user`; the larger fixed GUI content request has its own page-aware mapped-range copy helper. Direct user pointers are not trusted.

The release includes write/read, process lifecycle, task info, VFS read/enumeration, RTC/uptime, bounded spawn arguments, tmpfs/persistent-file operations, pipe operations, reboot and poweroff. The most important limits are:

| Limit | Value |
|---|---:|
| Generic write syscall payload | 512 bytes |
| GUI content request | 16,528 bytes: four 64-bit fields, a 112-byte NUL-terminated title and 16,384-byte content; dedicated active-session mapped-range copy |
| GUI viewer/editor content | 16 KiB (16,384 bytes); exactly up to sixty-four 256-byte VFS transfers |
| Spawn path | 112 bytes including terminator capacity |
| Spawn argument storage | 128 bytes |
| Unified VFS read/write chunk | 256 bytes |
| Legacy tmpfs/persistent compatibility write chunk | 128 bytes |
| Pipe channels | 4 |
| Pipe capacity per channel | 256 bytes |

The native assembler uses the existing blocking `MYOS_SYS_READ` syscall to receive its one byte and `MYOS_SYS_RTC_TIME` to read `myos_rtc_time`; this milestone does not add a new syscall number. Its generated ELF has an RX image at `0x400000` and a fixed 32-byte RW private-data mapping at `0x401000`. The entry prologue preserves the loader-supplied argument pointer in bytes `0..7`; `args` scans at most the existing 127-byte `MYOS_SPAWN_ARGUMENTS_MAX - 1` payload and writes it only when non-empty. Bytes `8..23` are input/time scratch and bytes `24..31` are eight private `store`/`load` slots. `set`, `input` and `load` establish a zero-extended byte condition in `EBX`; `add` and `sub` emit `add bl, imm8` and `sub bl, imm8`, while `mul` emits `mov eax, ebx; imul eax, eax, imm32; movzx ebx, al` and `div` emits `mov eax, ebx; xor edx, edx; mov ecx, imm32; div ecx; movzx ebx, al`. `mul` retains the low byte modulo 256; source-level `div` requires `1..255`, so its unsigned byte quotient cannot overflow or trap. `cmp <0..7>` emits `cmp bl, byte [absolute slot]; setne bl; movzx ebx, bl`, leaving an initialized zero byte for equality or one byte for inequality without exposing mutable memory beyond the fixed private slots. Syscall entry does not preserve general argument registers across dispatch, so the emitter reloads its scratch pointer after every syscall before it reuses that storage.

The SDK republishes an additive, fixed-size subset of this ABI in `sdk/include/myos.h`: VFS read, create-file, write and remove wrappers with 256-byte request payloads. The SDK-built `cp` app uses only those public wrappers. It creates only an absent target with an existing parent, never overwrites it and removes only the partial target it created if copying fails.

The source of truth is always the structures and constants in `include/syscall.h`, not this table, when changing the ABI.

## 5. Filesystem and storage design

### VFS layers

The VFS lookup order and implementation are in `kernel/fs/vfs.c`.

| Namespace | Backend | Lifetime |
|---|---|---|
| `/system/core/` | Read-only newc CPIO | Built into the image. |
| `/system/data/`, `/system/config/`, `/apps/`, `/users/myos/` | MYPFS004 persistent hierarchy over guarded AHCI data LBAs | Survives reboot of the same `myos.img`. |
| `/temp/` | In-memory bounded tmpfs | Lost at reboot. |
| `/system/live/` | Kernel-generated virtual System Inventory projection | Current boot only; read-only; boot, compiled-in driver, device and process records. |

`kernel/main.c` publishes measured Limine/bootstrap facts to the small `kernel/sys/inventory.c` state holder after probes complete. `kernel/fs/vfs.c` combines that immutable state with existing driver counters to generate bounded `key=value` records under `/system/live/boot`, `/system/live/drivers` and `/system/live/devices`; no new syscall, persistent node, raw-device handle or write path is introduced. The ordinary `sysinfo` user-shell command reads those records through the existing VFS read ABI.

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

COM1 output is mirrored to the framebuffer text console. The keyboard driver handles PS/2 Set 1 US QWERTY characters, decodes `Alt+Tab`, `Alt+F4` and `Ctrl+Q` into explicit bounded tokens, exposes a bounded internal character-injection helper for mouse-generated GUI actions, and wakes tasks in `INPUT` state. `Alt+Tab` and `Alt+F4` deliberately use low control tokens so the existing GUI syscall's `0..127` input validation remains unchanged; `Ctrl+Q` and mouse top-bar `X` are consumed directly by the GUI owner as explicit session-exit actions. The framebuffer maps four compact fixed launcher tiles (`SYSTEM`, `NOTES`, `EDIT NOTE`, `FILES`), up to four discovered package tiles, bounded FILES browser rows with fixed type/name/byte-size metadata, or window-chrome hit rectangles to internal mouse-action tokens. Package actions occupy the bounded `MYOS_INPUT_GUI_ACTION_APP_BASE` range; `MYOS_INPUT_GUI_ACTION_FILES` starts the browser, while its parent/previous/four entry/next row actions use an independent bounded range. The kernel admits only `/apps` directories with a non-empty regular `main.elf`; ring 3 independently re-enumerates and revalidates the selected package before spawning it, ending GUI and waiting for the child. For FILES, ring 3 owns the absolute current directory and page index, re-enumerates every selected entry before joining a printable no-slash child name, and permits navigation only through the existing logical VFS. The complete NUL-terminated current directory is also copied into the fixed 112-byte GUI title field; the compositor uses compact title glyph spacing so the supported 1280×800 File Workspace title bar can render the entire bounded logical path without changing pointer geometry. `Alt+F4` returns the state-specific result of closing the focused window through the same bounded GUI input path. It does not create a second input queue or generic pointer IPC. Every non-launcher content update raises the internal `NOTES` record before redraw so the active viewer/editor remains visible. The compositor snapshots the normalized RTC `HH:MM:SS` and fixed 16-slot scheduler allocated/runnable counts on GUI begin and each content update; it draws these display-only values as the top clock and footer `TASKS`/`RUN` status without new ABI state. PIT IRQ0 refreshes only the 72×32 top-bar clock rectangle once per configured second, restoring and redrawing the 11×11 pointer around that bounded update; it neither redraws windows nor polls ring 3. Task values remain content-update snapshots. After bootstrap, `kernel/console/shell.c` waits three seconds for `K` from PS/2 or COM1: no cancellation starts `/init` automatically; `K` retains the diagnostic kernel shell, where `init` still launches the same user shell manually. If `/init` is unavailable or automatic loading fails, the kernel reports the condition and remains in the kernel shell without retry looping.

The user shell provides deterministic history navigation and unique-prefix Tab completion. Its command list and command semantics are the source of truth for end-user documentation. Changes to `command_help()`, `execute_command()` or a user program should be reflected in `docs/USER_GUIDE.md` and `README.md`.

## 7. Validation baseline

Before committing a console change, at minimum perform:

| Test | Expected result |
|---|---|
| `make all img` | Strict `-Werror` build and both artifacts complete. |
| `make smoke` | Reproducible raw-image BIOS and UEFI markers pass: expected firmware, persistent AHCI mount and automatic `[myos]$` entry. |
| `make regression` | Disposable-image QMP PS/2 `Alt+Tab` focus, `Alt+F4` close of focused MONITOR, `Esc` viewer return, `Alt+F4` editor cancel-to-viewer and `Ctrl+Q` clean exit pass in BIOS and UEFI; mouse activation of compact NOTES and FILES tiles, File Workspace current-path title transitions for parent and `/system` navigation, FILES parent navigation, SYSTEM/MONITOR close controls, MONITOR title-bar raise, viewer close-to-home, editor cancel-to-viewer and a discovered installed-app tile launch also pass with PPM framebuffer transitions; a launcher capture additionally requires non-uniform clock and task-status text regions at the fixed 1280×800 QEMU geometry, while an unchanged launcher held for 1.75 seconds must show a clock-region transition without content input, and the FILES browser capture requires a visible current-path title plus title-region transitions for parent and `/system` navigation and visible fixed-column byte-size metadata in its first entry row. The read-only `/system/live/` System Inventory tree and `sysinfo` output pass in both firmware paths. The bounded native `tree` explorer, case-insensitive `find` search, two-line `head` view, two-line `tail` view, ASCII `sort` and `stat` type/size lookup, including BIOS shell help, pass against CPIO-backed logical-VFS paths. The native `stackprobe` touches a 12 KiB automatic buffer and verifies checksum `1566720` in both firmware paths, proving all four user-stack pages are mapped. The retained `startgui home` alias, a 16 KiB GUI note load/save/readback through sixty-four VFS chunks seeded from a deterministic initramfs fixture, a paced editor-authored 305-byte SDK `cp` copy across the VFS chunk boundary, exact readback and overwrite rejection, the installed `sdk-write` VFS create/write example with exact payload readback and its no-overwrite rule, editor source workflow, legacy zero/nonzero branches, a `store`/`load` private-variable branch plus rejected slot `8`, a modular `(250 + 8 - 2) mod 256` `add`/`sub` branch plus rejected uninitialized `add`, a persisted `MULDIV` branch validating `((200 * 2 mod 256) + 57) / 3 = 67` plus rejected `div 0`, a persisted `EQ`/`NE` private-slot comparison package validating both zero and nonzero results plus rejected uninitialized and slot-`8` `cmp`, empty and forwarded native `args`, native `input` exact-match/fallback paths, valid RTC `HH:MM:SS` output and rejected targets also pass. UEFI repeats the full GUI modifier/mouse surface, persisted text/copied-target and SDK-write-payload readback, package execution including the persisted SDK writer and arithmetic package, and clean GUI return. |
| `make release-check` | Clean source tree, clean rebuild, `make smoke`, `make regression`, source commit and SHA-256 artifacts all pass; no tag or remote publication occurs. |
| BIOS raw image | Limine boot, automatic `/init` after three seconds, then user shell. |
| BIOS cancellation | `K` during countdown keeps kernel shell; manual `init` reaches user shell. |
| UEFI raw image | Equivalent automatic startup and user shell through OVMF. |
| Fallback check | Missing or failed `/init` leaves diagnostic kernel shell without retry loop. |
| Process check | `run hello`, `spawn sleeper 1`, `ps`, `wait` or `kill`. |
| Filesystem check | Create/write/read/remove an absolute-path `/temp/` file and a persistent `/users/myos/...` file; list `/system/live` and read `/system/live/boot/info` or run `sysinfo`. |
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
| `feature/gui` | Separate GUI experimentation. Do not merge it into `main` unless a future GUI release is explicitly approved. |

A normal GitHub publication should push `main`, `console-stable` and the annotated console tag. Build artifacts `myos.iso` and `myos.img` are release attachments, not Git-tracked source files.

## 9. Known technical limits

This is a console milestone, not a production OS. Current non-goals include networking, USB HID, SMP, IOAPIC routing, NVMe, demand paging, dynamic linker, Unix ABI compatibility, package management, full filesystem semantics, Secure Boot and production security hardening. AHCI is deliberately limited to one bounded sector operation and the known isolated data range.

The bounded mouse-first desktop launcher entered by bare `startgui` (with `startgui home` as an alias) has four compact fixed tiles including File Workspace v1; `FILES` starts at `/users/myos/`, shows its full current logical path in the window title, traverses the logical VFS in four-entry pages, exposes read-only virtual/system records safely and invokes the completed 16 KiB GUI editor only for writable roots. Up to four verified `/apps/<name>/main.elf` package tiles, per-window title-bar raise and close controls, standard `Alt+Tab`/`Alt+F4`/`Esc`/`Ctrl+Q` keyboard fallback without legacy single-letter GUI commands, SDK VFS subset and live `cp` developer tool, native build, bounded control-flow, argument forwarding, input/time and general text-editor milestones are complete: `asm` emits an x86_64 `ET_EXEC` with a fixed private data segment from `.mya` source; shell `build` provides the project workflow; `args`, `input`, `time`, `set <0..255>`, `add <0..255>`, `sub <0..255>`, `mul <0..255>`, `div <1..255>`, `store <0..7>`, `load <0..7>`, `cmp <0..7>`, `label name:`, `jump name`, `jump_if_zero name`, `jump_if_nonzero name` and `jump_if <0..255> name` compile to bounded forward-only code with eight private byte variables. `add`/`sub`/`mul` require the initialized byte accumulator and wrap modulo 256; `div` has the same prerequisite, rejects zero and retains an unsigned integer quotient; `cmp` compares that accumulator with a private slot and turns equality into zero or inequality into one. Direct `edit <absolute-file>` provides cursor-based multi-line editing for ordinary files and `.mya` source, with a 4 KiB all-in-memory document limit; its contract is in [TEXT_EDITOR.md](TEXT_EDITOR.md). Future native work must preserve the established storage, ABI and control-flow limits; no C frontend or general linker is planned before those limits are stabilized. Do not merge GUI, MYPFS004 or native-toolchain work into `main` or `console-stable` without an explicit release decision.

## 10. Documentation maintenance

Documentation changes are part of feature maintenance. Any change to build/run behavior, public shell behavior, ABI, storage layout, host support, branch policy or safety guidance must update the corresponding documentation in the same commit. The authoritative checklist is [DOCUMENTATION_POLICY.md](DOCUMENTATION_POLICY.md).
