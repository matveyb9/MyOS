# MyOS User Guide

<p align="center">
  <a href="USER_GUIDE_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>


This guide is intended for someone who wants to build, run, and try MyOS without studying kernel internals. MyOS is an experimental tutorial-and-practice OS for `x86_64`, written from scratch in freestanding C11 and x86_64 NASM. Use QEMU first; running on real hardware should be done only with a separate test USB stick.

> **Current development branch:** `feature/gui`, version `0.13.1-gui-preview.1`. The stable console boundary is preserved by the immutable tag `v0.12.1-console`; the GUI and MYPFS004 are not yet merged into that boundary automatically.

To install the toolchain on Windows, WSL, macOS, and other host platforms, first open the [platforms guide](PLATFORMS.md). Below is how to use MyOS after preparing the build environment.

## 1. What you'll need

To build and run in QEMU you need GNU-compatible build tools, NASM, image-creation utilities, and QEMU. On Ubuntu/Debian the minimal set is installed like this:

```bash
sudo apt update
sudo apt install build-essential nasm xorriso mtools gdisk qemu-system-x86 ovmf
```

All examples assume your terminal is open at the repository root:

```bash
cd /home/ubuntu/myos
```

## 2. Building artifacts

Run:

```bash
make all img
```

| File | When to use |
|---|---|
| `myos.iso` | Quick boot test as an ISO/CD in QEMU. |
| `myos.img` | Recommended raw disk/USB image with GPT, a BIOS boot partition, an EFI partition, and a persistent MyOS data partition. |

> `make img` intentionally recreates `myos.img`. All data from the persistent MYPFS004 partition of the previous image will be removed. If you need test files, copy the image before rebuilding.

## 3. Recommended QEMU invocation

For the full scenario including persistent files and applications, attach the raw image as an IDE drive:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

After boot MyOS shows diagnostics in blocks `BOOT ENVIRONMENT`, `KERNEL SERVICES`, `STORAGE AND RUNTIME`, and `USER ENVIRONMENT`. Then a three-second countdown begins, after which the user shell is started automatically. The framebuffer is cleared before switching:

```text
[myos]$
```

Press `K` during the countdown if you want the diagnostic kernel shell. In that mode the boot log stays on screen and the user shell is started manually:

```text
kernel>
init
```

