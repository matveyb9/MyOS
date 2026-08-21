# MyOS Roadmap

<p align="center">
  <a href="ROADMAP_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>


> **Status as of 19 August 2026.** MyOS is an in-house educational and practical OS for `x86_64`, written in freestanding C11 and x86_64 NASM. The project is not based on Linux or BSD; Limine is used only as the current bootloader and provider of the boot environment. The stable console line is complete, and the new GUI functionality is isolated in a separate branch.

## Status markers

| Marker | Meaning |
|---|---|
| `[x]` | Completed, included in the corresponding milestone and verified to the scope of that stage. |
| `[~]` | Implemented in the development branch, but the stage does not yet have a separate stable release or requires planned closing checks. |
| `[ ]` | Planned; work not yet started. |
| `[R&D]` | Research direction. It does not block imminent milestones and will start only by separate decision. |

## Current project status

| Line | Purpose | Status |
|---|---|---|
| `console-stable` | Immutable baseline line of the completed console OS. | `[x]` `v0.12.1-console` at commit `b6914d4`. |
| `main` | Primary line of the console OS and its supported documentation. | `[x]` boot UX refinement in `0dbcc25`: stage headers, three-second auto-init and framebuffer clear before user shell. |
| `feature/gui` | Isolated development of a framebuffer GUI and user-program platform. | `[~]` Checkpoints `v0.12.2-gui-preview` and historical `v0.13.0-gui-rc.1` are retained; `v0.13.1-gui-preview.1` is the current QEMU-validated preview, with MYPFS004, persistent ELF execution, the external SDK VFS subset with live `cp`, the bounded native argument/input/time toolchain and File Workspace v1 implemented without merging into `main`. |

Development version — **MyOS 0.13.1-gui-preview.1**. Tags `v0.12.0-console` and `v0.12.1-console` are historical immutable boundaries and are not moved.

## 1. Base platform and kernel

| Status | Outcome | Contents |
|---|---|---|
| `[x]` | x86_64 boot | Higher-half kernel with Limine 12.5.2, BIOS and UEFI/OVMF paths, ISO and raw `IMG` artifacts. |
| `[x]` | Architectural foundations | GDT, IDT, TSS, exception/IRQ handling, SYSCALL/SYSRET boundary. |
| `[x]` | Memory management | PMM, four-level paging, kernel heap, user address spaces and guard pages. |
| `[x]` | Preemptive execution | Round-robin scheduler, PIT 100 Hz, Local APIC virtual-wire and up to 16 task slots. |
| `[x]` | Basic drivers | PS/2 keyboard, RTC, PIC, PCI, AHCI and ACPI S5 poweroff. |
| `[x]` | Data storage | Initramfs CPIO, tmpfs overlay, persistent storage and GPT disk image with an isolated data partition. |

## 2. Console OS — completed baseline

The console stage is complete and fixed by the release **`v0.12.1-console`**. Subsequent supporting improvements in `main` do not change the boundary of the stable console release without a separate decision for a new patch release.

| Status | Outcome | User capability |
|---|---|---|
| `[x]` | Framebuffer text console and COM1 mirror | Diagnostics available on the physical screen and via QEMU serial output. |
| `[x]` | Kernel diagnostic shell | Prompt `kernel>`, diagnostic commands and safe handoff into user space. |
| `[x]` | User shell `/init` | Prompt `[myos]$`, history, Up/Down, Tab completion, environment variables, arguments and pipes. |
| `[x]` | User utilities | `hello`, `sleeper`, `orphaner`, `safety`, `argshow`, `calc`, `pipewrite`, `piperead`, `wc`, `grep`, `edit`, `tree`, `find`. `tree` and `find` are read-only bounded logical-VFS explorer/search utilities with fixed recursion/output limits. |
| `[x]` | Direct calculator | Signed 64-bit arithmetic and quiet output without lifecycle messages. |
| `[x]` | Automatic launch | `/init` launches after three seconds; `K` cancels the launch and leaves the user at `kernel>`. |
| `[x]` | Readable boot presentation | Boot log split into four stage headers; normal user-shell handoff clears the framebuffer, diagnostic path preserves the log. |
| `[x]` | Up-to-date operational documentation | Root README, user/developer/platform guides, release policy and documentation for Linux, Windows, macOS. |

## 3. GUI bringup — current stage

The GUI intentionally remains in **`feature/gui`**. It is started only by the explicit `startgui` command from the user shell; this model preserves the console as a usable baseline and allows testing the GUI independently.

