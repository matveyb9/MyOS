# Native build в MyOS: ограниченный in-OS workflow

> **Язык:** [English](NATIVE_BUILD.md) | [Русский](NATIVE_BUILD_RU.md)

> **Статус:** реализовано и проверено в `gui/bringup`. Встроенный `asm` поддерживает вывод текста, bounded forwarding program arguments, ввод одного байта, вывод времени RTC, именованные метки, одно ограниченное condition value и безусловные или условные переходы только вперёд. Это не general-purpose assembler, C compiler и не замена host [MyOS SDK](SDK_RU.md).

## Назначение

Команда user shell `build` запускает встроенный инструмент `asm`. Он читает `.mya` source file, формирует static `x86_64 ELF64 ET_EXEC` в памяти и сохраняет его как обычный VFS file. Команда `install` копирует ELF в `/apps/<name>/main.elf`, а `run <name>` запускает package как отдельную ring-3 task.

Намеренно малый язык подтверждает полный путь **source → ELF → package → execution** внутри MyOS, не добавляя general-purpose compiler, linker, relocation model или writable user-controlled program data.

## Быстрый workflow

Эта программа выводит точную argument string, переданную после `run native-args`, внутри квадратных скобок, затем выводит время RTC в виде `HH:MM:SS`.

```text
mkdir /users/myos/projects/native
write /users/myos/projects/native/args.mya write "["; args; write "]\n"; time; exit 37
build /users/myos/projects/native/args.mya /users/myos/projects/native/args.elf
install /users/myos/projects/native/args.elf /apps/native-args/main.elf
run native-args hello MyOS
```

User shell сообщает `[hello MyOS]`, корректную строку времени `HH:MM:SS` и exit status `37`. Без parameters та же программа выводит `[]`. Для output ELF используйте persistent project paths: generated images немного больше 4 KiB, а temporary VFS намеренно мал.

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
| `input` | Блокируется до получения одного байта через `MYOS_SYS_READ`, отбрасывает `CR` и `LF` и сохраняет полученный unsigned byte (`0..255`) как текущее condition value. Поэтому после terminal-команды он пропускает line delimiter и принимает следующий значимый байт. |
| `time` | Читает RTC через `MYOS_SYS_RTC_TIME` и выводит одну строку из девяти байт `HH:MM:SS\n`. Source-level arguments и time arithmetic отсутствуют. |
| `args` | Выводит exact NUL-terminated argument string, переданную после `run <name>`, не более 127 visible bytes. При отсутствии parameters ничего не выводит, не добавляет newline и не изменяет текущее condition value. |
| `set <0..255>` | Сохраняет одно явное unsigned condition value для последующих conditional jumps. Arithmetic и variables не добавляются. |
| `jump name` | Безусловно переходит на строго более позднюю label. |
| `jump_if_zero name` | Переходит на строго более позднюю label, только если текущее condition value равно zero. |
| `jump_if_nonzero name` | Переходит на строго более позднюю label, только если текущее condition value не равно zero. |
| `jump_if <0..255> name` | Переходит на строго более позднюю label, только если текущее condition value точно совпадает с выбранным unsigned byte. |
| `exit <0..255>` | Вызывает `MYOS_SYS_EXIT` с выбранным status; обязательный final executable statement. |

Conditional jump требует более ранний `input` или `set`. Обе инструкции заменяют одно и то же condition value; `write`, `time` и `label` его не очищают. Target должен существовать и быть расположен позже в source. Это сохраняет source-level paths конечными: loops, backward/current targets и indirect jumps отклоняются. `input` может ждать human или serial input, но не добавляет source-language loop.

