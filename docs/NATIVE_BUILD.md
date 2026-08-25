# Native build in MyOS: a restricted in-OS workflow

<p align="center">
  <a href="NATIVE_BUILD_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

> **Status:** implemented and validated in the QEMU-validated `main` integration line. The built-in `asm` supports text output, bounded forwarding of program arguments, single-byte input, RTC time output, named labels, one bounded condition value, and forward-only unconditional or conditional jumps. It is not a general assembler, C compiler or replacement for the host [MyOS SDK](SDK.md).

## Purpose

The user-shell command `build` runs the built-in `asm` tool. It reads a `.mya` source file, generates a static `x86_64 ELF64 ET_EXEC` in memory, then writes it as an ordinary VFS file. `install` copies the ELF to `/apps/<name>/main.elf`; `run <name>` starts that package as a separate ring-3 task.

The deliberately small language validates the complete **source → ELF → package → execution** route inside MyOS without adding a general-purpose compiler, linker, relocation model or writable user-controlled program data.

## Quick workflow

Start a project with the fixed skeleton, then edit the source before its project shortcut build/install/run path.

```text
newproj native-args
editproj native-args
buildproj native-args
runproj native-args
installproj native-args
run native-args
```

`newproj` writes a runnable `Hello from MyOS project` program. Replace its contents through `edit` to author a program such as `write "["; args; write "]\n"; time; exit 37`; then `run native-args hello MyOS` reports `[hello MyOS]`, a valid `HH:MM:SS` time line, and exit status `37`. Without parameters the same authored program reports `[]`. Use persistent project paths for output ELF files: generated images are slightly larger than 4 KiB, while the temporary VFS is intentionally small.

## Shell commands

| Command | Purpose |
|---|---|
| `newproj <project-name>` | Creates `/users/myos/projects/<project-name>/main.mya` from one fixed runnable template. Names are 1–31 ASCII letters, digits, `-` or `_`; an existing project is rejected without overwrite. |
| `editproj <project-name>` | Opens the fixed `<project>/main.mya` path through the bounded `edit` program. It creates no file or directory. |
| `buildproj <project-name>` | Runs `build` through the fixed project paths `<project>/main.mya` → `<project>/main.elf`. It creates no directory and preserves assembler replacement/error behavior. |
| `runproj <project-name> [arguments]` | Revalidates and runs only the regular generated `<project>/main.elf` through the existing foreground loader, without creating or replacing a package. It forwards the existing native argument string of at most 127 visible bytes; a missing build reports that `buildproj` must run first. |
| `installproj <project-name>` | Runs `install` from `<project>/main.elf` to `/apps/<project-name>/main.elf`; an existing package target is deliberately replaced, exactly as with `install`. |
| `projstatus <project-name>` | Reads the fixed `main.mya`, `main.elf` and `/apps/<name>/main.elf` directory entries and reports `READY <size> bytes`, `MISSING` or `NOT REGULAR`. It makes no mutation. |
| `cleanproj <project-name>` | Removes only an existing regular `<project>/main.elf`. It keeps `main.mya` and `/apps/<name>/main.elf`; a missing output is reported without mutation and `buildproj` can create it again. |
| `build <source.mya> <output.elf>` | Public workflow wrapper; runs `asm` in the foreground. |
| `run asm <source.mya> <output.elf>` | Direct assembler invocation for diagnostics. |
| `help asm` | Shows the concise current syntax reference. |
| `install <source> /apps/<name>/main.elf` | Copies an ELF into an executable package location. |
| `run <name>` | Resolves and runs `/apps/<name>/main.elf`. |

