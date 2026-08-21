# Native build in MyOS: a restricted in-OS workflow

<p align="center">
  <a href="NATIVE_BUILD_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

> **Status:** implemented and validated in `feature/gui`. The built-in `asm` supports text output, bounded forwarding of program arguments, single-byte input, RTC time output, named labels, one bounded condition value, and forward-only unconditional or conditional jumps. It is not a general assembler, C compiler or replacement for the host [MyOS SDK](SDK.md).

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
| `set <0..255>` | Stores one explicit unsigned condition value for subsequent arithmetic or conditional jumps. |
| `not` | Bitwise-complements every bit of the initialized current unsigned byte. |
| `and <0..255>` | Applies bitwise AND between the initialized current unsigned byte and one unsigned byte. |
| `or <0..255>` | Applies bitwise OR between the initialized current unsigned byte and one unsigned byte. |
| `xor <0..255>` | Applies bitwise exclusive OR between the initialized current unsigned byte and one unsigned byte. |
| `shl <1..7>` | Logically shifts the initialized current unsigned byte left by 1 through 7 positions, discarding shifted-out bits. |
| `shr <1..7>` | Logically shifts the initialized current unsigned byte right by 1 through 7 positions, discarding shifted-out bits. |
| `add <0..255>` | Adds one unsigned byte to the initialized current condition, wrapping modulo 256. |
| `sub <0..255>` | Subtracts one unsigned byte from the initialized current condition, wrapping modulo 256. |
| `mul <0..255>` | Multiplies the initialized current condition by one unsigned byte, retaining the low byte modulo 256. |
| `div <1..255>` | Divides the initialized current condition by one nonzero unsigned byte and retains the unsigned integer quotient. |
| `store <0..7>` | Copies the current unsigned condition byte into one of eight private program-variable slots. It does not change the current condition. |
| `load <0..7>` | Restores one private program-variable byte as the current condition for subsequent conditional jumps. |
| `cmp <0..7>` | Compares the initialized current condition with one private slot and replaces it with `0` when equal or `1` when different. |
| `jump name` | Unconditionally jumps to a strictly later label. |
| `jump_if_zero name` | Jumps to a strictly later label only when the current condition value is zero. |
| `jump_if_nonzero name` | Jumps to a strictly later label only when the current condition value is non-zero. |
| `jump_if <0..255> name` | Jumps to a strictly later label only when the current condition value exactly matches the selected unsigned byte. |
| `exit <0..255>` | Calls `MYOS_SYS_EXIT` with the selected status; mandatory final executable statement. |

`not`, `and`, `or`, `xor`, `shl`, `shr`, `add`, `sub`, `mul`, `div` and `cmp` require an earlier `input`, `set` or `load` statement. `not`, `and`, `or`, `xor`, `shl`, `shr`, `add`, `sub` and `mul` retain that initialized single condition value as a byte; arithmetic wraps modulo 256, bitwise operations apply to its eight bits, and shifts accept only 1 through 7 positions before discarding shifted-out bits. `div` replaces it with the unsigned integer quotient and rejects zero as its source operand; `cmp` replaces it with `0` for equality or `1` for inequality against the selected private slot. A conditional jump also requires that initialized value. `store` copies it without changing it, while writes, time output and labels do not clear it. Targets must exist and appear later in the source. This keeps source-level paths finite: loops, backward/current targets and indirect jumps are rejected. `input` may wait for human or serial input, but it does not add a source-language loop.

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

