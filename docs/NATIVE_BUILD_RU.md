# Native build в MyOS: ограниченный in-OS workflow

> **Язык:** [English](NATIVE_BUILD.md) | [Русский](NATIVE_BUILD_RU.md)

> **Статус:** реализовано и проверено в `gui/bringup`. Встроенный `asm` поддерживает вывод текста, именованные метки, одно ограниченное condition value и безусловные или условные переходы только вперёд. Это не general-purpose assembler, C compiler и не замена host [MyOS SDK](SDK_RU.md).

## Назначение

Команда user shell `build` запускает встроенный инструмент `asm`. Он читает `.mya` source file, формирует static `x86_64 ELF64 ET_EXEC` в памяти и сохраняет его как обычный VFS file. Команда `install` копирует ELF в `/apps/<name>/main.elf`, а `run <name>` запускает package как отдельную ring-3 task.

Намеренно малый язык подтверждает полный путь **source → ELF → package → execution** внутри MyOS, не добавляя general-purpose compiler, linker, relocation model или writable program data.

## Быстрый workflow

Создайте project file с явным condition. Zero branch пропускает `bad` и выводит только `zero`.

```text
mkdir /users/myos/projects/native
write /users/myos/projects/native/zero.mya set 0; jump_if_zero done; write "bad\n"; label done:; write "zero\n"; exit 37
build /users/myos/projects/native/zero.mya /users/myos/projects/native/zero.elf
install /users/myos/projects/native/zero.elf /apps/native-zero/main.elf
run native-zero
```

User shell сообщает `zero` и exit status `37`. Для output ELF используйте persistent project paths: generated images немного больше 4 KiB, а temporary VFS намеренно мал.

## Команды shell

| Команда | Назначение |
|---|---|
| `build <source.mya> <output.elf>` | Public workflow wrapper; запускает `asm` в foreground. |
| `run asm <source.mya> <output.elf>` | Прямой вызов assembler для диагностики. |
| `help asm` | Показывает краткий current syntax reference. |
| `install <source> /apps/<name>/main.elf` | Копирует ELF в executable package location. |
| `run <name>` | Разрешает и запускает `/apps/<name>/main.elf`. |

Все paths должны быть absolute. Assembler не создаёт parent directories. Он разбирает весь source и формирует ELF до замены requested output file.

## Source language `.mya`

Statements разделяются `;` или концом строки. `#` начинает line comment. Program содержит один final `exit`; после него не допускается label или statement.

| Statement | Значение |
|---|---|
| `label name:` | Определяет уникальную case-sensitive position перед следующим instruction. `name` начинается с ASCII letter или `_`, затем содержит letters, digits или `_`. |
| `write "text"` | Отправляет text в standard output через `MYOS_SYS_WRITE`. Допускаются `\n`, `\r`, `\t`, `\\` и `\"`; empty strings отклоняются. |
| `set <0..255>` | Сохраняет одно явное unsigned condition value для последующих conditional jumps. Arithmetic и variables не добавляются. |
| `jump name` | Безусловно переходит на строго более позднюю label. |
| `jump_if_zero name` | Переходит на строго более позднюю label, только если последнее `set` value равно zero. |
| `jump_if_nonzero name` | Переходит на строго более позднюю label, только если последнее `set` value не равно zero. |
| `exit <0..255>` | Вызывает `MYOS_SYS_EXIT` с выбранным status; обязательный final executable statement. |

Conditional jump требует более ранний `set`. Его value остаётся valid до следующего `set`; `write` и `label` его не очищают. Target должен существовать и быть расположен позже в source. Это сохраняет все paths конечными: loops, backward/current targets и indirect jumps отклоняются.

```text
# Non-zero path: выводится только "yes".
set 5
jump_if_nonzero yes
write "bad\n"
label yes:
write "yes\n"
exit 0
```

## Generated code и bounds

`set` генерирует `mov ebx, imm32`. Каждый conditional branch генерирует `test ebx, ebx`, сразу за которым следует fixed-size `JZ rel32` или `JNZ rel32`; поэтому он не зависит от flags, оставленных syscall или другой instruction. `jump` остаётся `E9 rel32`. Assembler разрешает все labels до формирования ELF.

| Граница | Текущее правило |
|---|---|
| Source file | Не более 2 047 bytes, чтение bounded VFS requests по 256 bytes. |
| Text literals | Не более 2 048 bytes суммарно. |
| Executable statements | Не более 64 суммарных `write`, `set`, jumps и `exit` instructions. |
| Labels | Не более 16 unique labels; identifiers содержат 1–31 ASCII characters. |
| Control flow | Forward-only `jump`, `jump_if_zero` и `jump_if_nonzero`; loops и indirect targets отсутствуют. |
| Condition | Одно value `0..255` в generated `EBX`; arithmetic, comparison operators и mutable memory отсутствуют. |
| Generated ELF | Не более 8 192 bytes, запись bounded VFS writes по 256 bytes. |
| ELF layout | Один RX `PT_LOAD`, `ET_EXEC`, entry `0x400000`; relocation, libc, dynamic linker и writable data segment отсутствуют. |

Эти границы сохраняют parsing, target resolution, code generation и storage статичными и проверяемыми. Invalid syntax, duplicate labels, missing/non-forward targets, conditional jump без `set`, malformed paths или output-write failure оставляют shell usable и возвращают non-zero status.

```text
asm: syntax error; set <0..255>, labels need ':' and jumps must target a later label
```

## Завершённая проверка

| Проверка | Результат |
|---|---|
| Strict build | `make all img` завершается с `-Werror`. |
| BIOS zero-true branch | `set 0; jump_if_zero ...` пропускает `B`, выводит `Z` и завершает task с status `31`. |
| BIOS zero-false branch | `set 9; jump_if_zero ...` продолжает false path, выводит `N` и завершается с status `32`. |
| BIOS nonzero-true branch | `set 7; jump_if_nonzero ...` пропускает `B`, выводит `Y` и завершается с status `33`. |
| Rejection cases | Missing `set`, ordinary backward target и conditional backward target возвращают documented syntax diagnostic и status `2`. |
| Automated regression | `make regression` выполняет все BIOS branch/rejection cases, затем снова запускает installed zero-true package после UEFI/OVMF boot. |

## Связь с SDK и следующий шаг

Host [MyOS SDK](SDK_RU.md) остаётся поддерживаемым путём для более крупных freestanding C11 programs. Native build намеренно меньше: он подтверждает контролируемый in-OS authoring path, а не повторяет host toolchain. Реализованный [Текстовый редактор](TEXT_EDITOR_RU.md) — normal in-OS путь для создания multi-line `.mya` source перед `build`; он также редактирует ordinary text files. Следующее практическое расширение — carefully selected input и time syscalls без ослабления package separation или loader policy.
