# GUI release stabilization checklist

<p align="center">
  <a href="RELEASE_STABILIZATION_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>


> **Status:** active checklist for branch `feature/gui`. It does not create a release tag and does not allow automatic merge into `main`. Its purpose is to provide reproducible evidence prior to a separate decision on the first stable GUI release.

## Automated checks

| Command | What it checks | Isolation |
|---|---|---|
| `make smoke` | Raw-image BIOS and UEFI boot markers, persistent AHCI mount, and automatic `[myos]$` entry. | Uses `myos.img`; does not write user test files. |
| `make regression` | QMP-injected PS/2 `Alt+Tab` focus, `Alt+F4` close of focused MONITOR, `Esc` viewer return, `Alt+F4` editor cancel-to-viewer and `Ctrl+Q` clean exit in BIOS and UEFI, followed by mouse activation of compact `NOTES` and `FILES` tiles, FILES parent navigation, `SYSTEM` and `MONITOR` window close controls, MONITOR title-bar raise, viewer close-to-home, editor cancel-to-viewer and a discovered installed-app tile that launches the persisted editor-authored package then returns to the shell; PPM framebuffer transitions validate the visible steps. The harness also validates the read-only `/system/live/` System Inventory directory tree and `sysinfo` output in BIOS and UEFI, retains the `startgui home` alias, performs GUI note create/edit/save, File Workspace mouse creation and GUI-editor save of zero-byte `/users/myos/guinew` with UEFI type/size persistence, paced console-editor ordinary text save/readback, direct shell `cp` copying of a 305-byte file across the 256-byte VFS boundary with direct overwrite rejection and a retained `run cp` compatibility rejection, direct `wc` exact line/word/byte output for a persisted 259-byte final-word boundary case with retained `run wc` compatibility, direct `grep` output of a short matching line while a matching line crossing the 127-byte retained-line limit is skipped with retained `run grep` compatibility, editor-authored `.mya` source → `build` → `install` → `run`, bounded `BITWISE` not/and/or branch from byte `240` to `143` with rejected uninitialized `not` and `and 256`, conditional rejection cases, text/copied-file/native persistence and clean GUI enter/exit. | Creates a temporary copy of `myos.img` and deletes it after the check. The user's working image is not modified. |
| `make release-check` | Requires a clean Git tree, rebuilds ISO/IMG from scratch, runs `make smoke` and `make regression`, then prints the exact source commit and SHA-256 of both artifacts. | Local-only: does not create a tag, GitHub Release, Pre-release or remote push. |

`make smoke`, `make regression` and `make release-check` require QEMU and OVMF. `make regression` uses a fixed Q35 configuration and `-drive if=ide,format=raw`, because this is the supported persistent AHCI path. `make release-check` additionally requires a clean Git tree and itself runs a clean `make all img`.

```bash
make release-check
```

Expected outcome:

```text
boot smoke: BIOS passed
boot smoke: UEFI passed
interactive regression: BIOS GUI/native workflow passed
interactive regression: UEFI persistence workflow passed
release candidate: source commit <full-SHA>
release candidate: artifacts
<SHA-256>  myos.iso
<SHA-256>  myos.img
release candidate: automated checks passed
```

## Manual release gates

Automated commands are intentionally limited. Before a new GUI release tag is created, the following must be manually confirmed:

| Gate | Required evidence |
|---|---|
| Framebuffer visual check | Desktop, windows, pointer, focus, title-bar raise, per-window `X` behavior, note editor, and return to shell remain readable in a graphical QEMU session. |
| Fresh persistent workflow | On a fresh `myos.img` create a note and a native package, then reboot the guest separately and verify both results. |
| Migration fixtures | MYPFS003→MYPFS004 and MYPFS002→MYPFS004 fixtures complete recovery/migration and are readable after a second clean mount. |
| Physical x86_64 PC | Boot from a disposable USB, test keyboard input, framebuffer output, and clean poweroff/reboot smoke. This cannot be replaced by QEMU. |
| Release scope | Document known limitations, release notes, and the exact immutable tag target. `make release-check` provides a reproducible source SHA and artifact SHA-256, but does not create a public release. |

## What these tests do not prove

`make smoke` and `make regression` do not establish networking, USB HID, SMP, production security, long-duration stress reliability, physical-hardware compatibility or a full native C toolchain. They are release-stabilization evidence for the currently implemented GUI including its bounded mouse-first launcher with FILES navigation across the logical VFS, verified `/apps/<name>/main.elf` tiles, window chrome, MYPFS004, bounded File Workspace empty-file creation in writable logical roots, read-only System Inventory VFS, direct shell `cp` frontend to the public SDK VFS copy workflow, bounded direct `wc` and `grep` file inspection, bounded native not/and/or byte operations, and the restricted native development workflow only.

## Publication rule

After automated and manual gates are complete, creating a public Pre-release or release still requires a separate explicit confirmation. A new immutable tag must not be retagged or force-pushed; `main` must not be updated until a separate merge decision.
