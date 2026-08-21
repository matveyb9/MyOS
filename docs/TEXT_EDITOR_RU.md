# Текстовый редактор MyOS

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="TEXT_EDITOR.md">🇺🇸 ENGLISH</a>
</p>

> **Статус:** реализовано и проверено в `feature/gui`. `edit` — небольшой консольный текстовый редактор для обычных VFS files и многострочных `.mya` sources. Он намеренно дополняет GUI editor: File Workspace v1 открывает selected small writable file из `startgui` → `FILES`, а console editor остаётся более крупным general-purpose tool.

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
| Console editable document | Не более **4 096 bytes** в памяти. |
| Console file input/output | Чтение и запись используют bounded VFS requests по 256 bytes. |
| Подходящее содержимое | Printable ASCII, newline и tab; editor не предназначен для binary files. |
| File locations | Любой mutable absolute VFS file с existing parent directory. Console workflow следует VFS write policy; GUI editing File Workspace дополнительно ограничен selected existing regular files под `/users/myos/`, `/temp/`, `/system/data/` или `/system/config/`. |
| Save model | `Ctrl-S` заменяет target через remove/create и bounded writes. Undo, recovery journal, atomic rename и concurrent-edit coordination пока отсутствуют. |

Console editor limit 4 KiB намеренно меньше persistent-file open snapshot 128 KiB и намного меньше MYPFS004 ceiling 8 MiB. Отдельный GUI editor File Workspace принимает fixed document **16 KiB (16 384 bytes)** и загружает или сохраняет его через до шестидесяти четырёх неизменных VFS requests по 256 bytes. Он остаётся mouse-first editor для selected existing writable files, а console editor — general-purpose path для любого mutable absolute VFS file. Оба остаются bounded, deterministic и удобными для notes, configuration и native sources; large-file viewing и editing остаются отдельной будущей работой.

## Проверка

`make regression` создаёт и сохраняет two-line ordinary text file в console editor, проверяет exact BIOS readback, проверяет GUI editor load-save-readback 16 KiB через шестьдесят четыре VFS chunks, seeded from deterministic initramfs fixture, создаёт в editor multi-line conditional `.mya` file, собирает и запускает его installed package, затем повторяет ordinary-text readback, exact GUI payload 16 KiB и native-package execution после UEFI/OVMF boot. Regression использует disposable image и не заменяет remaining physical-PC release gate.

Общее поведение shell описано в [User Guide](USER_GUIDE_RU.md). GUI editor File Workspace и его navigation по logical VFS описаны в [GUI Bring-up](GUI_BRINGUP_RU.md).