```text
# Exact match: только заглавная A выбирает первый путь.
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

## Generated code и bounds

Generated entry prologue сохраняет loader-provided argument pointer в fixed private data segment. `args` заново загружает этот pointer, сканирует не более 127 bytes до NUL terminator и выполняет один write syscall, только когда supplied string не пуста. `set` генерирует `mov ebx, imm32`. `input` помещает один байт в data offset `8` через `MYOS_SYS_READ`, перезагружает scratch pointer после syscall boundary, отфильтровывает `CR` и `LF`, затем помещает принятый байт в `EBX`. `time` читает fixed layout `myos_rtc_time` по offset `8` той же private 32-byte area и форматирует часы, минуты и секунды в two decimal digits перед одним write syscall.

Каждый zero/non-zero branch генерирует `test ebx, ebx`, сразу за которым следует fixed-size `JZ rel32` или `JNZ rel32`. `jump_if` генерирует `cmp ebx, imm32`, за которым следует `JZ rel32`. Поэтому generated branches не зависят от flags, оставленных syscall или другой instruction. `jump` остаётся `E9 rel32`, а assembler разрешает все labels до формирования ELF.

| Граница | Текущее правило |
|---|---|
| Source file | Не более 2 047 bytes, чтение bounded VFS requests по 256 bytes. |
| Text literals | Не более 2 048 bytes суммарно. |
| Executable statements | Не более 64 суммарных `write`, `args`, `input`, `time`, `set`, jump и `exit` instructions. |
| Arguments | Existing loader ABI string из `run <name> [arguments]`, не более 127 visible bytes; `args` только читает и выводит её. |
| Labels | Не более 16 unique labels; identifiers содержат 1–31 ASCII characters. |
| Control flow | Forward-only `jump`, `jump_if_zero`, `jump_if_nonzero` и `jump_if`; source-level loops и indirect targets отсутствуют. |
| Condition | Одно value `0..255` в generated `EBX`, задаваемое только `input` или `set`; arithmetic, variables и user-addressable mutable memory отсутствуют. |
| Generated ELF | Не более 8 192 bytes, запись bounded VFS writes по 256 bytes. |
| ELF layout | Один RX `PT_LOAD` по `0x400000` и fixed RW `PT_LOAD` размером 32 bytes по `0x401000`: bytes `0..7` сохраняют entry argument pointer, а bytes `8..31` — private input/time scratch data. Relocation, libc и dynamic linker отсутствуют. |

Эти границы сохраняют parsing, target resolution, code generation и storage статичными и проверяемыми. Invalid syntax, duplicate labels, missing/non-forward targets, conditional jump без `input` или `set`, malformed paths или output-write failure оставляют shell usable и возвращают non-zero status.

```text
asm: syntax error; input/set must precede conditional jumps, labels need ':' and jumps must target a later label
```

## Завершённая проверка

| Проверка | Результат |
|---|---|
| Strict build | `make all img` завершается с `-Werror`. |
| BIOS zero/non-zero branches | Существующие программы с `set` сохраняют поведение zero-true, zero-false и nonzero-true. |
| BIOS native input | Одна установленная программа на отдельных запусках принимает `A` и `B`, выбирает соответствующие exact-match и fallback paths и завершается со status `46`. |
| BIOS native arguments | Один installed package выводит и `[]` без parameters, и `[alpha beta]` для forwarded arguments, сохраняет valid time output и завершается со status `47`. |
| BIOS RTC output | Та же программа выводит строку, соответствующую допустимым диапазонам `HH:MM:SS`. |
| Rejection cases | Missing condition, ordinary backward target и exact-conditional backward target возвращают documented syntax diagnostic и status `2`. |
| UEFI persistence | Установленные input/time и argument packages снова запускаются после UEFI/OVMF boot; `A` выбирает ожидаемый path, `[ovmf args]` выводится из forwarded arguments, а каждая time line остаётся корректной. |
| Automated regression | `make regression` выполняет disposable-image BIOS GUI/editor/native workflow, затем проверяет persistent files и installed native packages при UEFI/OVMF. |

## Связь с SDK и следующий шаг

Host [MyOS SDK](SDK_RU.md) остаётся поддерживаемым путём для более крупных freestanding C11 programs. Native build намеренно меньше: он подтверждает контролируемый in-OS authoring path, а не повторяет host toolchain. Реализованный [Текстовый редактор](TEXT_EDITOR_RU.md) — normal in-OS путь для создания multi-line `.mya` source перед `build`; он также редактирует ordinary text files. Будущая работа над native toolchain останется ограниченной и должна сохранять package separation, loader policy и гарантию forward-only source control flow.
