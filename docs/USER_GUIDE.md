# MyOS Console 0.12.0-dev User Guide

<p align="center">
  <a href="USER_GUIDE_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>


This guide is intended for someone who wants to build, run and try MyOS without studying kernel internals. MyOS is an experimental educational OS for x86_64. Use QEMU primarily; booting on physical hardware should be done only with a separate test USB flash drive.

> In this release there is no finished graphical interface. After boot you get a console shell with commands and a small set of programs.
>
> To install the toolchain on Windows, WSL, macOS and other host platforms first open the [platform guide](PLATFORMS.md). Below describes how to use MyOS after the build environment is already ready.

## 1. What you'll need

To run in QEMU you need GNU-compatible build tools, NASM, image utilities and QEMU. The exact installation path depends on the host platform; use the [platform guide](PLATFORMS.md) for Linux, Windows/WSL, native Windows/MSYS2 and macOS.

On Ubuntu/Debian the set is usually installed like this:

```bash
sudo apt update
sudo apt install build-essential nasm xorriso mtools gdisk qemu-system-x86 ovmf
```

The sources should be in the project directory. In all examples below it is assumed:

```bash
cd /home/ubuntu/myos
```

## 2. Building

Run:

```bash
make all img
```

On the first build Make will automatically download the Limine package and build two files in the project root.

| File | When to use |
|---|---|
| `myos.iso` | Quick boot as a CD/ISO in QEMU. |
| `myos.img` | Recommended raw disk image for QEMU, USB flash drive and checking persistent files. |

> `make img` recreates `myos.img`. All files that were saved in `disk/` inside the old image will be deleted.

## 3. Recommended way to run in QEMU

For the full console version with persistent files use `myos.img`:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

A QEMU window will open. After boot a kernel prompt will appear:

```text
myos>
```

Enter:

```text
init
```

After that the user shell will open:

```text
[myos]$
```

On first login the shell prints a compact start card with hints about `help`, `help calc`, Tab completion and command history.