For serial output to the terminal add `-serial stdio`. If you don't need the QEMU window, add `-display none`:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c -serial stdio -display none
```

## 4. Quick check

After automatic startup or manual `init`, try:

```text
help
uname
ps
meminfo
date
uptime
ls /
ls /system/live/processes
cat /system/core/resources/motd.txt
```

These commands exercise the user shell, scheduler, memory, clock, initramfs, root hierarchy, and the read-only runtime projection.

## 5. Files and directories

MyOS provides a single logical root `/`. Paths preserve the original casing of names, but ASCII lookup is case-insensitive: `Notes`, `NOTES`, and `notes` refer to the same object in a directory. The detailed tree specification is in [FILESYSTEM_SPEC.md](FILESYSTEM_SPEC.md).

| Path | Purpose | Persisted after reboot |
|---|---|---:|
| `/system/core/` | Read-only initramfs: built-in programs, resources, and SDK example. | Yes, as part of the boot image. |
| `/system/data/`, `/system/config/` | Shared mutable machine-wide data and configuration. | Yes. |
| `/system/live/` | Read-only snapshot of processes and devices for the current boot. | No. |
| `/apps/` | Global persistent application packages. | Yes. |
| `/users/myos/files/` | Personal ordinary files, notes, and imported legacy files. | Yes. |
| `/users/myos/projects/` | Projects, sources, and future build outputs. | Yes. |
| `/users/myos/data/`, `/users/myos/config/` | Personal data and configuration. | Yes. |
| `/temp/` | Temporary RAM files. | No. |

### Common file operations

Create a directory and a text file in your personal profile:

```text
mkdir /users/myos/projects/demo
write /users/myos/projects/demo/readme.txt My first MyOS project
ls /users/myos/projects/demo
cat /users/myos/projects/demo/readme.txt
```

For a temporary file use `/temp/`:

```text
write /temp/session.txt temporary text
cat /temp/session.txt
rm /temp/session.txt
```

After closing QEMU, reboot using the same `myos.img` and read the persistent file at the same absolute path. Do not run `make img` beforehand, because that command creates a new empty data partition.

### MYPFS004 practical limits

| Limit | Value |
|---|---:|
| Persistent object records | Up to 128 files and directories total. |
| Regular file | Up to 8 MiB. |
| Extents regular file | Up to 6 non-contiguous extents. |
| Interactive `write` command | Up to 256 ASCII bytes per command line. |
| Path | Up to 111 visible ASCII bytes plus NUL. |
| Name | Up to 63 visible ASCII bytes. |
| Path depth | Up to 8 components below `/`. |

MYPFS004 allocates storage lazily and grows a file as it is written. Large programs and tools should read and write files in offset-based chunks rather than assuming the entire file will be simultaneously mapped into a continuous kernel buffer.

## 6. Most useful shell commands

| Command | Example | Purpose |
|---|---|---|
| `help` | `help` | Brief shell capability map. |
| `ls` | `ls /users/myos` | Show directory contents. |
| `cat` | `cat /system/core/resources/motd.txt` | Display a file. |
| `run cp` | `run cp /users/myos/files/a.txt /users/myos/files/b.txt` | Copy a regular file in bounded chunks. The target must be a new absolute path and its parent must already exist; it is never overwritten. |
| `touch` | `touch /users/myos/files/note.txt` | Create an empty persistent file. |
| `mkdir` | `mkdir /users/myos/projects/demo` | Create a directory. |
| `write` | `write /users/myos/files/note.txt Hello` | Overwrite a file with a single line. |
| `rm` | `rm /users/myos/files/note.txt` | Remove a file or an empty directory. |
| `edit` | `edit /users/myos/files/note.txt` | Open the bounded multi-line text editor; `Ctrl-S` saves and exits, `Ctrl-Q` or `Esc` discards. |
| `ps` | `ps` | Show processes. |
| `calc` | `calc -5 + 2` | Perform signed 64-bit arithmetic. |
| `run` | `run hello` | Run a foreground user program. |
| `spawn` | `spawn sleeper 3` | Run a program in the background. |
| `wait` / `kill` | `wait 4`, `kill 4` | Wait for or stop a child process. |
| `set` / `get` / `env` | `set NAME MyOS` | Work with environment variables. |
| `startgui` | `startgui` | Start the experimental framebuffer GUI. |
| `reboot` / `poweroff` | `reboot` | Reboot or power off the virtual machine. |
| `clear` | `clear` | Clear the text console. |

Most built-in programs are started via `run` or `spawn`. Examples:

```text
run hello
run wc /system/core/resources/motd.txt
run grep MyOS /system/core/resources/motd.txt
run cp /system/core/resources/motd.txt /users/myos/files/motd-copy.txt
run argshow one two three
calc 12 / 3
```

`calc` accepts two signed 64-bit integers and an operator `+`, `-`, `*`, or `/`. Division is integer; division by zero and overflow are safely rejected.

## 7. User programs and the MyOS SDK

The built-in reference ELF is at `/system/core/examples/sdk/hello.elf`. It is copied into a global application package and then run by its short name:

```text
install /system/core/examples/sdk/hello.elf /apps/sdk-hello/main.elf
run sdk-hello external SDK validation
```

After reboot reinstallation is not required:

```text
run sdk-hello persisted
```

The SDK builds freestanding C11 programs on the host computer. Its public header includes bounded VFS read/create/write/remove wrappers, demonstrated by the image’s live `cp` utility. `cp` requires two absolute paths, never overwrites an existing target and removes only a partial target it created after a failure. The detailed workflow, ABI, and linker contract are in [SDK.md](SDK.md). For the first in-OS workflow use the restricted assembler described in the next section; a richer native C frontend is a later milestone.

## 8. Native build directly in MyOS

The native build workflow uses the restricted assembler and the `build` command. Sources are stored in `/users/myos/projects/`, the generated ELF remains next to the source, and to run the program it is installed into the global package `/apps/<name>/main.elf`. Use the general `edit` command for multi-line source; `write` remains useful for short one-line files.

```text
mkdir /users/myos/projects/native
edit /users/myos/projects/native/args.mya
# Type these source lines, then Ctrl-S:
write "["
args
write "]\n"
time
exit 37

build /users/myos/projects/native/args.mya /users/myos/projects/native/args.elf
install /users/myos/projects/native/args.elf /apps/native-args/main.elf
run native-args hello MyOS
```

The program prints `[hello MyOS]`, then writes the current RTC time as `HH:MM:SS` and returns status `37`. Running `run native-args` with no parameters prints `[]`. The source language supports `args`, `input`, `time`, `set <0..255>`, `label name:`, `write "text"`, `jump name`, `jump_if_zero name`, `jump_if_nonzero name`, `jump_if <0..255> name`, and a final `exit <0..255>`. `args` reads only the bounded argument string already supplied by `run`; it does not add variables or writable program memory. A conditional jump needs an earlier `input` or `set`; every target must be a defined label located later in the source, so loops and backward jumps are rejected. Escapes `\n`, `\r`, `\t`, `\\`, and `\"` are available inside text. The generated program runs in ring 3 and returns its authored exit status; use `help asm` and `help edit` for concise command help, [Text Editor](TEXT_EDITOR.md) for editing controls, and [Native Build](NATIVE_BUILD.md) for all bounds and syntax rules.

> Project ELF files are intentionally not directly runnable. The loader accepts installed user applications only from `/apps/<name>/main.elf`, so `install` remains the explicit package boundary.

