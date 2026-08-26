# MyOS User Guide

<p align="center">
  <a href="USER_GUIDE_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>


This guide is intended for someone who wants to build, run, and try MyOS without studying kernel internals. MyOS is an experimental tutorial-and-practice OS for `x86_64`, written from scratch in freestanding C11 and x86_64 NASM. Use QEMU first; running on real hardware should be done only with a separate test USB stick.

> **Current development branch:** `main`, the experimental QEMU-validated integration line. `console-stable` remains the stable console baseline at immutable tag `v0.12.1-console`. The GUI and MYPFS004 are integrated in `main`, but public artifacts remain Pre-releases until a physical-PC validation policy is adopted.

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
sysinfo
ps
meminfo
date
uptime
ls /
ls /system/live
ls /system/live/processes
tree /system
find tree /system/core
head /system/core/resources/motd.txt 2
stat /system/core/resources/motd.txt
tail /system/core/resources/motd.txt 2
sort /system/core/resources/motd.txt
run stackprobe
cat /system/core/resources/motd.txt
```

These commands exercise the user shell, scheduler, memory, clock, initramfs, root hierarchy, and the read-only System Inventory runtime projection.

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

### System Inventory

`sysinfo` prints bounded read-only records from `/system/live/boot/info`, `/system/live/drivers/` and `/system/live/devices/`. The boot record identifies the active Limine/firmware/initramfs environment; driver records report the current static compiled-in driver model and real bounded status or counters; device records summarize the active storage, display, input and clock paths. These are generated diagnostic records, not persistent files and not a raw-device interface. `ls /system/live` also shows the independent process snapshot tree.

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

### Native tree view

`tree [absolute-directory]` prints a type-aware recursive view of the logical VFS from `/`. Pass one absolute directory to start elsewhere, for example `tree /system` or `tree /users/myos`. `run tree` remains a compatibility form. Directory rows use `[D]`, regular files use `[F]` with their size, and virtual records use `[V]`. The built-in utility never mutates storage, accepts no relative path, follows only the existing logical VFS enumeration and stops at **eight directory levels**, **64 entries per directory**, or **256 printed entries**. These limits prevent an exploratory command from consuming unbounded user memory or output.

### Native find search

`find <name-fragment> [absolute-directory]` searches entry names case-insensitively across the logical VFS and prints matching absolute paths with `[D]`, `[F]` or `[V]` type markers. `run find` remains a compatibility form. The optional start directory must be absolute; a relative path, an empty fragment or extra arguments are rejected. `find TrEe /system/core` therefore finds `/system/core/apps/tree.elf`. Like `tree`, `find` is read-only and bounded to **eight directory levels**, **64 entries per directory** and **256 scanned entries**, so it cannot turn a recursive search into unbounded memory or output consumption.

### Native head view

`head <absolute-file> [1..64 lines]` prints the beginning of one readable VFS file. Without the optional count it prints the first **10 lines**; for example, `head /system/core/resources/motd.txt 2` displays the first two MOTD lines. `run head` remains a compatibility form. The utility accepts only one absolute file path and an optional decimal count from 1 through 64. It reads through the normal 256-byte VFS ABI chunks and stops after **4 KiB** of output even if the requested line boundary has not yet appeared, so a malformed or unusually long line cannot create unbounded output. It never modifies storage.

### Native stat lookup

`stat <absolute-path>` reports the logical VFS **type** and **size** of one existing file, directory or virtual record. For example, `stat /system/core/resources/motd.txt` reports a `regular` entry and its byte size; `stat /system/live/boot/info` reports a `virtual` entry. `run stat` remains a compatibility form. The tool resolves the final path component by scanning at most **128 entries** in its parent through the existing VFS list ABI, matches ASCII names case-insensitively like the filesystem, and performs no writes. It reports `stat: path not found` for an invalid or missing entry.

### Native tail view

`tail <absolute-file> [1..64 lines]` prints the end of one readable VFS file. Without the optional count it prints the final **10 lines**; for example, `tail /system/core/resources/motd.txt 2` displays the final two MOTD lines. `run tail` remains a compatibility form. The utility accepts one absolute file path and an optional decimal count from 1 through 64. It streams the file through normal 256-byte VFS ABI chunks but retains only its final **4 KiB**, then selects the requested trailing lines from that bounded buffer. When older content is discarded, it reports `tail: retained last 4096 bytes`; consequently, a line that is larger than the retained window may be partial. It never modifies storage.

### Native sort

`sort <absolute-file>` reads one text file and prints its retained lines in **bytewise ASCII ascending order**. For example, `sort /system/core/resources/motd.txt` prints its `The…`, `Use…`, then `Welcome…` lines. `run sort` remains a compatibility form. The operation is read-only and uses normal 256-byte VFS read chunks. To remain bounded, it retains at most **64 lines**, each up to **127 bytes**; CR bytes are ignored, and a line or entry beyond those limits is omitted with `sort: line or entry limit reached`. Duplicate lines remain in their original relative order.

### Native stack probe

`run stackprobe` is a read-only diagnostic utility for the user-program platform. It fills a 12 KiB automatic buffer and prints `stackprobe: 12288 bytes checksum 1566720`. That exact result confirms the current program can use all four mapped 4 KiB ring-3 stack pages; a guard page remains immediately below them to catch downward stack overflow.

### File Workspace v1

Run `startgui` and click **FILES**. The browser begins at `/users/myos/` and shows the complete current logical path in the window title. Its `[..]`, `[PREV]`, entry rows and `[NEXT]` controls allow mouse-first traversal of every path exposed through the logical VFS, including `/`, `/system/core/`, `/system/live/`, `/apps/`, `/users/` and `/temp/`. Directory rows enter a directory; regular and virtual files are opened safely. Each listed row shows a type marker (`D` directory, `F` regular file, `L` symbolic link, or `V` virtual record), a fixed 12-character visible-name column, and the entry's current VFS byte size with a `B` suffix. These are read-only logical-VFS metadata, not raw disk details. The raw Limine/EFI boot files and `kernel.elf` remain boot artifacts outside this runtime tree.

A regular file is editable in GUI only when its current VFS path is writable: `/users/myos/`, `/temp/`, `/system/data/` or `/system/config/`. `Ctrl-S` saves; `Esc`, `Alt+F4` or the NOTES-window `X` discard the unsaved draft. `/system/core/`, `/system/live/` and `/apps/` remain readable but never enter GUI editor mode. The GUI document capacity is **16 KiB (16,384 bytes)**. GUI loading and saving use at most sixty-four bounded 256-byte VFS transfers. The separate console `edit <path>` command remains capped at 4 KiB; use bounded program or SDK I/O for larger MYPFS004 files.

`[SEARCH]` is available in every browsable root, including read-only roots. It accepts a non-empty printable slash-free ASCII name fragment of at most 63 bytes and scans at most 128 entries in the current directory with case-insensitive matching. It renders the first four matching logical-VFS entries as mouse-openable rows, revalidates the chosen entry before opening it, and does not mutate storage; `[RETURN]` or `Esc` returns to the browser.

In each established writable root, `[NEW FILE]`, `[NEW FOLDER]`, `[DELETE]`, `[COPY]`, `[RENAME]` and `[MOVE]` accept bounded input. The file action creates one new empty regular file and opens its GUI editor; the folder action creates one new empty directory. `[DELETE]` first displays the complete named target and removes that file or empty directory only after a second **Enter** confirmation; `Esc` cancels before mutation. `[COPY]` prompts for a source regular-file name and an absent target name in the **same current directory**, then streams at most **64 KiB** through existing 256-byte VFS requests; it never overwrites a target and removes its own partial target on failure. `[RENAME]` prompts for an existing mutable file or directory name and then a new name in the **same current directory**. It changes only VFS metadata: content, extents and any directory children remain attached to the object.

`[MOVE]` is deliberately narrower than a general file manager move. It first accepts one existing **regular-file** source name of at most 63 printable slash-free ASCII bytes, then an absolute existing writable destination directory. The destination retains the source basename, the complete target path must fit the fixed VFS path limit, and an existing target is rejected without overwrite. The action only updates VFS metadata; it does not copy file content, move directories or emulate a move by copy and delete. A persistent file may move only inside one persistent move anchor: the `/users/...` hierarchy is one anchor, while `/system/data/...` and `/system/config/...` are separate anchors. A temporary file may move only within the `/temp/...` hierarchy. Read-only paths, malformed paths, missing sources or destination directories, and all cross-anchor or persistent-to-temporary moves are rejected without mutation. Each successful action refreshes the browser in place. Package installation and raw-device operations remain shell-only; use `rm`, `cp` and `install` for those workflows.

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
| `sysinfo` | `sysinfo` | Print the read-only boot, driver and device inventory. |
| `ls` | `ls /users/myos` | Show directory contents. |
| `cat` | `cat /system/core/resources/motd.txt` | Display a file. |
| `cp` | `cp /users/myos/files/a.txt /users/myos/files/b.txt` | Copy a file through the bounded native copy tool. The target must be a new absolute path and its parent must already exist; it is never overwritten. `run cp` remains a compatibility form. |
| `wc` | `wc /users/myos/files/a.txt` | Stream one absolute readable file in 256-byte VFS chunks and print newline-terminated lines, space/tab/CR/LF-delimited words and bytes. `run wc` remains a compatibility form. |
| `grep` | `grep MyOS /system/core/resources/motd.txt` | Print newline-terminated lines of at most 127 bytes that contain one unspaced text fragment while reading one absolute file in 256-byte VFS chunks. Longer lines are skipped. `run grep` remains a compatibility form. |
| `tree` | `tree /system` | Recursively show VFS entries without mutation; requires zero or one absolute directory and is limited to 8 levels, 64 entries per directory and 256 printed entries. `run tree` remains compatible. |
| `find` | `find tree /system/core` | Case-insensitively search entry names without mutation; accepts one fragment and an optional absolute directory, with limits of 8 levels, 64 entries per directory and 256 scanned entries. `run find` remains compatible. |
| `run stackprobe` | `run stackprobe` | Run the 12 KiB automatic-buffer diagnostic; expected checksum is `1566720`, confirming all four mapped ring-3 stack pages. |
| `head` | `head /system/core/resources/motd.txt 2` | Print the first 10 lines by default, or 1–64 requested lines, from one absolute readable file; VFS I/O uses 256-byte chunks and output is capped at 4 KiB. `run head` remains compatible. |
| `stat` | `stat /system/core/resources/motd.txt` | Report the type and byte size of one absolute logical-VFS entry through a bounded scan of at most 128 entries in its parent; never writes storage. `run stat` remains compatible. |
| `tail` | `tail /system/core/resources/motd.txt 2` | Print the last 10 lines by default, or 1–64 requested trailing lines, from one absolute readable file; streams 256-byte VFS chunks while retaining only the final 4 KiB. `run tail` remains compatible. |
| `sort` | `sort /system/core/resources/motd.txt` | Sort up to 64 retained text lines in bytewise ASCII ascending order; each line is capped at 127 bytes and storage is never modified. `run sort` remains compatible. |
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
| `startgui` | `startgui` | Start the experimental framebuffer GUI. Its footer shows the bounded active surface as `FOCUS HOME`, `FOCUS SYSTEM`, `FOCUS NOTES` or `FOCUS MONITOR`. Click `FILES` to browse the logical VFS from `/users/myos/`; its `[NEW FILE]`, `[NEW FOLDER]`, `[DELETE]`, `[COPY]`, `[RENAME]` and `[MOVE]` rows are available only in `/users/myos`, `/temp`, `/system/data` or `/system/config`. The first creates a new empty file and opens it in the GUI editor; the second creates a new directory and refreshes the browser; `[DELETE]` shows the named target and requires a second `Enter` before it removes a file or empty directory; `[COPY]` streams an existing regular source no larger than 64 KiB to an absent same-directory target; `[RENAME]` changes the name of an existing mutable file or directory within that same directory without copying its content; `[MOVE]` moves one regular file by metadata update to an existing writable absolute destination directory, preserving its basename, rejecting overwrite and remaining inside one persistent move anchor or the `/temp` hierarchy. Existing writable text files up to 16 KiB also open there. |
| `reboot` / `poweroff` | `reboot` | Reboot or power off the virtual machine. |
| `clear` | `clear` | Clear the text console. |

Most built-in programs are started via `run` or `spawn`. Examples:

```text
run hello
wc /system/core/resources/motd.txt
grep MyOS /system/core/resources/motd.txt
cp /system/core/resources/motd.txt /users/myos/files/motd-copy.txt
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

