# MyOS Console 0.12.0-dev User Guide

> **Language:** [English](USER_GUIDE.md) | [Русский](USER_GUIDE_RU.md)


This guide is intended for someone who wants to **build, run and try MyOS** without studying kernel internals. MyOS is an experimental teaching OS for x86_64. Use QEMU primarily; only run on real hardware with a dedicated test USB drive.

> This release does not include a ready graphical user interface. After booting, a console shell with commands and a small set of programs is available.
>
> To install the toolchain on Windows, WSL, macOS and other host platforms, first open the [platforms guide](PLATFORMS.md). Below describes using MyOS once the build environment is prepared.

## 1. What you need

To run in QEMU you need GNU-compatible build tools, NASM, image utilities and QEMU. The exact installation path depends on the host platform; use the [platforms guide](PLATFORMS.md) for Linux, Windows/WSL, native Windows/MSYS2 and macOS.

On Ubuntu/Debian you can typically install them with:

```bash
sudo apt update
sudo apt install build-essential nasm xorriso mtools gdisk qemu-system-x86 ovmf
```

The source tree should be in the project directory. In all examples below we assume:

```bash
cd /home/ubuntu/myos
```

## 2. Build

Run:

```bash
make all img
```

On the first build Make will automatically download the Limine package and build two files in the project root.

| Файл | Когда использовать |
|---|---|
| `myos.iso` | Быстрый запуск как CD/ISO в QEMU. |
| `myos.img` | Рекомендуемый raw disk image для QEMU, USB-флешки и проверки persistent files. |

> `make img` пересоздаёт `myos.img`. Все файлы, которые были сохранены в `disk/` внутри старого образа, при этом удаляются.

## 3. Recommended way to run in QEMU

For the full console version with persistent files use `myos.img`:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

A QEMU window will open. Boot diagnostics are shown in separate blocks `BOOT ENVIRONMENT`, `KERNEL SERVICES`, `STORAGE AND RUNTIME` and `USER ENVIRONMENT`. On a normal boot MyOS will display a three-second countdown and **automatically** start `/init`; the screen is cleared before the user shell, so it opens without the previous boot lines:

```text
[myos]$
```

If you press `K` before the countdown ends (on a PS/2 keyboard or in a serial console), automatic startup is cancelled and a diagnostic kernel shell appears:

```text
kernel>
```

In this mode boot diagnostics remain on the screen for troubleshooting, and the user shell can be started manually with the command:

```text
init
```

On first login the shell prints a compact startup card with hints for `help`, `help calc`, Tab completion and command history.