All paths must be absolute. `newproj` is the only project helper that creates its fixed directory and `main.mya` template; it uses existing VFS create/write operations, rejects an existing directory and removes only the directory or file it just created if its own later setup step fails. A project name of 16–31 characters remains valid for shell `run`, but its installed package is intentionally not launcher-tile eligible: the GUI accepts at most 15 printable name characters so its framebuffer and user-space action mappings remain identical. `editproj`, `buildproj`, `runproj`, `installproj`, `projstatus` and `cleanproj` accept the same restricted name and only construct their fixed existing project/package paths. `editproj`, `buildproj` and `installproj` delegate to the established `edit`, `asm` and `install` programs; `runproj` revalidates a regular `main.elf` then delegates to the existing foreground loader without mutation, forwarding only the ordinary native argument tail of at most 127 visible bytes. Its kernel loader allowlist admits only that exact bounded `/users/myos/projects/<name>/main.elf` path and still rejects every other `/users/...` executable path; `projstatus` reads at most 128 entries in each fixed parent directory, and `cleanproj` revalidates a regular `main.elf` before its one remove request. They do not add a VFS operation, change established package-replacement behavior or promise crash-transactional persistence. The assembler itself does not create parent directories. It parses the full source and builds the ELF before replacing the requested output file.

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
| `neg` | Negates the initialized current unsigned byte modulo 256. |
| `inc` | Increments the initialized current unsigned byte modulo 256. |
| `dec` | Decrements the initialized current unsigned byte modulo 256. |
| `clz` | Counts leading zero bits in the initialized byte; `0` yields `8`, while a nonzero byte yields `0..7`. |
| `parity` | Normalizes the initialized current byte to `1` when it has even parity or `0` when it has odd parity. |
| `test <0..255>` | Computes the byte-wise intersection of the initialized current condition and one unsigned byte, then normalizes the condition to `1` when nonzero or `0` when zero. |
| `and <0..255>` | Applies bitwise AND between the initialized current unsigned byte and one unsigned byte. |
| `or <0..255>` | Applies bitwise OR between the initialized current unsigned byte and one unsigned byte. |
| `xor <0..255>` | Applies bitwise exclusive OR between the initialized current unsigned byte and one unsigned byte. |
| `shl <1..7>` | Logically shifts the initialized current unsigned byte left by 1 through 7 positions, discarding shifted-out bits. |
| `shr <1..7>` | Logically shifts the initialized current unsigned byte right by 1 through 7 positions, discarding shifted-out bits. |
| `rol <1..7>` | Rotates the initialized current unsigned byte left by 1 through 7 positions, reintroducing shifted-out bits at the right edge. |
| `ror <1..7>` | Rotates the initialized current unsigned byte right by 1 through 7 positions, reintroducing shifted-out bits at the left edge. |
| `add <0..255>` | Adds one unsigned byte to the initialized current condition, wrapping modulo 256. |
| `sub <0..255>` | Subtracts one unsigned byte from the initialized current condition, wrapping modulo 256. |
| `mul <0..255>` | Multiplies the initialized current condition by one unsigned byte, retaining the low byte modulo 256. |
| `div <1..255>` | Divides the initialized current condition by one nonzero unsigned byte and retains the unsigned integer quotient. |
| `mod <1..255>` | Divides the initialized current condition by one nonzero unsigned byte and retains the unsigned remainder. |
| `store <0..7>` | Copies the current unsigned condition byte into one of eight private program-variable slots. It does not change the current condition. |
| `load <0..7>` | Restores one private program-variable byte as the current condition for subsequent conditional jumps. |
| `cmp <0..7>` | Compares the initialized current condition with one private slot and replaces it with `0` when equal or `1` when different. |
| `swap <0..7>` | Exchanges the initialized current condition byte with one private slot. |
| `jump name` | Unconditionally jumps to a strictly later label. |
| `jump_if_zero name` | Jumps to a strictly later label only when the current condition value is zero. |
| `jump_if_nonzero name` | Jumps to a strictly later label only when the current condition value is non-zero. |
| `jump_if <0..255> name` | Jumps to a strictly later label only when the current condition value exactly matches the selected unsigned byte. |
| `exit <0..255>` | Calls `MYOS_SYS_EXIT` with the selected status; mandatory final executable statement. |