The SDK builds freestanding C11 programs on the host computer. Its public header includes bounded VFS read/create/write/remove wrappers, demonstrated by the image’s live `cp` utility and packaged `sdk-write` reference example. Direct shell `cp` invokes that same native utility; `run cp` remains compatible. `cp` requires two absolute paths, while `sdk-write` accepts one new absolute target and writes a fixed payload; neither overwrites an existing target, and both remove only a partial target they created after a failure. To try the writer, run `install /system/core/examples/sdk/write.elf /apps/sdk-write/main.elf`, then `run sdk-write /users/myos/files/sdk-write-example.txt` and `cat` that path. The detailed workflow, ABI, and linker contract are in [SDK.md](SDK.md). For the first in-OS workflow use the restricted assembler described in the next section; a richer native C frontend is a later milestone.

## 8. Native build directly in MyOS

The native build workflow uses the restricted assembler and the `build` command. `newproj <name> [hello|args|empty] [edit|build|run|install]` safely creates `/users/myos/projects/<name>/main.mya` from the default runnable `hello` template, fixed `args` starter or editable zero-length `empty` source; names are 1–31 ASCII letters, digits, `-` or `_`. Only exact final `edit`, `build`, `run` or `install` is accepted: after successful creation it opens only that new fixed source in the established editor, builds only its fixed `main.mya → main.elf` pair, builds then runs only that fixed generated output, or builds then installs only its fixed `main.elf → /apps/<name>/main.elf` pair. `run` and `install` require a regular new build output before they invoke the loader or installer. An existing project, unknown starter or trailing token is rejected without creation or overwrite; a later editor-launch, build, run or install failure preserves the completed project. `editproj <name>`, `buildproj <name>`, `runproj <name> [arguments]`, `installproj <name>` and `uninstallproj <name>` then resolve only those fixed project paths and `/apps/<name>/main.elf`, preserving the editor, assembler, existing foreground loader and package installer behavior. `runproj <name> [arguments]` revalidates and runs only the generated regular project `main.elf` without creating or replacing a package; it forwards the existing native argument tail of at most 127 visible bytes, and when the build is absent it asks for `buildproj` first. `projlist` read-only enumerates at most 128 valid project directories and shows each fixed source, build and package status. `projstatus <name>` read-only reports whether each fixed source, build and package file is `READY`, `MISSING` or `NOT REGULAR`, with its byte size when ready. `uninstallproj <name>` removes only the installed regular package `main.elf`, keeping the project source/build so `installproj` can restore the package; an absent package is reported without mutation. `cleanproj <name>` removes only the generated project `main.elf`, keeping the source and installed package so a later `buildproj` can regenerate the output. `rmproj <name>` is the final project-workspace cleanup: after `cleanproj`, it accepts only the known project entries, removes a regular `main.mya` and its now-empty project directory in order, and keeps the installed package runnable. A 16–31-character project name still runs from the shell, but its package intentionally has no GUI launcher tile because launcher names are capped at 15 printable characters. Use the general `edit` command for multi-line source; `write` remains useful for short one-line files.