To run with serial output in the terminal add `-serial stdio`. If you want only the terminal without the QEMU window, also add `-display none`:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c -serial stdio -display none
```

## 4. Quick system check

After automatic startup or the manual `init` command try these commands in order:

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

This will confirm that the user shell, scheduler, memory, clocks, initramfs and the filesystem have started.

## 5. Working with files

MyOS has two simple types of files.

| Путь | Смысл | Что происходит после restart |
|---|---|---|
| `tmp/<имя>` | Временный файл в памяти | Исчезает. |
| `disk/<имя>` | Постоянный файл на data partition `myos.img` | Сохраняется, если не пересоздавать image. |

Temporary file example:

```text
touch tmp/test
write tmp/test Hello
cat tmp/test
rm tmp/test
```

Persistent file example:

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

The text should be preserved. The persistent filesystem is intentionally small: up to 8 files, each up to 512 bytes; a single `write` command transfers up to 128 bytes of text.

## 6. Most useful shell commands

| Команда | Пример | Назначение |
|---|---|---|
| `help` | `help` | Brief overview of shell features. |
| `help calc` | `help calc` | Syntax, examples and limitations of the calculator. |
| `ls` | `ls` | List files in the initramfs, `tmp/` and `disk/`. |
| `cat` | `cat motd.txt` | Show a file. |
| `touch` | `touch disk/note` | Create an empty file. |
| `write` | `write disk/note Hello` | Overwrite a file with a single line. |
| `rm` | `rm disk/note` | Delete a file. |
| `edit` | `run edit disk/note` | Open a simple one-line editor. |
| `ps` | `ps` | Show processes. |
| `sleep` | `sleep 2` | Wait the specified number of seconds. |
| `calc` | `calc 7 * 6` | Quickly perform simple arithmetic; prints only the result or an error. |
| `run` | `run hello` | Run an arbitrary program with diagnostic messages about process start and exit. |
| `spawn` | `spawn sleeper 3` | Run a program in the background. |
| `wait` | `wait 4` | Wait for a process by PID. |
| `kill` | `kill 4` | Terminate a child process. |
| `pipe` | `pipe hello` | Pass text through the built-in pipe workflow. |
| `set` / `get` / `env` | `set NAME Ada` | Work with shell environment variables. |
| `reboot` | `reboot` | Reboot the virtual machine. |
| `poweroff` | `poweroff` | Request proper power-off via ACPI. |
| `clear` | `clear` | Clear the serial terminal and the framebuffer text console. |

### Programs in the initramfs

Most programs are started with `run` or `spawn`; `calc` is also available as a direct shell command.

```text
run hello
calc 12 / 3
run wc motd.txt
run grep MyOS motd.txt
run argshow one two three
run edit disk/note
```

| Программа | Назначение |
|---|---|
| `hello` | Minimal ring-3 demo program. |
| `sleeper` | Sleeps for a specified time; useful for `ps`, `wait` and `kill`. |
| `orphaner` | Demonstrates orphan handling. |
| `safety` | Checks user/kernel safety boundary. |
| `argshow` | Shows received arguments. |
| `calc` | Performs simple arithmetic; available directly as a shell command or via `run calc ...`. |
| `pipewrite`, `piperead` | Utility programs for bounded pipes. |
| `wc` | Counts lines, words and bytes of a file. |
| `grep` | Searches for a string in a file. |
| `edit` | Edits a single line in a `tmp/` or `disk/` file. |

### Calculator `calc`

`calc` accepts two **signed 64-bit integers** and one operator: `+`, `-`, `*` or `/`. Numbers may start with a `+` or `-` sign. The recommended form is a direct shell command, so `run` is not required. In this mode the calculator prints only the result or its own error. If you need PID, lifecycle and exit status for diagnostics, use the explicit form `run calc <expression>`.

| Ввод | Результат |
|---|---|
| `calc 7 * 6` | `42` |
| `calc -5 + 2` | `-3` |
| `calc 5 - 8` | `-3` |
| `calc -7 * -6` | `42` |
| `calc 9 / 2` | `4`: integer division truncates toward zero. |
| `calc 1 / 0` | Division by zero error. |

The result range is from `-9223372036854775808` to `9223372036854775807`. If you forget the syntax, enter `help calc`. The calculator checks for signed 64-bit integer overflow for each operator, including the special case `-9223372036854775808 / -1`, and reports an error instead of producing an incorrect result.

## 7. Input conveniences

The user shell supports the following conveniences.

| Ввод | Результат |
|---|---|
| Up / Down | Navigate the limited command history. |
| Tab | Complete a unique command or unique file path. |
| `$NAME` | Substitute a previously set environment variable. |
| `clear` | Clear the screen without printing many blank lines. |

Example:

```text
set NAME MyOS
write tmp/greeting Hello $NAME
cat tmp/greeting
```

## 8. Booting ISO and UEFI

The ISO is suitable for a simple boot test, but does not contain a separate data partition for persistent files:

```bash
qemu-system-x86_64 -machine q35 -m 256M -cdrom myos.iso -boot d
```

For UEFI with a raw image:

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

For physical PCs use **`myos.img`**, not the ISO. This image contains a GPT, a BIOS boot partition, an EFI partition and a separate data partition for `disk/`.

1. Insert a separate USB flash drive with no important data.
2. Find its device name:

   ```bash
   lsblk
   ```

3. Make sure you've selected the whole disk, e.g. `/dev/sdb`, not a partition like `/dev/sdb1`.
4. Write the image:

   ```bash
   sudo dd if=myos.img of=/dev/sdX bs=4M conv=fsync status=progress
   sync
   ```

> The `dd` command will completely erase the contents of the selected disk. A wrong `/dev/sdX` can destroy data on your system or external disks. Do not run the command unless you are certain of the device name.

## 10. Release limitations

MyOS Console 0.12.0-dev is not a replacement for Linux, Windows or BSD. In its current state it has no networking, USB HID keyboard/mouse, SMP, Secure Boot, a full multi-window GUI environment, a general-purpose filesystem, a package manager or a compatibility layer for Unix programs. Use the OS as an educational and experimental project.

If the build or run fail, first run `make clean`, then `make all img` and repeat the QEMU command from section 3. For technical diagnostics and development consult the [developer guide](DEVELOPER_GUIDE.md).