`not`, `neg`, `inc`, `dec`, `clz`, `parity`, `test`, `and`, `or`, `xor`, `shl`, `shr`, `rol`, `ror`, `add`, `sub`, `mul`, `div`, `mod`, `cmp` and `swap` require an earlier `input`, `set` or `load` statement. `not`, `neg`, `inc`, `dec`, `and`, `or`, `xor`, `shl`, `shr`, `rol`, `ror`, `add`, `sub` and `mul` retain that initialized single condition value as a byte; `clz` counts leading zero bits, yielding `8` for zero; `parity` normalizes even parity to `1` or odd parity to `0`; `test` instead normalizes a byte-wise intersection to `0` or `1`. Arithmetic wraps modulo 256, bitwise operations apply to its eight bits, logical shifts accept only 1 through 7 positions before discarding shifted-out bits, and circular rotates accept the same counts while reintroducing those bits at the opposite edge. `div` replaces it with the unsigned integer quotient, while `mod` replaces it with the unsigned remainder; both reject zero as their source operand; `cmp` replaces it with `0` for equality or `1` for inequality against the selected private slot. A conditional jump also requires that initialized value. `store` copies it without changing it, while writes, time output and labels do not clear it. Targets must exist and appear later in the source. This keeps source-level paths finite: loops, backward/current targets and indirect jumps are rejected. `input` may wait for human or serial input, but it does not add a source-language loop.

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

The emitted entry prologue saves the loader-provided argument pointer in the fixed private data segment. `args` reloads that pointer, scans no more than 127 bytes for its NUL terminator, and issues one write syscall only when the supplied string is non-empty. `set` emits `mov ebx, imm32`. `input` writes a single byte to data offset `8` with `MYOS_SYS_READ`, reloads the scratch pointer after the syscall boundary, filters `CR` and `LF`, then places the accepted byte in `EBX`. `time` reads the fixed `myos_rtc_time` layout at offset `8` of the same private 32-byte area and formats hours, minutes and seconds as two decimal digits each before one write syscall. `store` emits one absolute byte store from `BL` into offset `24..31`; `load` emits one zero-extending absolute byte load into `EBX` from the same selected slot. `not` emits `not bl`; `neg` emits `neg bl`; `inc` emits `inc bl`; `dec` emits `dec bl`; `clz` emits a bounded zero branch plus `bsr ecx, ebx; mov eax, 7; sub eax, ecx; movzx ebx, al`; `parity` emits `test bl, bl; setp bl; movzx ebx, bl`; `test` emits `test bl, imm8; setne bl; movzx ebx, bl`; `and`, `or` and `xor` emit `and bl, imm8`, `or bl, imm8` and `xor bl, imm8`, respectively; `shl` and `shr` emit `shl bl, imm8` and `shr bl, imm8`; `rol` and `ror` emit `rol bl, imm8` and `ror bl, imm8`, all with a validated count from 1 through 7. `add` and `sub` emit `add bl, imm8` and `sub bl, imm8`, respectively. `mul` emits `mov eax, ebx; imul eax, eax, imm32; movzx ebx, al`; `div` emits `mov eax, ebx; xor edx, edx; mov ecx, imm32; div ecx; movzx ebx, al`; `mod` uses the same safe unsigned divide sequence and emits `movzx ebx, dl` for the remainder; `cmp` emits `cmp bl, byte [absolute slot]; setne bl; movzx ebx, bl`; `swap` emits `xchg bl, byte [absolute slot]`. The required nonzero divisor and byte accumulator ensure division cannot trap or produce a quotient above 255, while every arithmetic or comparison operation restores a zero-extended byte in `EBX` for later branches.

Each zero/non-zero branch emits `test ebx, ebx` immediately followed by a fixed-size `JZ rel32` or `JNZ rel32`. `jump_if` emits `cmp ebx, imm32` followed by `JZ rel32`. The generated branches therefore do not depend on flags left by a syscall or another instruction. `jump` remains `E9 rel32`, and the assembler resolves all labels before forming the ELF.

