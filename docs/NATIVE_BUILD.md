# Native build in MyOS: a restricted in-OS workflow

> **Language:** [English](NATIVE_BUILD.md) | [Русский](NATIVE_BUILD_RU.md)


> **Status:** implemented and validated in branch `gui/bringup`. The built-in tool `asm` supports printing text, named labels and unconditional forward-only jumps. This is the first controlled step toward writing your own programs directly inside MyOS; it does not replace the host SDK and is not a full C compiler.

## Purpose

MyOS contains a built-in `asm` tool and a user shell command `build`. They read a restricted assembly source from a persistent project directory, produce a statically linked `x86_64 ELF64 ET_EXEC` without an external linker and save it as a regular persistent file. The resulting ELF is then installed into an approved application package `/apps/<name>/main.elf` by the standard `install` command.

The `.mya` language is intentionally small. It allows emitting one or more text fragments, skipping a region of linear code via a named forward jump, and terminating the task with a chosen status code. This confirms an in-target-OS reproducible path **source → ELF → package → ring-3 execution** without introducing an uncontrolled general-purpose compiler.

## Quick workflow

First create a directory and a source file. For the current shell a one-line form with `;` between statements is convenient: the `write` command sends no more than 256 text bytes per call.

```text
mkdir /users/myos/projects/native
write /users/myos/projects/native/forward.mya write "before jump\n"; jump done; write "this text is skipped\n"; label done:; exit 37
```

Build the ELF in the project directory, install it into a global application package and run it by short name:

```text
build /users/myos/projects/native/forward.mya /users/myos/projects/native/forward.elf
install /users/myos/projects/native/forward.elf /apps/native-forward/main.elf
run native-forward
```

The expected output contains only the line `before jump`; the line `this text is skipped` is not printed, and the user shell reports exit status `37`. The source and intermediate ELF remain in `/users/myos/projects/native/`; the runnable package exists only at `/apps/native-forward/main.elf`, because the current loader accepts executable paths only from `/system/core/apps/` and `/apps/<name>/main.elf`.

## Shell commands

| Command | Purpose |
|---|---|
| `build <source.mya> <output.elf>` | Public workflow wrapper. Runs the built-in `asm` in the foreground. |
| `run asm <source.mya> <output.elf>` | Direct invocation of the same assembler, useful for diagnostics. |
| `help asm` | Shows a brief current syntax reference in the user shell. |
| `install <source> /apps/<name>/main.elf` | Copies the built ELF into the package location approved by the loader. |
| `run <name>` | Resolves and runs `/apps/<name>/main.elf` as a separate ring-3 task. |

All source and output paths must be absolute VFS paths. The assembler does not create parent directories: use `mkdir` first. The source is parsed and the ELF is fully assembled in memory before beginning to replace the output file.

## Source language `.mya`

The source consists of `label`, `write`, `jump` and a single final `exit` statement. Statements are separated by `;` or line breaks. A comment begins with `#` and runs to the end of the line. No statements or labels are allowed after `exit`.

| Statement | Meaning |
|---|---|
| `label name:` | Defines a named position before the next instruction. `name` starts with an ASCII letter or `_`, then may contain letters, digits and `_`. Labels are case-sensitive, unique and must end with `:`. |
| `write "text"` | Emits `text` to standard output via `MYOS_SYS_WRITE` with descriptor `1`. The string supports `\n`, `\r`, `\t`, `\\` and `\"`; empty strings are rejected. |
| `jump name` | Generates an unconditional x86_64 near jump to label `name`. The target label must be defined and located **after** the jump in the source. Unknown labels, jumps to the current position and backward jumps are rejected. |
| `exit <0..255>` | Invokes `MYOS_SYS_EXIT` with the chosen unsigned status code. This is the mandatory final executable statement. |

For example, the following source prints `first line`, skips the second `write` and terminates the task with code `0`:

```text
# Forward-only control flow
write "first line\n";
jump finish;
write "unreachable in this program\n";
label finish:
exit 0
```

The assembler encodes `jump` as a fixed-size x86_64 `E9 rel32`. Because the target is known only after parsing all statements, the assembler first collects a bounded instruction list, resolves label targets, and then forms the ELF. This model does not support loops and does not permit jumps to code that has already been executed or that appears earlier.

The output is a loader-valid little-endian `x86_64 ELF64 ET_EXEC` with a single page-aligned RX `PT_LOAD` segment at virtual address `0x400000`. The ELF contains direct `SYSCALL` instructions for write and exit; there is no dynamic linker, relocations, libc or host dependencies.

## Bounds and safety policy

| Bound | Current rule |
|---|---|
| Source file | No more than 2 047 bytes; reading is done via bounded VFS requests of 256 bytes. |
| Text literals | No more than 2 048 bytes total per program. |
| Executable statements | No more than 64 total `write`, `jump` and `exit` instructions. |
| Labels | No more than 16 unique labels; an identifier contains 1 to 31 ASCII characters. |
| Control flow | Only `jump` to a strictly later label. No loops, backward jumps, conditional branches or indirect jumps. |
| Generated ELF | No more than 8 192 bytes; storage is done via bounded VFS writes of 256 bytes. |
| ELF layout | One RX `PT_LOAD`, `ET_EXEC`, entry `0x400000`; no relocations or writable data segment. |
| Execution policy | Project outputs are data. A program must be installed to `/apps/<name>/main.elf` before `run <name>` can load it. |
| Out of scope | Arithmetic, data directives, symbols beyond bounded labels, macros, object files, C syntax and external linking. |

These bounds are part of the initial security model: they keep parsing, target resolution, code generation and VFS traffic static and auditable, prevent unbounded memory reservation and do not introduce arbitrary relocation/linking logic. Invalid syntax, oversized input, duplicate label, missing/non-forward target, malformed path or inability to write the output cause the tool to exit with a non-zero status; the shell remains usable.

For structural errors and invalid targets `asm` reports:

```text
asm: syntax error; labels need ':' and jumps must target a later label
```

## Relationship to the host SDK

The host-side [MyOS SDK](SDK.md) remains the supported route for larger freestanding C11 programs. It contains the public header, startup code, linker script and host build template. The native build does not duplicate that toolchain; it creates an in-OS storage and execution path that can later be extended with conditional branches, additional syscalls, a small linker, a multi-line editor and a restricted C frontend.

## Completed validation

| Check | Result |
|---|---|
| Strict build | `make all img` completed with `-Werror`. |
| BIOS forward jump | The source `write "bad\n"; jump done; write "good\n"; label done:; exit 23` was built, installed as `/apps/forward-jump/main.elf` and run. Only `bad` is shown; the task exits with status `23`. |
| BIOS backward-target rejection | A source with `label start:`, `jump start` was rejected with the documented syntax diagnostic and status `2`; no ELF was created. |
| UEFI/OVMF persistence | The package created under BIOS persisted after switching firmware and was successfully run in UEFI. It again printed only `bad` and exited with status `23`. |
| Automated regression | `make regression` now includes the forward-jump package, checks for skipped code in BIOS and UEFI, and the backward-jump rejection. |

## Next expansion

The next isolated feature will be restricted conditional control flow with explicit compare/set operations. Then additional syscalls, a multi-line project editor and a small linker are possible. A fuller C subset, a basic C library, build scripts and porting a large compiler can be considered only after stabilizing these limited steps. No extension should weaken project/package separation or bypass the loader’s approved executable paths.