| Status | Substage | Implemented or expected outcome |
|---|---|---|
| `[x]` | GUI launcher | `startgui` opens a framebuffer desktop from ring 3 and returns to the text console after exit. |
| `[x]` | Desktop and window manager | Dark desktop, three bounded windows (`SYSTEM`, `NOTES`, `MONITOR`), visibility, z-order and focus. |
| `[x]` | Keyboard interaction | Standard desktop fallback: `Alt+Tab` focus, `Alt+F4` focused-window close, `Esc` back/cancel and `Ctrl+Q` GUI exit; editor retains `Ctrl+S`. |
| `[x]` | Software pointer | Bounded crosshair pointer and topmost window focus under it. |
| `[x]` | VFS viewer | Viewing `/system/core/resources/motd.txt` or a file passed as `startgui [absolute-path]`, with ABI-limited content. |
| `[x]` | Persistent note editor | Loads `/users/myos/files/notes/note`, editing, `Ctrl-S` save and `Esc` cancel. |
| `[x]` | File Workspace v1 | `FILES` begins at `/users/myos/`, browses the complete logical VFS through four-entry pages and opens bounded writable regular files in the GUI editor without changing VFS write policy. |
| `[x]` | Boot UX integration | GUI branch contains stage headers and a clear before normal user-shell entry; BIOS regression and `startgui` regression passed. |
| `[x]` | Cross-firmware closure | UEFI/OVMF normal boot confirmed stage headers and a clean framebuffer before the user shell. |

## 4. Near-term GUI priorities

Work is carried out sequentially, with documentation and artifact validation for each user-visible change. The order below is chosen to first improve the primary text workflow, then extend storage, and only afterwards add new hardware input.

| Priority | Status | Work | Completion criteria |
|---:|---|---|---|
| 1 | `[x]` | Cursor-aware editor with scrolling | Caret, `Left`/`Right`/`Up`/`Down`, `Home`/`End`, `Delete` and a bounded 20-line viewport implemented; BIOS and UEFI smoke tests passed. |
| 2 | `[x]` | Historical pre-MYPFS003 named `disk/` files | This is a completed historical GUI validation stage: `startgui disk/name` selected a specific legacy path, `N` cycled files, and the editor saved the selected file. Current workflow uses absolute paths under `/users/myos/files/notes/`. |
| 3 | `[x]` | Hardware mouse/pointer support | PS/2 IRQ12 packets move the pointer, left click focuses the topmost window, and the compact modifier shortcuts remain a fallback; BIOS and UEFI tests passed. |
| 4 | `[x]` | GUI reliability pass | BIOS create/save/return/relaunch, UEFI readback/append/save/return and cross-firmware AHCI persistence passed with no `startgui` regressions. |
| 5 | `[x]` | GUI release boundary decision | Decided: immutable `v0.12.2-gui-preview` fixes the tested GUI scope; `main` and `console-stable` are not changed, and `feature/gui` continues to the next stage. |
| 6 | `[x]` | Pointer refresh hardening | Ordinary PS/2 movement no longer repaints the full desktop: kernel restores the 11×11 pointer underlay and draws the cursor at the new location. BIOS framebuffer captures, GUI note save, native program execution and UEFI remount checks passed. |
| 7 | `[ ]` | First GUI release-stabilization pass | `make smoke` automates clean raw-image BIOS/UEFI boot markers, `make regression` on a disposable image verifies BIOS GUI note save and native build/install/run, then UEFI persistence/readback and GUI exit, and `make release-check` cleanly rebuilds artifacts and records source/artifact SHA-256. Remaining tasks: perform a physical x86_64 PC smoke test, lock the final release scope and release notes, and then separately decide on the new GUI tag and moving the tested commit into `main`. |

## 5. Immediate post-GUI stage: native programs and development environment

The GUI release decision is made: the immutable preview tag fixes the tested framebuffer scope but does not declare the GUI production-ready and does not change the stable console baseline. After the checkpoint, persistent ELF execution, the MyOS SDK, MYPFS004, a restricted native `asm`/`build` workflow, pointer-refresh hardening, the read-only System Inventory VFS and `sysinfo`, File Workspace v1, `make smoke`, isolated `make regression` and clean-tree `make release-check` are implemented. The next merge-oriented priority is to complete the GUI release-stabilization pass: remaining tasks are the physical x86_64 PC smoke test, final release scope and an explicit decision on a new tag. The GUI does not need to be merged into `main` to continue developing the environment, but a stable merge should not precede this validation.

