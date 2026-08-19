# Native build в MyOS: ограниченный in-OS workflow

> **Язык:** [English](NATIVE_BUILD.md) | [Русский](NATIVE_BUILD_RU.md)


> **Статус:** реализовано и проверено в ветке `gui/bringup`. Встроенный инструмент `asm` поддерживает вывод текста, именованные метки и безусловные переходы только вперёд. Это первый контролируемый шаг к написанию собственных программ непосредственно внутри MyOS; он не заменяет host SDK и не является полноценным компилятором C.

## Назначение

MyOS содержит встроенный инструмент `asm` и команду user shell `build`. Они читают ограниченный assembly source из persistent project directory, создают статически связанный `x86_64 ELF64 ET_EXEC` без внешнего linker и сохраняют его как обычный persistent file. Полученный ELF затем устанавливается в approved application package `/apps/<name>/main.elf` штатной командой `install`.

Язык `.mya` намеренно мал. Он позволяет вывести один или несколько text fragments, пропустить участок линейного кода через именованный переход вперёд и завершить task выбранным status code. Тем самым внутри целевой ОС подтверждён воспроизводимый путь **source → ELF → package → ring-3 execution** без введения неподконтрольного general-purpose compiler.

## Быстрый workflow

Сначала создайте directory и source file. Для текущего shell удобна однострочная запись с `;` между statements: команда `write` передаёт не более 256 text bytes за один вызов.

```text
mkdir /users/myos/projects/native
write /users/myos/projects/native/forward.mya write "before jump\n"; jump done; write "this text is skipped\n"; label done:; exit 37
```

Соберите ELF в project directory, установите его в global application package и запустите коротким именем:

```text
build /users/myos/projects/native/forward.mya /users/myos/projects/native/forward.elf
install /users/myos/projects/native/forward.elf /apps/native-forward/main.elf
run native-forward
```

Ожидаемый результат содержит только строку `before jump`; строка `this text is skipped` не выводится, а user shell сообщает exit status `37`. Source и intermediate ELF остаются в `/users/myos/projects/native/`; runnable package находится только в `/apps/native-forward/main.elf`, поскольку текущий loader принимает executable paths лишь из `/system/core/apps/` и `/apps/<name>/main.elf`.

## Команды shell

| Команда | Назначение |
|---|---|
| `build <source.mya> <output.elf>` | Public workflow wrapper. Запускает встроенный `asm` в foreground. |
| `run asm <source.mya> <output.elf>` | Прямой вызов того же assembler, полезный для диагностики. |
| `help asm` | Показывает краткий current syntax reference в user shell. |
| `install <source> /apps/<name>/main.elf` | Копирует собранный ELF в package location, одобренный loader. |
| `run <name>` | Разрешает и запускает `/apps/<name>/main.elf` как отдельную ring-3 task. |

Все source и output paths должны быть absolute VFS paths. Assembler не создаёт parent directories: сначала используйте `mkdir`. Source разбирается и ELF полностью формируется в памяти до начала замены output file.

## Source language `.mya`

Source состоит из `label`, `write`, `jump` и одного final `exit` statement. Statements разделяются `;` или концом строки. Комментарий начинается с `#` и действует до конца строки. После `exit` не допускается ни statement, ни label.

| Statement | Значение |
|---|---|
| `label name:` | Определяет именованную позицию перед следующим instruction. `name` начинается с ASCII letter или `_`, затем может содержать letters, digits и `_`. Метки case-sensitive, уникальны и обязательно оканчиваются `:`. |
| `write "text"` | Выводит `text` в standard output через `MYOS_SYS_WRITE` с descriptor `1`. В строке допускаются `\n`, `\r`, `\t`, `\\` и `\"`; empty strings отклоняются. |
| `jump name` | Генерирует безусловный x86_64 near jump на label `name`. Target label должен быть определён и располагаться **после** jump в source. Unknown labels, переходы на текущую позицию и backward jumps отклоняются. |
| `exit <0..255>` | Вызывает `MYOS_SYS_EXIT` с выбранным unsigned status code. Это обязательный последний executable statement. |

