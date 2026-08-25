# MyOS Roadmap

<p align="center">
  <a href="ROADMAP_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

> **Status as of 25 August 2026.** MyOS is an educational, practical `x86_64` operating system written in freestanding C11 and x86_64 NASM. It is not based on Linux or BSD. Limine remains the current bootloader and provider of the boot environment.

## Status markers

| Marker | Meaning |
|---|---|
| `[x]` | Completed and validated to the stated milestone scope. |
| `[~]` | Active or integrated QEMU-validated work not represented by a separate public release. |
| `[ ]` | Planned; implementation has not started. |
| `[R&D]` | Research direction; starts only after a separate decision. |

## Current project status

| Line | Purpose | Status |
|---|---|---|
| `console-stable` | The sole immutable stable console baseline. | `[x]` Anchored by `v0.12.1-console`; maintenance requires a separate stable decision. |
| `main` | Active experimental QEMU-validated integration line. | `[~]` Includes the GUI, MYPFS004, native execution, SDK workflow and File Workspace. |
| `feature/gui` | Historical pre-integration GUI line. | `[x]` Retained for history, inactive and not a target for new work. |

`main` does not itself imply a release identifier. `v0.12.0-console`, `v0.12.1-console` and `v0.13.1-gui-preview.1` remain immutable historical checkpoints.

## 1. Base platform and stable console baseline

| Status | Outcome | Scope |
|---|---|---|
| `[x]` | x86_64 boot and hardware foundation | Higher-half kernel; Limine BIOS and UEFI/OVMF paths; ISO and raw GPT image; GDT, IDT, TSS, PMM, paging, user address spaces, scheduler, PS/2, RTC, AHCI, PCI and ACPI S5 poweroff. |
| `[x]` | Persistent logical VFS | CPIO `/system/core`, MYPFS004-backed writable roots, tmpfs `/temp`, generated read-only `/system/live`, GPT data partition and legacy storage migration. |
| `[x]` | Stable console user environment | User shell, completion, history, pipes, direct bounded file tools, text editor, diagnostics and the completed `v0.12.1-console` boundary. |
| `[x]` | QEMU BIOS/UEFI baseline | Raw-image boot smoke, persistent AHCI path and reproducible validation commands. |

The stable console boundary remains unchanged unless a separate maintenance decision is made.

## 2. Integrated GUI, VFS and native platform

The framebuffer GUI, VFS workspace and native-program platform are integrated into **`main`**. `startgui` remains explicitly launched from the user shell, preserving the console interaction model while `console-stable` remains unchanged.

| Status | Capability | Implemented outcome |
|---|---|---|
| `[x]` | Desktop and window system | Ring-3 `startgui`, bounded `SYSTEM`, `NOTES` and `MONITOR` windows, pointer, z-order, visible close controls, standard modifier hotkeys, RTC clock and task status. |
| `[x]` | GUI editor and viewer | Writable regular files can be viewed and edited through a bounded 16 KiB GUI document ABI; read-only paths cannot enter the editor. |
| `[x]` | File Workspace | `FILES` browses the logical VFS through four-entry pages with revalidated type and metadata rows. Writable roots support new file, new folder, delete confirmation, copy, rename and file-only move; every browsable root supports bounded read-only search. |
| `[x]` | File Workspace MOVE safety | MOVE retains the basename, uses metadata rather than copy-delete, rejects existing targets, is file-only, and stays within one persistent move anchor or the `/temp` hierarchy. The QEMU workflow verifies rejection, successful relocation and UEFI persistence. |
| `[x]` | Persistent native applications | Validated ELF64 loading, `/apps/<name>/main.elf` installation and `run <name> [arguments]`; discovered app tiles launch verified packages. |
| `[x]` | SDK and in-OS development | Public SDK, bounded VFS subset, `asm`, `build`, `install`, a constrained `.mya` language, and `/users/myos/projects/` source workflow. |
| `[x]` | Runtime inventory | Read-only `/system/live` records and `sysinfo` expose bounded boot, driver, device and process information without a new storage format. |

Detailed contracts are maintained in [GUI_BRINGUP.md](GUI_BRINGUP.md), [FILESYSTEM_SPEC.md](FILESYSTEM_SPEC.md), [SDK.md](SDK.md), [NATIVE_BUILD.md](NATIVE_BUILD.md) and [RELEASE_STABILIZATION.md](RELEASE_STABILIZATION.md).