The emitted entry prologue saves the loader-provided argument pointer in the fixed private data segment. `args` reloads that pointer, scans no more than 127 bytes for its NUL terminator, and issues one write syscall only when the supplied string is non-empty. `set` emits `mov ebx, imm32`. `input` writes a single byte to data offset `8` with `MYOS_SYS_READ`, reloads the scratch pointer after the syscall boundary, filters `CR` and `LF`, then places the accepted byte in `EBX`. `time` reads the fixed `myos_rtc_time` layout at offset `8` of the same private 32-byte area and formats hours, minutes and seconds as two decimal digits each before one write syscall. `store` emits one absolute byte store from `BL` into offset `24..31`; `load` emits one zero-extending absolute byte load into `EBX` from the same selected slot. `not` emits `not bl`; `and`, `or` and `xor` emit `and bl, imm8`, `or bl, imm8` and `xor bl, imm8`, respectively; `shl` and `shr` emit `shl bl, imm8` and `shr bl, imm8` with a validated count from 1 through 7. `add` and `sub` emit `add bl, imm8` and `sub bl, imm8`, respectively. `mul` emits `mov eax, ebx; imul eax, eax, imm32; movzx ebx, al`; `div` emits `mov eax, ebx; xor edx, edx; mov ecx, imm32; div ecx; movzx ebx, al`; `cmp` emits `cmp bl, byte [absolute slot]; setne bl; movzx ebx, bl`. The required nonzero divisor and byte accumulator ensure division cannot trap or produce a quotient above 255, while every arithmetic or comparison operation restores a zero-extended byte in `EBX` for later branches.

Each zero/non-zero branch emits `test ebx, ebx` immediately followed by a fixed-size `JZ rel32` or `JNZ rel32`. `jump_if` emits `cmp ebx, imm32` followed by `JZ rel32`. The generated branches therefore do not depend on flags left by a syscall or another instruction. `jump` remains `E9 rel32`, and the assembler resolves all labels before forming the ELF.

| Bound | Current rule |
|---|---|
| Source file | At most 2,047 bytes, read in bounded 256-byte VFS requests. |
| Text literals | At most 2,048 bytes total. |
| Executable statements | At most 64 total `write`, `args`, `input`, `time`, `set`, `not`, `and`, `or`, `xor`, `shl`, `shr`, `add`, `sub`, `mul`, `div`, `store`, `load`, `cmp`, jump and `exit` instructions. |
| Arguments | Existing loader ABI string from `run <name> [arguments]`, at most 127 visible bytes; `args` only reads and writes it. |
| Labels | At most 16 unique labels; identifiers are 1–31 ASCII characters. |
| Control flow | Forward-only `jump`, `jump_if_zero`, `jump_if_nonzero` and `jump_if`; no source-level loops or indirect targets. |
| Condition | One `0..255` value in generated `EBX`, initialized by `input`, `set` or `load`; `not`, `and`, `or` and `xor` update its eight bits, `shl` and `shr` shift its byte logically by 1..7 positions, `add`, `sub` and `mul` update its low byte modulo 256, `div` uses an unsigned quotient with a required `1..255` divisor, and `cmp <0..7>` writes `0` for equality or `1` for inequality against a private slot. There is no general user-addressable mutable memory. |
| Generated ELF | At most 8,192 bytes, written in bounded 256-byte VFS requests. |
| ELF layout | One RX `PT_LOAD` at `0x400000` plus a fixed 32-byte RW `PT_LOAD` at `0x401000`: bytes `0..7` retain the entry argument pointer, bytes `8..23` are private input/time scratch data, and bytes `24..31` are eight zero-initialized `store`/`load` slots. No relocations, libc or dynamic linker are present. |

These bounds keep parsing, target resolution, code generation and storage static and auditable. Invalid syntax, duplicate labels, a store/load/cmp slot outside `0..7`, an and/or/xor/add/sub/mul operand outside `0..255`, a shl/shr count outside `1..7`, a divisor outside `1..255`, bitwise/shift/arithmetic/cmp or a conditional jump without `input`, `set` or `load`, missing or non-forward targets, malformed paths or output-write failures leave the shell usable and return a non-zero status.