Например, следующий source выводит `first line`, пропускает второй `write` и завершает task с кодом `0`:

```text
# Forward-only control flow
write "first line\n";
jump finish;
write "unreachable in this program\n";
label finish:
exit 0
```

Assembler кодирует `jump` как fixed-size x86_64 `E9 rel32`. Поскольку target известен после разбора всех statements, assembler сначала собирает bounded instruction list, разрешает label targets, а затем формирует ELF. Эта модель не поддерживает loops и не разрешает переход на участок кода, уже выполненный или расположенный раньше.

Выходные данные представляют собой loader-valid little-endian `x86_64 ELF64 ET_EXEC` с одним page-aligned RX `PT_LOAD` segment по virtual address `0x400000`. ELF содержит прямые `SYSCALL` instructions для write и exit; dynamic linker, relocations, libc и host dependencies отсутствуют.

## Bounds and safety policy

| Граница | Текущее правило |
|---|---|
| Source file | Не более 2 047 bytes; чтение выполняется bounded VFS requests по 256 bytes. |
| Text literals | Не более 2 048 bytes суммарно на программу. |
| Executable statements | Не более 64 суммарных `write`, `jump` и `exit` instructions. |
| Labels | Не более 16 уникальных labels; identifier содержит от 1 до 31 ASCII character. |
| Control flow | Только `jump` на строго более позднюю label. Loops, backward jumps, conditional branches и indirect jumps отсутствуют. |
| Generated ELF | Не более 8 192 bytes; хранение выполняется bounded VFS writes по 256 bytes. |
| ELF layout | Один RX `PT_LOAD`, `ET_EXEC`, entry `0x400000`; relocation и writable data segment отсутствуют. |
| Execution policy | Project outputs являются data. Program необходимо установить в `/apps/<name>/main.elf`, прежде чем `run <name>` сможет его загрузить. |
| Out of scope | Arithmetic, data directives, symbols beyond bounded labels, macros, object files, C syntax и external linking. |

Границы являются частью начальной security model: они сохраняют parsing, target resolution, code generation и VFS traffic статичными и проверяемыми, исключают unbounded memory reservation и не вводят arbitrary relocation/linking logic. Неверный syntax, слишком большой input, duplicate label, missing/non-forward target, malformed path или невозможная запись output завершают инструмент с non-zero status; shell остаётся usable.

Для structural errors и неверных targets `asm` сообщает:

```text
asm: syntax error; labels need ':' and jumps must target a later label
```

## Relationship to the host SDK

Host-side [MyOS SDK](SDK_RU.md) остаётся поддерживаемым путём для более крупных freestanding C11 programs. Он содержит public header, startup code, linker script и host build template. Native build не дублирует эту toolchain; он создаёт in-OS storage и execution path, который позднее можно расширить условными переходами, дополнительными syscalls, small linker, multi-line editor и ограниченным C frontend.

## Completed validation

| Проверка | Результат |
|---|---|
| Strict build | `make all img` завершилась с `-Werror`. |
| BIOS forward jump | Source `write "bad\n"; jump done; write "good\n"; label done:; exit 23` был собран, установлен как `/apps/forward-jump/main.elf` и запущен. Отображается только `bad`; task завершается с status `23`. |
| BIOS backward-target rejection | Source с `label start:`, `jump start` был отклонён с documented syntax diagnostic и status `2`; ELF не создан. |
| UEFI/OVMF persistence | Package, созданный в BIOS, сохранился после переключения firmware и был успешно запущен в UEFI. Он снова вывел только `bad` и завершился с status `23`. |
| Automated regression | `make regression` теперь включает forward-jump package, проверку skipped code в BIOS и UEFI, а также backward-jump rejection. |

## Next expansion

Следующим изолированным решением будет ограниченный conditional control flow с явными compare/set operations. Затем возможны дополнительные syscalls, multi-line project editor и small linker. Более полное C subset, базовая C-библиотека, build scripts и перенос крупного compiler могут рассматриваться только после стабилизации этих ограниченных шагов. Ни одно расширение не должно ослаблять project/package separation или обходить approved executable paths loader.
