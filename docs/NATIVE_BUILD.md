# Native build in MyOS: a restricted in-OS workflow

> [🇷🇺 РУССКИЙ](NATIVE_BUILD_RU.md) / **🇺🇸 ENGLISH**

> **Status:** implemented and validated in `gui/bringup`. The built-in `asm` supports text output, bounded forwarding of program arguments, single-byte input, RTC time output, named labels, one bounded condition value, and forward-only unconditional or conditional jumps. It is not a general assembler, C compiler or replacement for the host [MyOS SDK](SDK.md).

## Purpose

The user-shell command `build` runs the built-in `asm` tool. It reads a `.mya` source file, generates a static `x86_64 ELF64 ET_EXEC` in memory, then writes it as an ordinary VFS file. `install` copies the ELF to `/apps/<name>/main.elf`; `run <name>` starts that package as a separate ring-3 task.

The deliberately small language validates the complete **source → ELF → package → execution** route inside MyOS without adding a general-purpose compiler, linker, relocation model or writable user-controlled program data.

## Quick workflow

This program prints the exact argument string passed after `run native-args`, enclosed in brackets, then prints the RTC time as `HH:MM:SS`.

```text
mkdir /users/myos/projects/native
write /users/myos/projects/native/args.mya write "["; args; write "]\n"; time; exit 37
build /users/myos/projects/native/args.mya /users/myos/projects/native/args.elf
install /users/myos/projects/native/args.elf /apps/native-args/main.elf
run native-args hello MyOS
```

The user shell reports `[hello MyOS]`, a valid `HH:MM:SS` time line, and exit status `37`. Without parameters the same program reports `[]`. Use persistent project paths for output ELF files: generated images are slightly larger than 4 KiB, while the temporary VFS is intentionally small.

## Shell commands

| Command | Purpose |
|---|---|
| `build <source.mya> <output.elf>` | Public workflow wrapper; runs `asm` in the foreground. |
| `run asm <source.mya> <output.elf>` | Direct assembler invocation for diagnostics. |
| `help asm` | Shows the concise current syntax reference. |
| `install <source> /apps/<name>/main.elf` | Copies an ELF into an executable package location. |
| `run <name>` | Resolves and runs `/apps/<name>/main.elf`. |

All paths must be absolute. The assembler does not create parent directories. It parses the full source and builds the ELF before replacing the requested output file.

## Source language `.mya`

Statements are separated by `;` or line breaks. `#` starts a line comment. A program has one final `exit`; no label or statement may follow it.

| Statement | Meaning |
|---|---|
| `label name:` | Defines a unique, case-sensitive position before the next instruction. `name` starts with an ASCII letter or `_`, then uses letters, digits or `_`. |
| `write "text"` | Sends text to standard output with `MYOS_SYS_WRITE`. It accepts `\n`, `\r`, `\t`, `\\` and `\"`; empty strings are rejected. |
| `input` | Blocks for one byte through `MYOS_SYS_READ`, discards `CR` and `LF`, and stores the resulting unsigned byte (`0..255`) as the current condition. It therefore consumes the line delimiter left by a terminal command before accepting the next meaningful byte. |
| `time` | Reads the RTC through `MYOS_SYS_RTC_TIME` and writes one nine-byte `HH:MM:SS\n` line. It has no source-level arguments or time arithmetic. |
| `args` | Writes the exact NUL-terminated argument string provided after `run <name>`, up to 127 visible bytes. It writes nothing when no arguments were supplied, does not append a newline, and does not change the current condition value. |
| `set <0..255>` | Stores one explicit unsigned condition value for subsequent conditional jumps. It does not provide arithmetic or variables. |
| `jump name` | Unconditionally jumps to a strictly later label. |
| `jump_if_zero name` | Jumps to a strictly later label only when the current condition value is zero. |
| `jump_if_nonzero name` | Jumps to a strictly later label only when the current condition value is non-zero. |
| `jump_if <0..255> name` | Jumps to a strictly later label only when the current condition value exactly matches the selected unsigned byte. |
| `exit <0..255>` | Calls `MYOS_SYS_EXIT` with the selected status; mandatory final executable statement. |

A conditional jump requires an earlier `input` or `set` statement. Both replace the same single condition value; writes, time output and labels do not clear it. Targets must exist and appear later in the source. This keeps source-level paths finite: loops, backward/current targets and indirect jumps are rejected. `input` may wait for human or serial input, but it does not add a source-language loop.

