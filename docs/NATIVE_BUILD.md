# Native build in MyOS: a restricted in-OS workflow

> **Language:** [English](NATIVE_BUILD.md) | [Русский](NATIVE_BUILD_RU.md)

> **Status:** implemented and validated in `gui/bringup`. The built-in `asm` supports text output, named labels, a bounded condition value, and forward-only unconditional or conditional jumps. It is not a general assembler, C compiler or replacement for the host [MyOS SDK](SDK.md).

## Purpose

The user-shell command `build` runs the built-in `asm` tool. It reads a `.mya` source file, generates a static `x86_64 ELF64 ET_EXEC` in memory, then writes it as an ordinary VFS file. `install` copies the ELF to `/apps/<name>/main.elf`; `run <name>` starts that package as a separate ring-3 task.

The deliberately small language validates the complete **source → ELF → package → execution** route inside MyOS without adding a general-purpose compiler, linker, relocation model or writable program data.

## Quick workflow

Create a project file that sets an explicit condition. The zero branch skips `bad` and prints only `zero`.

```text
mkdir /users/myos/projects/native
write /users/myos/projects/native/zero.mya set 0; jump_if_zero done; write "bad\n"; label done:; write "zero\n"; exit 37
build /users/myos/projects/native/zero.mya /users/myos/projects/native/zero.elf
install /users/myos/projects/native/zero.elf /apps/native-zero/main.elf
run native-zero
```

The user shell reports `zero` and exit status `37`. Use persistent project paths for output ELF files: generated images are slightly larger than 4 KiB, while the temporary VFS is intentionally small.

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
| `set <0..255>` | Stores one explicit unsigned condition value for subsequent conditional jumps. It does not provide arithmetic or variables. |
| `jump name` | Unconditionally jumps to a strictly later label. |
| `jump_if_zero name` | Jumps to a strictly later label only when the latest `set` value is zero. |
| `jump_if_nonzero name` | Jumps to a strictly later label only when the latest `set` value is non-zero. |
| `exit <0..255>` | Calls `MYOS_SYS_EXIT` with the selected status; mandatory final executable statement. |

A conditional jump requires an earlier `set` statement. `set` remains valid until replaced by another `set`; writes and labels do not clear it. Targets must exist and appear later in the source. This keeps all paths finite: loops, backward/current targets and indirect jumps are rejected.

```text
# Non-zero path: only "yes" is printed.
set 5
jump_if_nonzero yes
write "bad\n"
label yes:
write "yes\n"
exit 0
```

## Generated code and bounds

`set` emits `mov ebx, imm32`. Each conditional branch emits `test ebx, ebx` immediately followed by a fixed-size `JZ rel32` or `JNZ rel32`; it therefore does not depend on flags left by a syscall or another instruction. `jump` remains `E9 rel32`. The assembler resolves all labels before forming the ELF.

| Bound | Current rule |
|---|---|
| Source file | At most 2,047 bytes, read in bounded 256-byte VFS requests. |
| Text literals | At most 2,048 bytes total. |
| Executable statements | At most 64 total `write`, `set`, jumps and `exit` instructions. |
| Labels | At most 16 unique labels; identifiers are 1–31 ASCII characters. |
| Control flow | Forward-only `jump`, `jump_if_zero` and `jump_if_nonzero`; no loops or indirect targets. |
| Condition | One `0..255` value in generated `EBX`; no arithmetic, comparison operators or mutable memory. |
| Generated ELF | At most 8,192 bytes, written in bounded 256-byte VFS requests. |
| ELF layout | One RX `PT_LOAD`, `ET_EXEC`, entry `0x400000`; no relocations, libc, dynamic linker or writable data segment. |

These bounds keep parsing, target resolution, code generation and storage static and auditable. Invalid syntax, duplicate labels, missing or non-forward targets, conditional jumps without `set`, malformed paths or output-write failures leave the shell usable and return a non-zero status.

```text
asm: syntax error; set <0..255>, labels need ':' and jumps must target a later label
```

## Completed validation

| Check | Result |
|---|---|
| Strict build | `make all img` completes with `-Werror`. |
| BIOS zero-true branch | `set 0; jump_if_zero ...` skips `B`, prints `Z` and exits with status `31`. |
| BIOS zero-false branch | `set 9; jump_if_zero ...` continues on the false path, prints `N` and exits with status `32`. |
| BIOS nonzero-true branch | `set 7; jump_if_nonzero ...` skips `B`, prints `Y` and exits with status `33`. |
| Rejection cases | Missing `set`, ordinary backward target and conditional backward target all return the documented syntax diagnostic and status `2`. |
| Automated regression | `make regression` executes all BIOS branch/rejection cases, then runs the installed zero-true package again after UEFI/OVMF boot. |

## Relationship to the SDK and next work

The host-side [MyOS SDK](SDK.md) remains the supported route for larger freestanding C11 programs. Native build intentionally remains smaller: it proves a controlled in-OS authoring path rather than duplicating a host toolchain. The implemented [Text Editor](TEXT_EDITOR.md) is the normal in-OS way to create multi-line `.mya` source before `build`; it also edits ordinary text files. The next practical expansion is carefully selected input and time syscalls, without weakening package separation or loader policy.
