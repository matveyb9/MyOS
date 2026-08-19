# Текстовый редактор MyOS

> **Язык:** [English](TEXT_EDITOR.md) | [Русский](TEXT_EDITOR_RU.md)

> **Статус:** реализовано и проверено в `gui/bringup`. `edit` — небольшой консольный текстовый редактор для обычных VFS files и многострочных `.mya` sources. Он намеренно отделён от GUI note editor, который остаётся GUI-функцией для заметок.

## Начало редактирования

Используйте absolute path. Parent directory должен уже существовать. Existing file открывается с cursor в последнем byte; отсутствующий file создаётся как пустой text document.

```text
edit /users/myos/projects/hello.mya
```

Editor перерисовывает компактный text viewport после каждого input event. Видимый marker `|` — это byte cursor; он не сохраняется в file.

| Управление | Действие |
|---|---|
| Printable ASCII | Вставляет character в позицию cursor. |
| `Enter` | Вставляет newline. |
| `Left`, `Right` | Перемещает на один byte. |
| `Home`, `End` | Переходит в начало или конец current line. |
| `Up`, `Down` | Переходит между lines, сохраняя column где возможно. |
| `Backspace`, `Delete` | Удаляет preceding или current byte. |
| `Ctrl-S` | Заменяет file edited document и завершает editor. |
| `Ctrl-Q` или `Esc` | Завершает editor без сохранения in-memory edits. |

Старый вариант `run edit <absolute-file>` остаётся совместимым, но normal workflow — прямой `edit <absolute-file>`. Команда `help edit` показывает те же command и key controls в user shell.

## Написание native program

Editor — общий text tool; написание `.mya` — его первый development-oriented use. Создайте source с настоящими line breaks, сохраните его, затем соберите и установите как обычно.

```text
edit /users/myos/projects/zero.mya
# Наберите и сохраните Ctrl-S:
set 0
jump_if_zero done
write "bad\n"
label done:
write "editor-built\n"
exit 44

build /users/myos/projects/zero.mya /users/myos/projects/zero.elf
install /users/myos/projects/zero.elf /apps/editor-zero/main.elf
run editor-zero
```

Program выводит только `editor-built` и возвращает status `44`. Полная restricted `.mya` grammar и safety bounds описаны в [Native Build](NATIVE_BUILD_RU.md).

## Bounds и save behavior

| Элемент | Текущее правило |
|---|---|
| Editable document | Не более **4 096 bytes** в памяти. |
| File input/output | Чтение и запись используют bounded VFS requests по 256 bytes. |
| Подходящее содержимое | Printable ASCII, newline и tab; editor не предназначен для binary files. |
| File locations | Любой mutable absolute VFS file с existing parent directory, включая `/users/myos/`, data paths в `/apps/` и `/temp/`. |
| Save model | `Ctrl-S` заменяет target через remove/create и bounded writes. Undo, recovery journal, atomic rename и concurrent-edit coordination пока отсутствуют. |

Limit 4 KiB намеренно меньше persistent-file open snapshot 128 KiB и намного меньше MYPFS004 ceiling 8 MiB. Это делает первый all-in-memory editor небольшим, deterministic и удобным для notes, configuration и native sources. Large-file viewing и editing остаются отдельной будущей работой.

## Проверка

`make regression` создаёт и сохраняет two-line ordinary text file в console editor, проверяет exact BIOS readback, создаёт в editor multi-line conditional `.mya` file, собирает и запускает его installed package, затем повторяет ordinary-text readback и native-package execution после UEFI/OVMF boot. Regression использует disposable image и не заменяет remaining physical-PC release gate.

Общее поведение shell описано в [User Guide](USER_GUIDE_RU.md). GUI note editor и его отдельная navigation описаны в [GUI Bring-up](GUI_BRINGUP_RU.md).