```text
# Exact match: only uppercase A selects the first path.
input
jump_if 65 matched
write "other\n"
jump done
label matched:
write "A\n"
label done:
time
exit 0
```

## Generated code and bounds

The emitted entry prologue saves the loader-provided argument pointer in the fixed private data segment. `args` reloads that pointer, scans no more than 127 bytes for its NUL terminator, and issues one write syscall only when the supplied string is non-empty. `set` emits `mov ebx, imm32`. `input` writes a single byte to data offset `8` with `MYOS_SYS_READ`, reloads the scratch pointer after the syscall boundary, filters `CR` and `LF`, then places the accepted byte in `EBX`. `time` reads the fixed `myos_rtc_time` layout at offset `8` of the same private 32-byte area and formats hours, minutes and seconds as two decimal digits each before one write syscall.

Each zero/non-zero branch emits `test ebx, ebx` immediately followed by a fixed-size `JZ rel32` or `JNZ rel32`. `jump_if` emits `cmp ebx, imm32` followed by `JZ rel32`. The generated branches therefore do not depend on flags left by a syscall or another instruction. `jump` remains `E9 rel32`, and the assembler resolves all labels before forming the ELF.

| Bound | Current rule |
|---|---|
| Source file | At most 2,047 bytes, read in bounded 256-byte VFS requests. |
| Text literals | At most 2,048 bytes total. |
| Executable statements | At most 64 total `write`, `args`, `input`, `time`, `set`, jump and `exit` instructions. |
| Arguments | Existing loader ABI string from `run <name> [arguments]`, at most 127 visible bytes; `args` only reads and writes it. |
| Labels | At most 16 unique labels; identifiers are 1–31 ASCII characters. |
| Control flow | Forward-only `jump`, `jump_if_zero`, `jump_if_nonzero` and `jump_if`; no source-level loops or indirect targets. |
| Condition | One `0..255` value in generated `EBX`, set only by `input` or `set`; no arithmetic, variables or user-addressable mutable memory. |
| Generated ELF | At most 8,192 bytes, written in bounded 256-byte VFS requests. |
| ELF layout | One RX `PT_LOAD` at `0x400000` plus a fixed 32-byte RW `PT_LOAD` at `0x401000`: bytes `0..7` retain the entry argument pointer, and bytes `8..31` are private input/time scratch data. No relocations, libc or dynamic linker are present. |

These bounds keep parsing, target resolution, code generation and storage static and auditable. Invalid syntax, duplicate labels, missing or non-forward targets, a conditional jump without `input` or `set`, malformed paths or output-write failures leave the shell usable and return a non-zero status.

```text
asm: syntax error; input/set must precede conditional jumps, labels need ':' and jumps must target a later label
```

## Completed validation

| Check | Result |
|---|---|
| Strict build | `make all img` completes with `-Werror`. |
| BIOS zero/non-zero branches | Existing `set` programs retain the zero-true, zero-false and nonzero-true behaviors. |
| BIOS native input | One installed program accepts `A` and `B` on separate runs, selects the corresponding exact-match and fallback paths, and exits with status `46`. |
| BIOS native arguments | One installed package renders both `[]` with no parameters and `[alpha beta]` for forwarded arguments, retains valid time output and exits with status `47`. |
| BIOS RTC output | The same program emits a line matching valid `HH:MM:SS` ranges. |
| Rejection cases | A missing condition, ordinary backward target and exact-conditional backward target all return the documented syntax diagnostic and status `2`. |
| UEFI persistence | Installed input/time and argument packages are run again after UEFI/OVMF boot; `A` selects its expected path, `[ovmf args]` is rendered from forwarded arguments, and each time line remains valid. |
| Automated regression | `make regression` runs the disposable-image BIOS GUI/editor/native workflow, then verifies persistent files and the installed native packages on UEFI/OVMF. |

## Relationship to the SDK and next work

The host-side [MyOS SDK](SDK.md) remains the supported route for larger freestanding C11 programs. Native build intentionally remains smaller: it proves a controlled in-OS authoring path rather than duplicating a host toolchain. The implemented [Text Editor](TEXT_EDITOR.md) is the normal in-OS way to create multi-line `.mya` source before `build`; it also edits ordinary text files. Future native-toolchain work will remain bounded and must preserve package separation, loader policy and the forward-only source control-flow guarantee.