| Priority | Status | Work | Completion criteria |
|---:|---|---|---|
| 1 | `[x]` | Persistent ELF64 program execution | `install <absolute-source> /apps/<name>/main.elf` copies a bounded ELF into a global application package; `run <name> [arguments]` creates a separate user task. The loader validates x86_64 ELF64 `ET_EXEC`, load segments and entry. |
| 2 | `[x]` | MyOS SDK for external builds | Public header, startup code, linker script, build template and example app are in `sdk/`; host-built ELF installs to `/apps/<name>/main.elf` and runs without rebuilding the kernel. Details and validation — in [SDK.md](SDK.md). |
| 3 | `[x]` | Developer filesystem workflow | MYPFS003 implemented: real directories, lower-case unified root, `/system/core`, `/system/data`, `/system/config`, `/apps`, `/users/myos`, `/temp` and read-only `/system/live`. Supported: absolute paths, ASCII case-preserving/case-insensitive lookup, `/apps` packages, shell `ls`/`mkdir`/`touch`/`write`/`rm`, MYPFS001/MYPFS002 migration and legacy disk namespace removal. [FILESYSTEM_SPEC.md](FILESYSTEM_SPEC.md) records the contract. |
| 4 | `[x]` | MYPFS004 dynamic large-file storage | Regular files lazily grow up to 8 MiB, use up to six extents and 64 KiB allocation batches; AHCI command DMA frames are freed on all exit paths. Passed: fragmented 1 MiB exact readback, fresh-boot streamed read, MYPFS003 `M4MG` migration, MYPFS002 migration and BIOS/UEFI SDK execution. [MYPFS004_STORAGE.md](MYPFS004_STORAGE.md) records the contract. |
| 5 | `[x]` | First native build in MyOS | Implemented `asm` and a public shell wrapper `build`: bounded `.mya` source from `/users/myos/projects/` is turned into a loader-valid x86_64 ELF64, then `install` packages it as `/apps/<name>/main.elf`. BIOS build/run, fresh remount and UEFI execution passed. [NATIVE_BUILD.md](NATIVE_BUILD.md) records syntax and bounds. |
| 6 | `[x]` | Labels and forward-only jumps | `.mya` supports `label name:` and `jump name`; identifiers are bounded, labels are unique, and a target must be strictly after the jump. BIOS package execution emitted only code up to the jump with authored status `23`; backward targets are rejected, and the BIOS-created package was re-executed in UEFI. [NATIVE_BUILD.md](NATIVE_BUILD.md) records syntax, limits and diagnostics. |
| 7 | `[x]` | Bounded conditional control flow | `.mya` now supports `set <0..255>`, `jump_if_zero name` and `jump_if_nonzero name` in addition to labels and unconditional jumps. Conditional and ordinary targets remain strictly forward; missing conditions and backward targets are rejected. BIOS true/false paths, rejected cases and UEFI persistence are covered by [NATIVE_BUILD.md](NATIVE_BUILD.md) and `make regression`. |
| 8 | `[x]` | General in-OS text editor | Direct `edit <absolute-file>` provides multi-line cursor editing for ordinary mutable VFS files and `.mya` source. It has a 4 KiB document limit, explicit save/discard controls, and bounded VFS I/O. BIOS ordinary-text readback, editor-authored program build/run, and UEFI persistence are covered by [TEXT_EDITOR.md](TEXT_EDITOR.md) and `make regression`. |
| 9 | `[x]` | Bounded native arguments, input, RTC time and exact comparison | `.mya` supports `args`, `input`, `time` and `jump_if <0..255> name`. `args` forwards the existing bounded `run <name> [arguments]` string; `input` filters terminal `CR`/`LF` and supplies one condition byte; `time` outputs RTC `HH:MM:SS`; all targets remain strictly forward. Generated ELF uses a fixed 32-byte private RW data segment for the entry pointer and input/time scratch. `make regression` verifies empty and forwarded BIOS arguments, input paths and valid time output, then persistent UEFI argument/input/time execution. [NATIVE_BUILD.md](NATIVE_BUILD.md) records the contract. |
| 10 | `[x]` | SDK VFS subset and practical copy tool | The public SDK adds fixed-size VFS read/create-file/write/remove wrappers. The SDK-built live app `cp` copies an editor-authored 305-byte persistent file through the 256-byte request boundary, refuses an existing destination and persists exact target data through UEFI. [SDK.md](SDK.md) records the ABI and user contract. |
| 11 | `[x]` | Bounded mouse-first desktop launcher | Bare `startgui` presents fixed clickable `SYSTEM`, `NOTES` and `EDIT NOTE` tiles plus top-bar `X` exit; `startgui home` is a compatibility alias. The launcher uses the existing single GUI session and bounded viewer/editor state. [GUI_BRINGUP.md](GUI_BRINGUP.md) records the contract. |
| 12 | `[x]` | Mouse window chrome | Non-launcher content raises `NOTES` so the active viewer/editor stays visible. Clicking an exposed title bar raises that window; each window has a bounded `X`. `SYSTEM`/`MONITOR` hide, viewer `NOTES` returns home, and editor `NOTES` cancels to its viewer without saving. QMP BIOS/UEFI regression verifies SYSTEM/MONITOR close, MONITOR title-bar raise, viewer close-to-home and editor cancel-to-viewer through PPM transitions. |
| 13 | `[x]` | Standard modifier hotkeys | Retired the temporary `M`/`N`/`E`/`H`/`Q`, pointer, numeric and reset letter controls. `Alt+Tab` cycles focus, `Alt+F4` closes the focused window, `Esc` returns or cancels and `Ctrl+Q` exits; `Ctrl+S` remains editor save. QMP BIOS/UEFI regression injects each GUI-level standard shortcut. |
| 14 | `[x]` | Read-only System Inventory VFS | `/system/live/boot/info`, compiled-in driver records, device summaries and process snapshots are generated from bounded runtime state; `sysinfo` reads them through the existing VFS ABI. No persistent storage format, raw-device capability or new syscall is introduced. BIOS/UEFI regression validates the directory tree and stable inventory output. |
| 15 | `[x]` | File Workspace v1 | Compact `FILES` starts at `/users/myos/`, freely traverses the logical VFS through four-entry pages, safely views system/inventory records and invokes the 4 KiB GUI editor only for writable roots. BIOS/UEFI QMP regression covers tile entry, parent navigation and CPIO-backed `/system/core/apps` traversal. |
| 16 | `[x]` | GUI editor 4 KiB expansion | The fixed GUI content ABI now supports 4,096-byte viewer/editor payloads. The active GUI-session syscall uses a dedicated mapped-range copy while ordinary syscall I/O remains capped at 512 bytes; ring 3 uses at most sixteen 256-byte VFS reads or writes. A deterministic 4 KiB initramfs fixture is copied through the existing SDK `cp` path, then BIOS verifies GUI save/readback and UEFI verifies exact persisted content. |