To run with serial output in the terminal add `-serial stdio`. If you only want the terminal without the QEMU window, also add `-display none`:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c -serial stdio -display none
```

## 4. Quick system check

After `init` try these commands in sequence:

```text
help
help calc
calc 7 * 6
uname
ps
meminfo
date
uptime
ls
cat motd.txt
```

This will confirm that the user shell, scheduler, memory, clock, initramfs and filesystem have started.

## 5. Working with files

MyOS has two simple file types.

| Path | Meaning | What happens after a restart |
|---|---|---|
| `tmp/<имя>` | Temporary in-memory file | Disappears. |
| `disk/<имя>` | Persistent file on the data partition `myos.img` | Is saved unless the image is recreated. |

Example of a temporary file:

```text
touch tmp/test
write tmp/test Hello
cat tmp/test
rm tmp/test
```

Example of a persistent file:

```text
touch disk/note
write disk/note My first persistent file
cat disk/note
ls
```

Close QEMU, then boot the **same** `myos.img` again and run:

```text
init
cat disk/note
```

The text should be preserved. The persistent filesystem is intentionally small: up to 8 files, each up to 512 bytes; a single `write` command sends up to 128 bytes of text.

## 6. Most useful shell commands

| Command | Example | Purpose |
|---|---|---|
| `help` | `help` | Brief overview of the shell features. |
| `help calc` | `help calc` | Syntax, examples and limitations of the calculator. |
| `ls` | `ls` | List initramfs files, `tmp/` and `disk/`. |
| `cat` | `cat motd.txt` | Show a file. |
| `touch` | `touch disk/note` | Create an empty file. |
| `write` | `write disk/note Hello` | Overwrite a file with a single line. |
| `rm` | `rm disk/note` | Remove a file. |
| `edit` | `run edit disk/note` | Open a simple one-line editor. |
| `ps` | `ps` | Show processes. |
| `sleep` | `sleep 2` | Wait the specified number of seconds. |
| `calc` | `calc 7 * 6` | Quickly perform simple arithmetic; prints only the result or an error. |
| `run` | `run hello` | Run an arbitrary program with diagnostic messages about process start and exit. |
| `spawn` | `spawn sleeper 3` | Run a program in the background. |
| `wait` | `wait 4` | Wait for a process by PID. |
| `kill` | `kill 4` | Stop a child process. |
| `pipe` | `pipe hello` | Pass text through the built-in pipe workflow. |
| `set` / `get` / `env` | `set NAME Ada` | Work with the shell environment variables. |
| `reboot` | `reboot` | Reboot the virtual machine. |
| `poweroff` | `poweroff` | Request a clean shutdown via ACPI. |
| `clear` | `clear` | Clear the serial terminal and framebuffer text console. |

### Programs from initramfs

Most programs are run via `run` or `spawn`; `calc` is also available as a direct shell command.

```text
run hello
calc 12 / 3
run wc motd.txt
run grep MyOS motd.txt
run argshow one two three
run edit disk/note
```

| Program | Purpose |
|---|---|
| `hello` | Minimal ring-3 demo program. |
| `sleeper` | Sleeps for a specified time; useful for `ps`, `wait` and `kill`. |
| `orphaner` | Demonstrates orphan handling. |
| `safety` | Checks the user/kernel safety boundary. |
| `argshow` | Shows the received arguments. |
| `calc` | Performs simple arithmetic; available directly as a shell command or via `run calc ...`. |
| `pipewrite`, `piperead` | Utility programs for bounded pipes. |
| `wc` | Counts lines, words and bytes of a file. |
| `grep` | Searches for a string in a file. |
| `edit` | Edits a single line in a `tmp/` or `disk/` file. |

### Calculator `calc`

`calc` accepts two **signed 64-bit integers** and one operator: `+`, `-`, `*` or `/`. Numbers may begin with a `+` or `-`. The recommended format is as a direct shell command, so `run` is not required. In this mode the calculator outputs only the result or its own error. If you need PID, lifecycle and exit status for diagnostics, use the explicit form `run calc <expression>`.

| Input | Result |
|---|---|
| `calc 7 * 6` | `42` |
| `calc -5 + 2` | `-3` |
| `calc 5 - 8` | `-3` |
| `calc -7 * -6` | `42` |
| `calc 9 / 2` | `4`: division is integer and truncates the fractional part toward zero. |
| `calc 1 / 0` | Division by zero error. |

The result range is from `-9223372036854775808` to `9223372036854775807`. The calculator checks for signed 64-bit integer overflow for each operator, including the special case `-9223372036854775808 / -1`, and reports an error instead of producing an incorrect result.

## 7. Input conveniences

The user shell supports the following conveniences.

| Input | Result |
|---|---|
| Up / Down | Browse the limited command history. |
| Tab | Complete a unique command or a unique file path. |
| `$NAME` | Substitute a previously set environment variable. |
| `clear` | Clears the screen without printing many blank lines. |

Example:

```text
set NAME MyOS
write tmp/greeting Hello $NAME
cat tmp/greeting
```

## 8. Booting ISO and UEFI

The ISO is suitable for a simple boot test but does not contain a separate data partition for persistent files:

```bash
qemu-system-x86_64 -machine q35 -m 256M -cdrom myos.iso -boot d
```

For UEFI with the raw image:

```bash
cp /usr/share/OVMF/OVMF_VARS_4M.fd /tmp/myos-vars.fd
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/tmp/myos-vars.fd \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

## 9. Writing to a USB flash drive

For a physical PC use **`myos.img`**, not the ISO. This image contains a GPT, a BIOS boot partition, an EFI partition and a separate data partition for `disk/`.

1. Connect a separate flash drive with no important data.
2. Find its device name:

   ```bash
   lsblk
   ```

3. Make sure you selected the whole disk, for example `/dev/sdb`, not a partition like `/dev/sdb1`.
4. Write the image:

   ```bash
   sudo dd if=myos.img of=/dev/sdX bs=4M conv=fsync status=progress
   sync
   ```

> The `dd` command will completely erase the selected disk. The wrong `/dev/sdX` can destroy data on your system or external drives. Do not run the command if you are not certain of the device name.

## 10. Limitations of this release

MyOS Console 0.12.0-dev is not a replacement for Linux, Windows or BSD. In its current state there is no networking, USB HID keyboard/mouse, SMP, Secure Boot, a full multi-window GUI environment, general-purpose filesystem, package manager or compatibility layer for Unix programs. Use the OS as a learning and experimental project.

If the build or boot does not work, first run `make clean`, then `make all img` and repeat the QEMU command from section 3. For technical diagnostics and development consult the [developer guide](DEVELOPER_GUIDE.md).
