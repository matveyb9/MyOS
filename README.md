<h1 align="center">MyOS</h1>

<p align="center">
  <a href="README_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

**MyOS** is an experimental operating system written from scratch for **x86_64** in freestanding C11 and x86_64 NASM. Limine currently supplies the boot environment, while the kernel, memory management, scheduler, ring-3 programs, shell, filesystem and drivers belong to this repository.

<h2 align="center">Status</h2>

The stable console baseline is [`v0.12.1-console`](docs/RELEASES.md). Current graphical and user-program work is isolated in [`feature/gui`](https://github.com/matveyb9/MyOS/tree/feature/gui); it is **experimental** and is not merged into `main` automatically. This branch includes BIOS/UEFI boot paths, MYPFS004 persistent storage, framebuffer GUI with a bounded mouse-first `startgui` desktop launcher that discovers up to four installed `/apps/<name>/main.elf` tiles, a compositor-owned `HH:MM:SS` clock widget refreshed once per PIT second through a clock-only partial repaint and live bounded `FOCUS`/`TASKS`/`RUN` status footer refreshed with GUI content, per-window title-bar raise and close controls, a general bounded text editor, persistent ELF packages, the MyOS SDK with a bounded public VFS subset, direct shell `cp` backed by the live developer tool, direct bounded `wc` word counting, direct bounded `grep` text search, direct bounded `tree` hierarchy view, direct bounded `find` name search, direct bounded `head` text preview, direct bounded `sort` text ordering, direct bounded `tail` text preview, direct bounded `stat` metadata lookup and packaged `sdk-write` safe-write example, native read-only bounded `tree`, `find`, `head`, `tail`, `sort`, `stat` and 12 KiB `stackprobe` VFS/platform diagnostics, a four-page 16 KiB guarded ring-3 user stack, an in-OS assembler with bounded program-argument forwarding, single-byte input, RTC `HH:MM:SS` output, eight private `store`/`load` byte variables, bounded bitwise `not`, modular byte `neg` and `inc`, `and`, `or` and `xor`, bounded logical `shl`/`shr` byte shifts and circular `rol`/`ror` byte rotates, modular `add`/`sub`/`mul` byte arithmetic, safe unsigned `div`, bounded unsigned-remainder `mod`, and bounded private-slot `cmp`, labels, explicit condition values, exact-byte comparison and forward-only unconditional or conditional jumps, a read-only `/system/live/` System Inventory with boot, compiled-in driver, device and process records exposed by `sysinfo`, and File Workspace v1: a compact `FILES` desktop tile that starts at `/users/myos/`, browses the complete logical VFS with its full current path in the window title, fixed-column type and byte-size metadata, creates a new empty file or directory from a bounded slash-free filename prompt only in existing writable roots, opens a new file in the GUI editor and refreshes the browser after a new directory, and opens bounded writable text files up to 16 KiB in the GUI editor through up to sixty-four unchanged 256-byte VFS transfers without weakening read-only or boot boundaries.

| Line | Purpose | State |
|---|---|---|
| `console-stable` | Reviewed console baseline. | Stable, tagged `v0.12.1-console`. |
| `main` | Console maintenance and documentation baseline. | Supported console line. |
| `feature/gui` | GUI and native-development work. | Experimental development line. |

<h2 align="center">Quick start</h2>

Clone the repository and choose the branch you want to run. The commands below select the current GUI development line.

```bash
git clone https://github.com/matveyb9/MyOS.git myos
cd myos
git switch feature/gui
make all img
```

Run the persistent raw disk image in QEMU. The `if=ide` attachment is required for the tested AHCI persistence path.

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

MyOS opens the user shell after a three-second countdown. Press `K` during the countdown only when you need the diagnostic `kernel>` shell. In the user shell, start with:

```text
help
sysinfo
ls /
tree /system
find tree /system/core
run head /system/core/resources/motd.txt 2
run stat /system/core/resources/motd.txt
tail /system/core/resources/motd.txt 2
run sort /system/core/resources/motd.txt
run stackprobe
run hello
startgui
# Click FILES to browse from /users/myos
```

> `make img` recreates `myos.img` and erases its previous persistent MyOS data. Use a disposable copy before experiments. For a physical USB test, use `myos.img`, not the ISO, and follow the safety guidance in the User Guide.

<h2 align="center">Documentation</h2>

| Guide | Start here when you need… |
|---|---|
| [Documentation map](docs/README.md) | A short route to every current and historical document. |
| [User Guide](docs/USER_GUIDE.md) | QEMU, shell commands, files, persistence and USB safety. |
| [Platform Guide](docs/PLATFORMS.md) | Linux, Windows/WSL, macOS and host-tool setup. |
| [Developer Guide](docs/DEVELOPER_GUIDE.md) | Architecture, source layout, ABI, storage rules and validation. |
| [Release Guide](docs/RELEASES.md) | Branches, tags, release notes and bilingual commit convention. |
| [Roadmap](docs/ROADMAP.md) | Completed work, current priorities and future directions. |
| [GUI Guide](docs/GUI_BRINGUP.md) | Controls and limits of the experimental framebuffer desktop. |
| [Native Build Guide](docs/NATIVE_BUILD.md) | In-OS `.mya` source, build, install and run workflow. |
| [Documentation Policy](docs/DOCUMENTATION_POLICY.md) | Required same-commit documentation and translation updates. |

<h2 align="center">Verification</h2>

```bash
make smoke          # BIOS and UEFI boot markers
make regression     # disposable-image GUI and native workflow
make release-check  # clean rebuild, checks and SHA-256 evidence
```

`make regression` uses a disposable copy of `myos.img`: it covers QMP-injected PS/2 `Alt+Tab` focus, `Alt+F4` close of focused MONITOR, `Esc` viewer return, `Alt+F4` editor cancel-to-viewer and `Ctrl+Q` clean exit in BIOS/UEFI, then mouse actions for launcher `NOTES`, `FILES` (including its current-path title, parent navigation, byte-size metadata and bounded NEW FILE prompt/editor flow) and discovered installed-app tiles, SYSTEM/MONITOR window close controls, MONITOR title-bar raise, viewer close-to-home and editor cancel-to-viewer with framebuffer screenshot transitions, including visible clock, focus-indicator and task-status regions on the desktop plus a wait proving the clock glyph region changes without GUI content input. An app-tile click launches a verified persisted package, ends the GUI session and returns its output to the shell. It also covers the retained `startgui home` alias in BIOS, the 16 KiB GUI editor load/save/reload workflow through a deterministic initramfs fixture and sixty-four VFS chunks with exact UEFI persisted readback, plus GUI creation of a zero-byte `/users/myos/guinew` file and `/users/myos/guidir` directory with UEFI type/size persistence, the read-only System Inventory directory tree and `sysinfo` output in both firmware paths, direct bounded `tree` with retained `run tree` compatibility, direct case-insensitive `find` with retained `run find` compatibility, direct two-line `head` preview with retained `run head` compatibility, direct two-line `tail` preview with retained `run tail` compatibility, direct `sort` ASCII ordering with retained `run sort` compatibility, direct `stat` type/size VFS output with retained `run stat` compatibility plus the `stackprobe` 12 KiB automatic-buffer checksum `1566720` in BIOS and UEFI, a 305-byte direct shell `cp` copy across the VFS request boundary with a retained `run cp` compatibility rejection, exact direct `wc` line/word/byte output for a persisted 259-byte file whose final word crosses the 256-byte boundary with retained `run wc` compatibility, direct `grep` output of a short matching line while a matching 127-byte-limit crossing line is skipped and `run grep` remains compatible, the packaged `sdk-write` create/write example with exact payload readback and no-overwrite behavior, and their UEFI persistence, native forward-only control flow, `store`/`load` variable persistence with rejected slot `8`, modular `(250 + 8 - 2) mod 256` add/sub arithmetic with rejected uninitialized `add`, multiply/divide `MULDIV` persistence with rejected `div 0`, bounded `BITWISE` not/and/or persistence with rejected uninitialized `not` and out-of-range `and 256`, bounded `XOR` persistence for `170 xor 255 xor 85 = 0` with rejected uninitialized and out-of-range xor, bounded `SHIFT` persistence for `3 shl 5 shr 4 = 6` with rejected uninitialized, zero and out-of-range shifts, bounded `ROTATE` persistence for `129 rol 1 ror 2 = 192` with rejected uninitialized, zero and out-of-range rotates, bounded `MOD` persistence for `200 mod 57 = 29` with rejected uninitialized and zero-divisor mod, bounded `NEG` persistence for modular byte negation (`7` becomes `249`) with rejected uninitialized `neg`, bounded `INC` persistence for byte increment wrapping (`255` becomes `0`) with rejected uninitialized `inc`, private-slot `EQ`/`NE` comparison persistence with rejected uninitialized or slot-`8` `cmp`, empty and forwarded native program arguments, input true/fallback branches and valid RTC `HH:MM:SS` output. `make release-check` is local verification only. It does not create a tag, GitHub Release or Pre-release.

---

MyOS is an educational experimental project, not a production desktop operating system. Networking, USB HID, SMP, Secure Boot, dynamic linking, a full native C compiler and physical-PC release validation remain outside the current scope. A project license has not yet been selected.