> **User priority:** native programs and the first native build workflow are not deferred until networking, SMP, USB or a custom bootloader. After the GUI release decision they form the nearest line of functional development.

## 6. Subsequent system horizon

| Status | Direction | Rule for decision |
|---|---|---|
| `[ ]` | Additional user applications | Build on top of the SDK and executable workflow, starting with practical developer tools. |
| `[ ]` | Physical hardware support | Test on a real x86_64 machine after preserving the QEMU BIOS/UEFI regression baseline. |
| `[ ]` | Users and access control | Introduce after the basic user-program workflow: uid/gid, owners, file permissions, login/session model. |
| `[ ]` | Networking | Start with a QEMU-supported Ethernet driver and a minimal IPv4 path after agreements on user-program execution and storage contracts. |
| `[R&D]` | Multiboot compatibility | Investigate as an additional boot protocol if a concrete compatibility need arises; do not replace the current Limine path without validating all boot artifacts. |
| `[R&D]` | Custom bootloader | Begin with an isolated educational proof of concept; do not replace Limine until a custom path reaches BIOS/UEFI feature parity and repeatable validation. |
| `[ ]` | SMP, IOAPIC and extended timer model | Plan when tasks that truly require parallel CPU execution appear. |

## 7. Boundaries that do not change

| Decision | Status | Rationale |
|---|---|---|
| Primary architecture is x86_64 only | `[x]` | Do not add a 32-bit port to the current roadmap: it duplicates low-level platform work and will slow the first GUI release. A possible i386 learning lab is allowed later as a separate branch. |
| Limine remains the loader for current artifacts | `[x]` | It provides a validated BIOS/UEFI path; a custom bootloader and Multiboot are future research items. |
| GUI is not merged into the console baseline automatically | `[x]` | `feature/gui` remains a separate experimental branch until a separate release decision. |
| Pedagogical comments and educational documentation — after development | `[x]` | A full pedagogical pass will start only after functional development is complete, so unfinished details do not become a false specification. |