| Bound | Current rule |
|---|---|
| Source file | At most 2,047 bytes, read in bounded 256-byte VFS requests. |
| Text literals | At most 2,048 bytes total. |
| Executable statements | At most 64 total `write`, `args`, `input`, `time`, `set`, `not`, `neg`, `inc`, `dec`, `clz`, `parity`, `test`, `and`, `or`, `xor`, `shl`, `shr`, `rol`, `ror`, `add`, `sub`, `mul`, `div`, `mod`, `store`, `load`, `cmp`, `swap`, jump and `exit` instructions. |
| Arguments | Existing loader ABI string from `run <name> [arguments]`, at most 127 visible bytes; `args` only reads and writes it. |
| Labels | At most 16 unique labels; identifiers are 1–31 ASCII characters. |
| Control flow | Forward-only `jump`, `jump_if_zero`, `jump_if_nonzero` and `jump_if`; no source-level loops or indirect targets. |
| Condition | One `0..255` value in generated `EBX`, initialized by `input`, `set` or `load`; `not`, `neg`, `inc`, `dec`, `and`, `or` and `xor` update its eight bits; `clz` counts leading zero bits, yielding `8` for zero; `parity` normalizes even parity to `1` or odd parity to `0`; `test` normalizes a byte-wise intersection to `0` or `1`, `shl` and `shr` shift its byte logically by 1..7 positions, while `rol` and `ror` rotate its byte circularly by the same range, `add`, `sub` and `mul` update its low byte modulo 256, `div` uses an unsigned quotient and `mod` uses an unsigned remainder, each with a required `1..255` divisor, and `cmp <0..7>` writes `0` for equality or `1` for inequality against a private slot. There is no general user-addressable mutable memory. |
| Generated ELF | At most 8,192 bytes, written in bounded 256-byte VFS requests. |
| ELF layout | One RX `PT_LOAD` at `0x400000` plus a fixed 32-byte RW `PT_LOAD` at `0x401000`: bytes `0..7` retain the entry argument pointer, bytes `8..23` are private input/time scratch data, and bytes `24..31` are eight zero-initialized `store`/`load` slots. No relocations, libc or dynamic linker are present. |

These bounds keep parsing, target resolution, code generation and storage static and auditable. Invalid syntax, duplicate labels, a store/load/cmp slot outside `0..7`, an and/or/xor/test/add/sub/mul operand outside `0..255`, a shl/shr/rol/ror count outside `1..7`, a div/mod divisor outside `1..255`, bitwise/shift/arithmetic/cmp or a conditional jump without `input`, `set` or `load`, missing or non-forward targets, malformed paths or output-write failures leave the shell usable and return a non-zero status.