## 9. Experimental GUI

The GUI is available only on the `feature/gui` branch and is started from the console rather than automatically:

```text
startgui
# Compatibility alias: startgui home
```

Without an argument `startgui` opens **MYOS DESKTOP**, a bounded mouse-first launcher; `startgui home` remains an alias. Click `SYSTEM` for the system message, `NOTES` for notes, or `EDIT NOTE` for the default personal-note editor. Up to four installed packages with `/apps/<name>/main.elf` also appear below the fixed tiles as `OPEN APP`; clicking one starts that program, closes the GUI and returns its normal output to the console. Click the top-bar `X` to exit. Launcher and window actions are mouse-only. The retained GUI-level keyboard shortcuts are `Alt+Tab` to move focus to the next visible window, `Alt+F4` to close the focused window, `Esc` to return or cancel, and `Ctrl+Q` to exit. For a personal note you can pass an absolute path:

```text
startgui /users/myos/files/notes/note
```

To add a desktop app tile, first use the existing package boundary:

```text
install /system/core/apps/hello.elf /apps/hello/main.elf
startgui
# Click HELLO → OPEN APP
```

`Alt+F4` closes the focused window using the same state-specific behavior as its `X`: it hides SYSTEM or MONITOR, returns the NOTES viewer to MYOS DESKTOP, or cancels an editor draft back to the viewer. In viewer or editor mode, the active NOTES window is brought to the front. Click an exposed window title bar to raise it. The desktop top-bar `X` and `Ctrl+Q` exit the complete GUI session. `Esc` returns the viewer home and cancels an editor draft, while `Ctrl+S` saves. Normal PS/2 mouse movement repaints only an 11×11 pointer region; full desktop refresh is reserved for content, focus, window visibility, and layout changes. The full description of controls, the notes editor, and known limitations is in [GUI_BRINGUP.md](GUI_BRINGUP.md).

## 10. UEFI and ISO

The ISO is suitable for a simple boot test, but is not intended for the persistent data workflow:

```bash
qemu-system-x86_64 -machine q35 -m 256M -cdrom myos.iso -boot d
```

For UEFI with the raw image on Linux use OVMF:

```bash
cp /usr/share/OVMF/OVMF_VARS_4M.fd /tmp/myos-vars.fd
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/tmp/myos-vars.fd \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

## 11. Writing to a USB flash drive

For a physical computer use **`myos.img`**, not the ISO. The image contains GPT, a BIOS boot partition, an EFI partition, and the MYPFS004 data partition.

1. Insert a separate flash drive with no important data on it.
2. Find its device name, for example with `lsblk`.
3. Make sure you select the whole disk, e.g. `/dev/sdb`, not a partition like `/dev/sdb1`.
4. Write the image:

   ```bash
   sudo dd if=myos.img of=/dev/sdX bs=4M conv=fsync status=progress
   sync
   ```

> `dd` will completely erase the contents of the selected device. A wrong `/dev/sdX` can destroy data on your system or other disks. Do not run this command if you are not sure about the device name.

## 12. Limitations of the current branch

MyOS is not a replacement for Linux, Windows, or BSD. On `feature/gui` there is currently no networking, USB HID, SMP, Secure Boot, demand paging, package manager, user accounts/permissions, a full native C compiler, or production security hardening. The restricted native assembler is implemented, but the GUI remains a bounded framebuffer environment rather than a general-purpose desktop.

If a build or run fails, do `make clean`, then `make all img`, `make smoke`, and `make regression`. The `smoke` command headlessly checks BIOS and UEFI boot markers, persistent AHCI mount, and automatic `[myos]$` entry. The `regression` command uses a disposable image copy: it creates and saves the default GUI note through the mouse `EDIT NOTE` tile, injects QMP PS/2 `Alt+Tab` for MONITOR focus, `Alt+F4` to close focused MONITOR, `Esc` for viewer return, `Alt+F4` for editor cancel-to-viewer and `Ctrl+Q` for clean exit, then exercises the centered NOTES launcher tile, SYSTEM/MONITOR window close controls, MONITOR title-bar raise, viewer close-to-home and editor cancel-to-viewer with PPM framebuffer transitions. It also retains a BIOS `startgui home` alias check, uses SDK `cp` to copy an editor-authored 305-byte file across its 256-byte request boundary, verifies exact target data and rejects overwrite, builds and installs native packages in BIOS, verifies legacy forward-only branches, empty and forwarded native arguments, `input` exact-match and fallback paths, valid `HH:MM:SS` RTC output, and rejected invalid control flow, then verifies persisted files, the `cp` target and installed input/time/argument packages through UEFI. Both commands do not replace a physical-PC test. After that repeat the QEMU command from section 3. For host-platform setup use [PLATFORMS.md](PLATFORMS.md), for release gates use [RELEASE_STABILIZATION.md](RELEASE_STABILIZATION.md), and for technical diagnostics use [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md).