```text
newproj native-args args
# Create an editable empty project and open only its new source immediately:
newproj native-author empty edit
# Create the default runnable starter and build only its fixed project pair immediately:
newproj native-built build
# Create, build and run the default starter through only its fixed generated output:
newproj native-ran run
# Create, build and install the default starter through only its fixed paths:
newproj native-installed install
buildproj native-args
runproj native-args hello MyOS
installproj native-args
run native-args hello MyOS
projlist
uninstallproj native-args
projstatus native-args
projlist
installproj native-args
cleanproj native-args
buildproj native-args
runproj native-args
cleanproj native-args
rmproj native-args
projlist
run native-args hello MyOS
```

The fixed `args` starter prints `[hello MyOS]` when arguments are forwarded, and `[]` without parameters. To replace it with an authored one-line source, use `write /users/myos/projects/native-args/main.mya write "[";args;write "]\n";time;exit 37`, then rebuild; that program also writes the current RTC time as `HH:MM:SS` and returns status `37`. The source language supports `args`, `input`, `time`, `set <0..255>`, `not`, `neg`, `inc`, `dec`, `test <0..255>`, `and <0..255>`, `or <0..255>`, `xor <0..255>`, `shl <1..7>`, `shr <1..7>`, `rol <1..7>`, `ror <1..7>`, `add <0..255>`, `sub <0..255>`, `mul <0..255>`, `div <1..255>`, `mod <1..255>`, `store <0..7>`, `load <0..7>`, `cmp <0..7>`, `swap <0..7>`, `label name:`, `write "text"`, `jump name`, `jump_if_zero name`, `jump_if_nonzero name`, `jump_if <0..255> name`, and a final `exit <0..255>`. `store` saves the current condition byte in one of eight private slots; `load` restores it as the condition and is valid before arithmetic or a conditional jump. `not`, `neg`, `inc`, `dec`, `test`, `and`, `or`, `xor`, `shl`, `shr`, `rol`, `ror`, `add`, `sub` and `mul` require an initialized condition from `input`, `set` or `load`. The bitwise operations update that byte; operand-free `neg` replaces it with its two’s-complement byte negation modulo 256 (`set 5; neg` yields `251`, while zero remains zero); operand-free `inc` increments it modulo 256 (`set 255; inc` yields `0`); operand-free `dec` decrements it modulo 256 (`set 0; dec` yields `255`); `test <mask>` normalizes the byte-wise intersection to `1` when nonzero or `0` when zero; `shl` and `shr` apply logical byte shifts of exactly 1 through 7 positions, discarding shifted-out bits; `rol` and `ror` circularly rotate the byte through the same range; `add`, `sub` and `mul` update it modulo 256. `div` has the same prerequisite, takes a nonzero divisor from `1..255`, and replaces the byte with its unsigned integer quotient; `mod` uses the same divisor range and replaces the byte with its unsigned remainder. `cmp <slot>` also requires an initialized condition, compares it with the selected private slot, and replaces it with `0` for equality or `1` for inequality. `swap <slot>` has the same prerequisite and exchanges the accumulator byte with that private slot. The slots are private to the running program, zero-initialized, and cannot be named or addressed directly. Every target must be a defined label located later in the source, so loops and backward jumps are rejected. Escapes `\n`, `\r`, `\t`, `\\`, and `\"` are available inside text. For example, `set 250; add 8; store 3; set 0; load 3; sub 2; jump_if_zero matched` demonstrates that `(250 + 8 - 2) mod 256` is zero. The sequence `set 200; mul 2; add 57; div 3` computes `67`: multiplication retains the low byte and division is unsigned integer division. The sequence `set 73; store 5; set 73; cmp 5; jump_if_zero equal` selects `equal`, while changing the active value to `72` makes `jump_if_nonzero` select the inequality path. Slot numbers outside `0..7`, add/sub/mul/and/or/xor operands outside `0..255`, `shl`/`shr`/`rol`/`ror` counts outside `1..7`, `div 0`, `mod 0`, uninitialized byte operations including `neg`, `inc`, `dec` or `test`, and uninitialized or out-of-range `cmp` or `swap` are rejected. The generated program runs in ring 3 and returns its authored exit status; use `help asm` and `help edit` for concise command help, [Text Editor](TEXT_EDITOR.md) for editing controls, and [Native Build](NATIVE_BUILD.md) for all bounds and syntax rules.