## 3. Validation, branch and publication policy

Work proceeds through small user-visible milestones. A compact, fully validated change may commit directly to `main`; a short isolated branch remains appropriate for higher-risk VFS/ABI work, experiments or multi-part changes.

| Status | Rule | Evidence or decision gate |
|---|---|---|
| `[x]` | GUI/VFS integration | Integrated into `main` after a full `make release-check`. |
| `[x]` | File Workspace completion | BIOS/UEFI QEMU regression covers create, folder, delete confirmation, copy, rename, file-only move, search and MOVE no-overwrite behavior. |
| `[x]` | Normal development gate | Use the relevant build or QEMU regression evidence before committing a user-visible milestone. |
| `[~]` | Future Pre-release | Consider only after a coherent group of meaningful changes and a new `make release-check`; creating it requires separate explicit confirmation. |
| `[ ]` | Future stable release | Requires a physical x86_64 PC smoke test in addition to the QEMU baseline. It does not block QEMU-only development or a scoped Pre-release. |

No release, tag or history rewrite is created as part of ordinary feature work. The detailed workflow is recorded in [DEVELOPMENT_WORKFLOW.md](DEVELOPMENT_WORKFLOW.md).

## 4. Current development focus

The next functional work should improve the practical end-to-end path:

```text
/users/myos/projects/  →  build  →  install  →  run
```

| Priority | Status | Direction | Completion principle |
|---:|---|---|---|
| 1 | `[~]` | Project and developer workflow | Deliver small, visible, bounded improvements that make source creation, editing, build, installation and execution easier to use together. Do not add a new VFS primitive unless the workflow genuinely requires it. |
| 2 | `[ ]` | Follow-on user-facing tools | Choose the next utility or GUI step only when it directly strengthens the established project workflow. |
| 3 | `[ ]` | Coherent Pre-release review | Reconsider a Pre-release after several related milestones, not after every commit. |
| 4 | `[ ]` | Physical-PC validation | Perform a disposable-media x86_64 smoke test when hardware access becomes available, then separately decide whether stable-release work is appropriate. |

> **Priority rule:** project and developer workflow improvements are not deferred until networking, SMP, USB or a custom bootloader.

## 5. Subsequent system horizon

| Status | Direction | Rule for decision |
|---|---|---|
| `[ ]` | Users and access control | Introduce uid/gid, ownership, permissions and login/session concepts only after the basic user-program workflow is mature. |
| `[ ]` | Networking | Start with a QEMU-supported Ethernet driver and a minimal IPv4 path after execution and storage contracts are settled. |
| `[ ]` | SMP, IOAPIC and extended timer model | Plan only when concrete workloads require parallel CPU execution. |
| `[R&D]` | Multiboot compatibility | Investigate only for a concrete compatibility need; do not replace the current Limine path without validating every artifact. |
| `[R&D]` | Custom bootloader | Start as an isolated educational proof of concept; do not replace Limine before BIOS/UEFI parity and repeatable validation. |

## 6. Boundaries that do not change

| Decision | Status | Rationale |
|---|---|---|
| Primary architecture is x86_64 only | `[x]` | A 32-bit port duplicates low-level platform work and is outside the selected scope. |
| Limine remains the current loader | `[x]` | It provides the validated BIOS/UEFI boot path. |
| `console-stable` and `main` have different roles | `[x]` | `console-stable` is the sole stable console baseline; `main` is the active QEMU-validated integration line. |
| `feature/gui` is historical | `[x]` | It is retained for history and is not developed further. |
| Pedagogical edition follows functional completion | `[x]` | Explanatory comments, chapters, diagrams and labs are a separate stage so unfinished behavior does not become a false specification. |

## 7. Next action

Choose one narrow project/developer-workflow milestone from the current `main` baseline, implement it with a bounded contract and synchronized EN/RU documentation, validate it in QEMU at the appropriate depth, and then decide separately whether to publish the verified commit. No Pre-release is needed merely because this Roadmap was updated.

After the functional release scope is deliberately closed, a separate pedagogical-edition stage can add source comments, sequential educational chapters, architecture diagrams, reproducible lab exercises and an updated validation guide.
