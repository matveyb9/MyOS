<h1 align="center">MyOS</h1>

<p align="center">
  <a href="README_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

**MyOS** is an experimental operating system written from scratch for **x86_64** in freestanding C11 and x86_64 NASM. Limine currently supplies the boot environment, while the kernel, memory management, scheduler, ring-3 programs, shell, filesystem and drivers belong to this repository.

<h2 align="center">Status</h2>

The stable console baseline is [`v0.12.1-console`](docs/RELEASES.md). Current graphical and user-program work is isolated in [`feature/gui`](https://github.com/matveyb9/MyOS/tree/feature/gui); it is **experimental** and is not merged into `main` automatically. This branch includes BIOS/UEFI boot paths, MYPFS004 persistent storage, framebuffer GUI with a bounded mouse-first `startgui` desktop launcher that discovers up to four installed `/apps/<name>/main.elf` tiles, per-window title-bar raise and close controls, a general bounded text editor, persistent ELF packages, the MyOS SDK with a bounded public VFS subset and live `cp` developer tool, an in-OS assembler with bounded program-argument forwarding, single-byte input, RTC `HH:MM:SS` output, labels, explicit condition values, exact-byte comparison and forward-only unconditional or conditional jumps, a read-only `/system/live/` System Inventory with boot, compiled-in driver, device and process records exposed by `sysinfo`, and File Workspace v1: a compact `FILES` desktop tile that starts at `/users/myos/`, browses the complete logical VFS and opens bounded writable text files in the GUI editor without weakening read-only or boot boundaries.

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

`make regression` uses a disposable copy of `myos.img`: it covers QMP-injected PS/2 `Alt+Tab` focus, `Alt+F4` close of focused MONITOR, `Esc` viewer return, `Alt+F4` editor cancel-to-viewer and `Ctrl+Q` clean exit in BIOS/UEFI, then mouse actions for launcher `NOTES`, `FILES` (including its parent navigation) and discovered installed-app tiles, SYSTEM/MONITOR window close controls, MONITOR title-bar raise, viewer close-to-home and editor cancel-to-viewer with framebuffer screenshot transitions. An app-tile click launches a verified persisted package, ends the GUI session and returns its output to the shell. It also covers the retained `startgui home` alias in BIOS, the GUI/editor workflow, the read-only System Inventory directory tree and `sysinfo` output in both firmware paths, a 305-byte SDK `cp` copy across the VFS request boundary, its no-overwrite rule and UEFI persistence, native forward-only control flow, empty and forwarded native program arguments, input true/fallback branches and valid RTC `HH:MM:SS` output. `make release-check` is local verification only. It does not create a tag, GitHub Release or Pre-release.

---

MyOS is an educational experimental project, not a production desktop operating system. Networking, USB HID, SMP, Secure Boot, dynamic linking, a full native C compiler and physical-PC release validation remain outside the current scope. A project license has not yet been selected.