> `runproj <name> [arguments]` is the bounded development exception: it accepts only the fixed generated regular `/users/myos/projects/<name>/main.elf` after revalidation, forwards at most 127 visible native-argument bytes, and does not create or replace a package. `install` remains the explicit package boundary for persistent applications and GUI launcher discovery.

## 9. Experimental GUI

The GUI is available on the QEMU-validated `main` branch and is started from the console rather than automatically:

```text
startgui
# Compatibility alias: startgui home
```

Without an argument `startgui` opens **MYOS DESKTOP**, a bounded mouse-first launcher; `startgui home` remains an alias. Click `SYSTEM` for the system message, `NOTES` for notes, or `EDIT NOTE` for the default personal-note editor. Up to four installed packages with `/apps/<name>/main.elf` also appear below the fixed tiles as `OPEN APP`; clicking one starts that program, closes the GUI and returns its normal output to the console. Click `FILES` to start at `/users/myos/`; use `startgui projects` to enter the same File Workspace directly at `/users/myos/projects`, or `startgui project <name>` to open one existing project directory after its bounded 1–31-character name and directory type are revalidated. Add the exact `edit` suffix to open only that project's revalidated regular writable `main.mya` in the existing GUI editor. The exact read-only `status` suffix reports the fixed source, build and installed-package `READY <size> bytes`, `MISSING` or `NOT REGULAR` rows without mutation. The exact `build` suffix revalidates only the fixed regular source and invokes the established assembler for that project's `main.mya → main.elf`, closing the GUI while the result remains in the console and then exiting with the assembler status. The exact `run [arguments]` suffix revalidates only that project's fixed regular `main.elf`, launches only this bounded generated output, forwards at most the existing 127 visible-byte native tail, and then closes GUI while returning the child status. The exact `install` suffix revalidates only the same fixed regular output and invokes the established installer only for that project's `main.elf → /apps/<name>/main.elf`, closing GUI and returning installer status; existing package replacement remains intentional. The exact `uninstall` suffix revalidates only fixed regular `/apps/<name>/main.elf`, removes only that package output while preserving project source/build, and keeps GUI active with a narrow result status. The exact `clean` suffix revalidates only fixed regular `<project>/main.elf`, removes only that generated output while preserving source/package, and keeps GUI active with a narrow result status. The exact `new [hello|args|empty]` suffix accepts only the bounded name and fixed `hello` default, `args`, or editable zero-length `empty` starter, creates only `<project>/main.mya`, rejects an existing project and rolls back only its own partial creation state; GUI stays active with a narrow result status. The exact `new [hello|args|empty] edit` suffix uses that same creation contract and, after it succeeds, opens only the newly created fixed source in the existing editor; a later editor-handoff failure leaves the completed project intact. The exact `new [hello|args|empty] build` suffix uses that same creation contract and, after it succeeds, invokes the established assembler only for the new fixed `main.mya → main.elf` pair; GUI ends for console completion, and a later assembler failure leaves the completed project intact. The exact `new [hello|args|empty] install` suffix uses that same creation contract, builds only the new fixed source/output pair and, only if that output is regular, installs only its matching `/apps/<name>/main.elf` package. GUI ends for child completion; a later build or installer failure leaves the completed project intact. The exact `remove` suffix immediately revalidates the exact project directory, accepts only regular `main.mya` with absent `main.elf`, removes that source then the empty directory, preserves `/apps/<name>/main.elf`, and keeps GUI active with a narrow result status; it is not crash-transactional. Invalid or absent project requests report `UNABLE TO OPEN PROJECT`; a missing or non-regular direct source reports `UNABLE TO OPEN PROJECT SOURCE`, a missing or non-regular direct output reports `UNABLE TO OPEN PROJECT OUTPUT`, and a missing or non-regular package reports `UNABLE TO OPEN PROJECT PACKAGE`, without opening a viewer. Its window title shows the complete current logical-VFS path and updates after parent or child navigation. Click the top-bar `X` to exit. Launcher and window actions are mouse-only. The retained GUI-level keyboard shortcuts are `Alt+Tab` to move focus to the next visible window, `Alt+F4` to close the focused window, `Esc` to return or cancel, and `Ctrl+Q` to exit. For a personal note you can pass an absolute path:

