# Native build в MyOS: первый in-OS workflow

> **Статус:** реализовано и проверено в ветке `gui/bringup`. Это первый шаг к созданию собственных программ непосредственно внутри MyOS. Он не заменяет host SDK и не является полноценным C compiler.

## Назначение

MyOS теперь содержит встроенный инструмент `asm` и user-shell команду `build`. Они читают restricted assembly source из persistent project directory, создают statically linked `x86_64 ELF64 ET_EXEC` без внешнего linker и сохраняют его как обычный persistent file. Полученный ELF затем устанавливается в approved application package `/apps/<name>/main.elf` штатной командой `install`.

Первый language intentionally small. Он позволяет написать программу, которая выводит один или несколько text fragments через MyOS syscall boundary и завершает task выбранным status code. Это даёт полный, воспроизводимый workflow **source → ELF → package → ring-3 execution** прямо на целевой ОС, не вводя неподконтрольный general-purpose compiler.

## Быстрый workflow

Создайте project directory и source file. Для первой версии source удобно хранить в одну строку с `;` между statements; `write` command shell передаёт до 256 text bytes за один вызов.

```text
mkdir /users/myos/projects/native
write /users/myos/projects/native/hello.mya write "Hello from MyOS native build\n"; exit 37
```

Соберите ELF в project directory, затем установите его в global application package и запустите коротким именем:

```text
build /users/myos/projects/native/hello.mya /users/myos/projects/native/hello.elf
install /users/myos/projects/native/hello.elf /apps/native-hello/main.elf
run native-hello
```

Ожидаемый результат содержит строку `Hello from MyOS native build`, а user shell сообщает exit status `37`. Project source и intermediate ELF остаются в `/users/myos/projects/native/`; runnable package lives only under `/apps/native-hello/main.elf` because current loader accepts executable paths only from `/system/core/apps/` and `/apps/<name>/main.elf`.

## Команды shell

| Команда | Назначение |
|---|---|
| `build <source.mya> <output.elf>` | Public workflow wrapper. It starts the built-in `asm` tool in foreground. |
| `run asm <source.mya> <output.elf>` | Direct invocation of the same assembler; useful for diagnostics. |
| `help asm` | Показывает краткий syntax reference. |
| `install <source> /apps/<name>/main.elf` | Copies built project ELF to the package location approved by the loader. |
| `run <name>` | Resolves and starts `/apps/<name>/main.elf` as a separate ring-3 task. |

Все source и output paths должны быть absolute VFS paths. The assembler does not create parent directories; create project directories with `mkdir` first. It replaces an existing output file only after it successfully parses the source and emits the new ELF image.

## Source language `.mya`

A source consists of `write` and `exit` statements. Statements are separated by a semicolon or line ending. Comments start with `#` and continue through the end of the line. Exactly one final `exit` statement is required; no statement may follow it.

| Statement | Meaning |
|---|---|
| `write "text"` | Emits `text` to standard output through `MYOS_SYS_WRITE` with descriptor `1`. The string may use `\n`, `\r`, `\t`, `\\` and `\"` escapes. Empty strings are rejected. |
| `exit <0..255>` | Calls `MYOS_SYS_EXIT` with the selected unsigned status code. |

A multi-statement source can be written across lines when a future editor supports it, or authored as one line with separators today:

```text
# A native MyOS user program
write "first line\n";
write "second line\n";
exit 0
```

The output is a loader-valid little-endian `x86_64 ELF64 ET_EXEC`. It has one page-aligned RX `PT_LOAD` segment at virtual address `0x400000`, contains direct `SYSCALL` instructions for write and exit, and has no dynamic linker, relocations, libc or host dependencies.

## Bounds and safety policy

| Boundary | Current rule |
|---|---|
| Source file | Up to 2,047 bytes; it is read through bounded 256-byte VFS requests. |
| Text literals | Up to 2,048 bytes total across the program. |
| `write` statements | Up to 32. |
| Generated ELF | Up to 8,192 bytes; it is stored by bounded 256-byte VFS writes. |
| ELF layout | One RX `PT_LOAD`, `ET_EXEC`, entry `0x400000`; no relocation or writable data segment. |
| Execution policy | Project outputs are data. A program must be installed under `/apps/<name>/main.elf` before `run <name>` can load it. |
| Scope | No labels, jumps, arithmetic, data directives, symbols, macros, object files, C syntax or external linking in this milestone. |

The bounds are part of the first security model. They keep parsing, code generation and VFS traffic static and auditable, avoid arbitrary relocation/linking logic, and prevent the assembler from reserving unbounded memory. Invalid syntax, oversized input, malformed paths or impossible output writes fail with a non-zero tool status; the shell remains usable.

## Relationship to the host SDK

The host-side [MyOS SDK](SDK_RU.md) remains the supported path for larger freestanding C11 programs. It provides a public header, startup code, linker script and host build template. Native build does not duplicate that toolchain. Instead, it establishes the in-OS storage and execution path that future stages can extend with a richer assembler, a small linker, a restricted C frontend and eventually a broader native development environment.

## Completed validation

| Check | Result |
|---|---|
| Strict build | `make all img` completed with `-Werror`. |
| BIOS source/build/install/run | A `.mya` source was created in `/users/myos/projects/native/`, `build` emitted `hello.elf`, `install` created `/apps/native-hello/main.elf`, and `run native-hello` printed its text and exited with status `37`. |
| Fresh BIOS remount | The project source and ELF remained visible; the persisted installed program executed again with the same output and status. |
| UEFI/OVMF | The same BIOS-created package executed successfully after an OVMF boot. |

## Next expansion

The next native-toolchain decision is deliberately separate. Candidate work includes labels and restricted control flow, more syscalls, a small linker, a multi-line project editor, and a constrained C frontend. None of those changes should weaken the current project/package separation or bypass the loader’s approved executable paths.