```text
asm: syntax error; set/load/input must precede not/neg/inc/dec/clz/parity/test/add/sub/mul/div/mod/and/or/xor/shl/shr/rol/ror/cmp/swap and conditional jumps, add/sub/mul/and/or/xor/test are byte values 0..255, shl/shr/rol/ror are 1..7, div/mod are 1..255, store/load/cmp/swap slots are 0..7, labels need ':' and jumps must target a later label
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
| BIOS byte leading-zero count | A program computes `clz 32 = 2` and `clz 0 = 8`, selects both expected branches, prints `CLZ` and exits with status `63`; uninitialized `clz` is rejected. |
| BIOS byte parity predicate | A program normalizes byte `3` to even parity and byte `1` to odd parity, selects both expected paths, prints `PARITY` and exits with status `62`; uninitialized `parity` is rejected. |
| BIOS byte test predicate | A program normalizes `160 test 128` to nonzero and `160 test 15` to zero, selects both expected paths, prints `TEST` and exits with status `61`; uninitialized `test` is rejected. |
| BIOS private-slot swap | A program stores `73` in slot `4`, sets `12`, swaps with slot `4`, selects the `73` branch, prints `SWAP` and exits with status `60`; uninitialized `swap` is rejected. |
| BIOS byte decrement | A program computes `dec 0 = 255` modulo 256, stores/reloads the byte, takes the exact branch, prints `DEC` and exits with status `59`; uninitialized `dec` is rejected. |
| BIOS byte increment | A program computes `inc 255 = 0` modulo 256, stores/reloads the byte, takes the zero branch, prints `INC` and exits with status `58`; uninitialized `inc` is rejected. |
| BIOS byte negation | A program computes `neg 7 = 249` modulo 256, stores/reloads the byte, takes the exact branch, prints `NEG` and exits with status `57`; uninitialized `neg` is rejected. |
| BIOS bitwise byte operations | A program initializes byte `240`, applies operand-free `not`, then `and 63` and `or 128` to obtain byte `143`, stores/reloads it, takes the exact branch, prints `BITWISE` and exits with status `52`; uninitialized `not` and `and 256` are rejected. |
| BIOS exclusive-or byte operation | A program computes `170 xor 255 xor 85 = 0`, stores/reloads the byte, takes the zero branch, prints `XOR` and exits with status `53`; uninitialized `xor` and `xor 256` are rejected. |
| BIOS logical byte shifts | A program computes `3 shl 5 shr 4 = 6`, stores/reloads the byte, takes the exact branch, prints `SHIFT` and exits with status `54`; uninitialized shifts, `shl 0` and `shr 8` are rejected. |
| BIOS byte rotates | A program computes `129 rol 1 ror 2 = 192`, stores/reloads the byte, takes the exact branch, prints `ROTATE` and exits with status `55`; uninitialized rotates, `rol 0` and `ror 8` are rejected. |
| BIOS multiply/divide | A third program computes `((200 * 2 mod 256) + 57) / 3 = 67`, stores and reloads the quotient, takes its zero branch after subtracting `67`, prints `MULDIV` and exits with status `50`; `div 0` is rejected. |
| BIOS byte remainder | A program computes `200 mod 57 = 29`, stores/reloads the byte, takes the exact branch, prints `MOD` and exits with status `56`; uninitialized `mod` and `mod 0` are rejected. |
| BIOS private comparison | A fourth program stores `73` in slot `5`, verifies equality with `cmp 5` through `jump_if_zero`, then verifies inequality after `set 72` through `jump_if_nonzero`; it prints `EQ` and `NE`, exits with status `51`, and rejects uninitialized or slot-`8` comparisons. |
| Rejection cases | A missing condition, ordinary backward target and exact-conditional backward target all return the documented syntax diagnostic and status `2`. |
| UEFI persistence | Installed input/time, argument and variable packages are run again after UEFI/OVMF boot; `A` selects its expected path, `[ovmf args]` is rendered from forwarded arguments, the `store`/`load` package selects `VAR`, the persisted add/sub arithmetic package selects `ARITH`, the persisted mul/div package selects `MULDIV`, the persisted XOR package selects `XOR`, the persisted shift package selects `SHIFT`, the persisted rotate package selects `ROTATE`, the persisted remainder package selects `MOD`, the persisted negation package selects `NEG`, the persisted increment package selects `INC`, the persisted decrement package selects `DEC`, the persisted swap package selects `SWAP`, the persisted leading-zero package selects `CLZ`, the persisted parity package selects `PARITY`, the persisted test package selects `TEST`, the persisted comparison package selects both `EQ` and `NE`, and each time line remains valid. |
| Project direct-run lifecycle | BIOS proves `buildproj` → `runproj` executes the uninstalled generated template, then an edited project forwards its normal native argument string directly before `installproj` packages it; after `cleanproj`, the package still runs while the build is missing, then a rebuild runs directly again before final clean. UEFI confirms persisted source/package with the final build output missing. |
| Automated regression | `make regression` runs the disposable-image BIOS GUI/editor/native workflow, then verifies persistent files and the installed native packages on UEFI/OVMF. |

## Relationship to the SDK and next work

The host-side [MyOS SDK](SDK.md) remains the supported route for larger freestanding C11 programs. Native build intentionally remains smaller: it proves a controlled in-OS authoring path rather than duplicating a host toolchain. The implemented [Text Editor](TEXT_EDITOR.md) is the normal in-OS way to create multi-line `.mya` source before `build`; it also edits ordinary text files. Future native-toolchain work will remain bounded and must preserve package separation, loader policy and the forward-only source control-flow guarantee.