```text
startgui /users/myos/files/notes/note
# Project root:
startgui projects
# Create only fixed starter source:
startgui project native-args new args
# Or create an editable regular zero-length source:
startgui project native-empty new empty
# Create an exact starter and open only its new fixed source in the GUI editor:
startgui project native-author new args edit
# Create the default starter and build only its new fixed project pair:
startgui project native-built new build
# Create, build and install the default starter through only its fixed project/package paths:
startgui project native-installed new install
# Exact existing project directory:
startgui project native-args
# Exact project source editor:
startgui project native-args edit
# Read-only project lifecycle view:
startgui project native-args status
# Build the fixed project output and return to console:
startgui project native-args build
# Run only the fixed generated project output with an optional bounded native tail:
startgui project native-args run hello MyOS
# Install only the fixed generated output into the matching package:
startgui project native-args install
# Remove only the matching installed package while source/build stay intact:
startgui project native-args uninstall
# Remove only the generated build output while source/package stay intact:
startgui project native-args clean
# Remove only a clean bounded workspace; the installed package stays intact:
startgui project native-args remove
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

MyOS is not a replacement for Linux, Windows, or BSD. On the QEMU-validated `main` branch there is currently no networking, USB HID, SMP, Secure Boot, demand paging, package manager, user accounts/permissions, a full native C compiler, or production security hardening. The restricted native assembler is implemented, but the GUI remains a bounded framebuffer environment rather than a general-purpose desktop. Physical-PC validation is not currently available, so published artifacts remain Pre-releases.

If a build or run fails, do `make clean`, then `make all img`, `make smoke`, and `make regression`. The `smoke` command headlessly checks BIOS and UEFI boot markers, persistent AHCI mount, and automatic `[myos]$` entry. The `regression` command uses a disposable image copy: it creates and saves the default GUI note through the mouse `EDIT NOTE` tile, injects QMP PS/2 `Alt+Tab` for MONITOR focus, `Alt+F4` to close focused MONITOR, `Esc` for viewer return, `Alt+F4` for editor cancel-to-viewer and `Ctrl+Q` for clean exit, then exercises the centered NOTES and FILES launcher tiles, including visible File Workspace current-path title transitions for parent and `/system` navigation, SYSTEM/MONITOR window close controls, MONITOR title-bar raise, viewer close-to-home and editor cancel-to-viewer with PPM framebuffer transitions. It also retains a BIOS `startgui home` alias check, uses direct shell `cp` to copy an editor-authored 305-byte file across its 256-byte request boundary while retaining a `run cp` compatibility rejection check, verifies exact target data and rejects overwrite, then validates direct `wc` on a 259-byte persisted file whose final word spans the 256-byte chunk boundary and retains a `run wc` compatibility check, builds and installs native packages in BIOS, verifies legacy forward-only branches, empty and forwarded native arguments, `input` exact-match and fallback paths, valid `HH:MM:SS` RTC output, modular `(250 + 8 - 2) mod 256` add/sub arithmetic with rejected uninitialized `add`, persisted `MULDIV` multiply/divide arithmetic with rejected `div 0`, persisted `EQ`/`NE` private-slot comparison with rejected uninitialized or slot-`8` `cmp`, and rejected invalid control flow, then verifies persisted files, the `cp` target and installed input/time/argument/arithmetic packages through UEFI. Both commands do not replace a physical-PC test. After that repeat the QEMU command from section 3. For host-platform setup use [PLATFORMS.md](PLATFORMS.md), for release gates use [RELEASE_STABILIZATION.md](RELEASE_STABILIZATION.md), and for technical diagnostics use [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md).