```text
asm: syntax error; set/load/input must precede not/add/sub/mul/div/and/or/xor/shl/shr/cmp and conditional jumps, add/sub/mul/and/or/xor are byte values 0..255, shl/shr are 1..7, div is 1..255, store/load/cmp slots are 0..7, labels need ':' and jumps must target a later label
```

## Completed validation

| Check | Result |
|---|---|
| Strict build | `make all img` completes with `-Werror`. |
| BIOS zero/non-zero branches | Existing `set` programs retain the zero-true, zero-false and nonzero-true behaviors. |
| BIOS native input | One installed program accepts `A` and `B` on separate runs, selects the corresponding exact-match and fallback paths, and exits with status `46`. |
| BIOS native arguments | One installed package renders both `[]` with no parameters and `[alpha beta]` for forwarded arguments, retains valid time output and exits with status `47`. |
| BIOS RTC output | The same program emits a line matching valid `HH:MM:SS` ranges. |
| BIOS native variables | A built program stores `73` in slot `2`, overwrites the active condition, reloads slot `2`, selects the exact-match `VAR` branch and exits with status `48`. Slot `8` is rejected. |
| BIOS byte arithmetic | A second program computes `(250 + 8 - 2) mod 256`, stores and reloads the intermediate byte, takes its zero branch, prints `ARITH` and exits with status `49`; uninitialized `add` is rejected. |
| BIOS bitwise byte operations | A program initializes byte `240`, applies operand-free `not`, then `and 63` and `or 128` to obtain byte `143`, stores/reloads it, takes the exact branch, prints `BITWISE` and exits with status `52`; uninitialized `not` and `and 256` are rejected. |
| BIOS exclusive-or byte operation | A program computes `170 xor 255 xor 85 = 0`, stores/reloads the byte, takes the zero branch, prints `XOR` and exits with status `53`; uninitialized `xor` and `xor 256` are rejected. |
| BIOS logical byte shifts | A program computes `3 shl 5 shr 4 = 6`, stores/reloads the byte, takes the exact branch, prints `SHIFT` and exits with status `54`; uninitialized shifts, `shl 0` and `shr 8` are rejected. |
| BIOS multiply/divide | A third program computes `((200 * 2 mod 256) + 57) / 3 = 67`, stores and reloads the quotient, takes its zero branch after subtracting `67`, prints `MULDIV` and exits with status `50`; `div 0` is rejected. |
| BIOS private comparison | A fourth program stores `73` in slot `5`, verifies equality with `cmp 5` through `jump_if_zero`, then verifies inequality after `set 72` through `jump_if_nonzero`; it prints `EQ` and `NE`, exits with status `51`, and rejects uninitialized or slot-`8` comparisons. |
| Rejection cases | A missing condition, ordinary backward target and exact-conditional backward target all return the documented syntax diagnostic and status `2`. |
| UEFI persistence | Installed input/time, argument and variable packages are run again after UEFI/OVMF boot; `A` selects its expected path, `[ovmf args]` is rendered from forwarded arguments, the `store`/`load` package selects `VAR`, the persisted add/sub arithmetic package selects `ARITH`, the persisted mul/div package selects `MULDIV`, the persisted XOR package selects `XOR`, the persisted shift package selects `SHIFT`, the persisted comparison package selects both `EQ` and `NE`, and each time line remains valid. |
| Automated regression | `make regression` runs the disposable-image BIOS GUI/editor/native workflow, then verifies persistent files and the installed native packages on UEFI/OVMF. |

## Relationship to the SDK and next work

The host-side [MyOS SDK](SDK.md) remains the supported route for larger freestanding C11 programs. Native build intentionally remains smaller: it proves a controlled in-OS authoring path rather than duplicating a host toolchain. The implemented [Text Editor](TEXT_EDITOR.md) is the normal in-OS way to create multi-line `.mya` source before `build`; it also edits ordinary text files. Future native-toolchain work will remain bounded and must preserve package separation, loader policy and the forward-only source control-flow guarantee.