## 8. Condition for transition to the pedagogical edition

After functional completion of the chosen release scope, a separate final stage will be required: explanatory comments in sources, sequential educational chapters, architecture diagrams, reproducible lab exercises and an updated validation guide. This stage is intentionally not performed in parallel with active development.

## Next action

Native build workflow, labels/forward-only jumps, bounded conditional control flow, SDK VFS copy tooling, native input/time, the bounded default `startgui` desktop launcher with dynamic installed-app tiles, the general text editor, GUI pointer-refresh hardening, the read-only System Inventory VFS with `sysinfo`, and the automated release-stabilization baseline are completed in `feature/gui`: MYPFS004 VFS provides a unified root with `/system`, `/apps`, `/users/myos` and `/temp`, and `build` compiles bounded `.mya` source into a native ELF64. `args` forwards the existing bounded `run <name> [arguments]` string without adding variables or general writable memory. `input` captures one non-`CR`/`LF` byte as the condition; `set <0..255>` supplies an explicit alternative; `jump_if_zero`, `jump_if_nonzero` and `jump_if <0..255>` keep every target strictly forward. `time` outputs one RTC line in `HH:MM:SS` format. A fixed 32-byte private RW ELF segment retains the entry argument pointer plus syscall scratch storage, not general writable program data. The SDK header also exposes fixed-size VFS read/create/write/remove wrappers; its live `cp` tool copies files in 256-byte chunks, requires a new destination with an existing parent and never overwrites it. Bare `startgui` is a bounded mouse-first launcher: its four compact system-tile rectangles (`SYSTEM`, `NOTES`, `EDIT NOTE`, `FILES`) and top-bar exit rectangle dispatch existing actions; FILES starts at `/users/myos/`, re-enumerates bounded VFS rows before navigation, reaches `/` without exposing raw boot media and opens only writable files up to 4 KiB in its GUI editor, while up to four verified `/apps/<name>/main.elf` packages appear below as mouse-only launch tiles. A tile revalidates and spawns its exact package, ends GUI and waits for the child so console output remains visible. `Alt+Tab` cycles focus, `Alt+F4` closes the focused window, `Esc` returns or cancels, and `Ctrl+Q` exits; temporary single-letter, pointer, numeric and reset controls are removed. `startgui home` remains an alias rather than a general window API or application installer. Non-launcher content raises the active NOTES surface; exposed window title bars raise their records, and per-window `X` controls hide SYSTEM/MONITOR, return viewer NOTES home, or cancel editor NOTES without saving. Direct `edit <absolute-file>` provides cursor-based multi-line editing for ordinary files and `.mya` source, with explicit save/discard and a 4 KiB in-memory document limit. The disposable-image `make regression` gate covers QMP PS/2 `Alt+Tab` focus, `Alt+F4` focused-MONITOR close, `Esc` viewer return, `Alt+F4` editor cancel-to-viewer and `Ctrl+Q` clean exit in BIOS and UEFI, plus mouse activation of compact `NOTES` and `FILES` tiles, FILES parent navigation, SYSTEM/MONITOR close, MONITOR title-bar raise, viewer close-to-home, editor cancel-to-viewer and a discovered installed-app tile launch with PPM framebuffer transitions; it also covers the retained `startgui home` alias, the System Inventory directory tree and `sysinfo` output in BIOS and UEFI, ordinary-text readback, a 4 KiB GUI editor load/save/readback through sixteen VFS chunks seeded from a deterministic initramfs fixture, a paced 305-byte SDK `cp` copy across the VFS request boundary with overwrite rejection, editor-authored program build/run, empty and forwarded arguments, legacy and exact input branches, valid RTC time output, invalid-control-flow rejection, and UEFI persistence of the copied file and installed native packages.
 `install` explicitly moves an output to `/apps/<name>/main.elf`, after which `run <name>` or the discovered desktop tile creates a separate ring-3 task. `make smoke` confirms raw-image BIOS/UEFI boot markers, and `make release-check` produces clean-rebuild source/artifact evidence. The next merge-oriented GUI milestone still requires a physical x86_64 PC smoke test, final release scope and an explicit decision on a new immutable GUI tag. Future native work must preserve the bounded language and execution contract; personal application installation (`/users/myos/apps`) remains a separate future extension. The preview checkpoints, including QEMU-validated `v0.13.1-gui-preview.1`, are not merged automatically into `main`; `main` and `console-stable` retain a console-only scope. The source `myos.iso` and `myos.img` continue to be built by the `make all img` command.
