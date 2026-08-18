# GUI bring-up: native framebuffer desktop

Этот документ описывает **экспериментальный GUI** только для ветки `gui/bringup`. Он не входит в стабильный console release `v0.12.1-console` и не изменяет назначение веток `main` или `console-stable`. GUI остаётся нативным x86_64-компонентом MyOS: он рисуется непосредственно в RGB framebuffer, без web runtime, внешнего graphical toolkit или dynamic memory allocation.

## Запуск

Сначала необходимо переключиться на GUI-ветку и собрать raw image. Для проверки persistent storage образ следует подключать к QEMU как IDE-диск.

```bash
git switch gui/bringup
make all img
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

После kernel bootstrap MyOS автоматически запускает user shell через три секунды. Нажмите `K` во время countdown, если нужна diagnostic kernel shell; в этом случае `init` сохраняется как ручной запуск user shell. Затем запустите graphical viewer. Без аргумента viewer загружает `motd.txt`; необязательный аргумент задаёт путь к читаемому VFS file.

```text
startgui
# либо: startgui disk/note
```

`startgui` является обычной ring-3 программой. Она создаёт ограниченную GUI session через существующую syscall boundary, читает и редактирует только bounded user-space payload, а kernel получает лишь проверенные syscall requests. `Q` или `Esc` за пределами editor завершает graphical session и возвращает в тот же user shell.

## Текущее поведение

| Компонент | Реализованное поведение |
|---|---|
| Renderer | Нативное прямое рисование в Limine RGB framebuffer без внешнего GUI runtime. |
| Владение session | Одновременно допустим ровно один GUI owner; kernel отклоняет вторую параллельную session. |
| Desktop | Тёмный desktop, верхняя строка состояния и нижняя строка доступных controls. |
| Windows | Три статические bounded window records: `SYSTEM`, `NOTES` и `MONITOR`. |
| Z-order | Focused window поднимается на передний план; каждый event вызывает bounded full redraw композиции. |
| Viewer | `NOTES` отображает до 128 bytes выбранного VFS file. |
| File loading | `startgui [file]` читает первые 128 bytes указанного VFS file; без аргумента используется `motd.txt`. |
| Persistent note | `D` загружает `disk/note`; если file ещё не создан, viewer отображает `UNABLE TO READ FILE`. |
| Editor entry | `E` открывает bounded editor для `disk/note`; отсутствующий file начинает с пустого draft. |
| Editor input | Printable ASCII вставляется в позиции caret; `Enter` вставляет newline; `Backspace` удаляет byte слева, `Delete` — byte под caret. |
| Caret и navigation | `Left`/`Right` перемещают caret на byte, `Up`/`Down` — по logical lines с сохранением column, `Home`/`End` переходят к границам строки. |
| Bounded scrolling | Renderer отображает окно до 20 logical newline-separated lines; viewport автоматически следует за строкой caret. |
| Save и cancel | `Ctrl-S` заменяет `disk/note`, записывает draft и возвращает к `FILE VIEWER`. `Esc` отменяет draft и перезагружает ранее сохранённое содержимое. |
| Built-in choices | `M` или `m` повторно загружает `motd.txt`. |
| Focus | `Tab`, `Enter` или `Space` вне editor переводят focus на следующее видимое окно. |
| Pointer | Строчные `W`, `A`, `S`, `D` перемещают bounded crosshair pointer. `F` фокусирует верхнее видимое окно под pointer. |
| Visibility | `1`, `2`, `3` переключают `SYSTEM`, `NOTES`, `MONITOR`; `X` скрывает focused window, не позволяя скрыть все окна. |
| Reset и выход | `R` восстанавливает исходный layout и z-order. `Q` или `Esc` вне editor завершает session и возвращает framebuffer text console. |

Буква `D` используется только в верхнем регистре для выбора persistent file. Это сохраняет строчную `d` как перемещение pointer вправо. В editor обычные printable keys становятся текстом draft и не передаются window manager, поэтому `D`, `Q` и другие символы можно вводить как часть заметки.

## Ограничения editor и граница ABI

`NOTES` использует дескриптор `MYOS_GUI_SET_CONTENT = 3` в `MYOS_SYS_GUI_SESSION`. Kernel принимает content request лишь от текущего GUI owner при активной session, копирует request после проверки отображения user buffer и не хранит user pointers. Framebuffer владеет собственными статическими copies title и data. Editor использует уже существующие persistent VFS syscalls: удаляет прежний `disk/note`, создаёт file снова и записывает один bounded payload с offset `0`.

| Поле или операция | Ограничение | Назначение |
|---|---:|---|
| `MYOS_GUI_CONTENT_TITLE_MAX` | 16 bytes | NUL-terminated title для NOTES window. |
| `MYOS_GUI_CONTENT_MAX` | 128 bytes | Максимальная длина viewer content и editor draft. |
| `struct myos_gui_content_request` | 176 bytes | `length`, `flags`, `cursor`, `viewport`, `title[16]`, `data[128]`; укладывается в syscall user-copy limit 256 bytes. |
| `struct myos_tmpfs_write_request` | 208 bytes | Reused bounded write request; также укладывается в limit 256 bytes. |
| File read | До 128 bytes | Один bounded `MYOS_SYS_VFS_READ` request из ring 3. |
| Persistent save | До 128 bytes | `PERSIST_REMOVE`, `PERSIST_CREATE`, затем `PERSIST_WRITE` для `disk/note`. |
| Allocation | Static storage | Нет heap allocations или background operations; cursor и viewport передаются как bounded scalar metadata. |

Текст переносится в пределах внутренней поверхности NOTES. Символы вне printable ASCII заменяются renderer на `?`; newline начинает следующую logical line. В editor kernel рисует cyan caret по index, переданному ring-3 программой; viewport начинается на границе logical line и удерживает строку caret в окне до 20 строк. Обновление content вызывает redraw, но не запускает layout initialization, поэтому сохраняет текущие visibility, focus и z-order window manager. Draft, который достиг 128 bytes, больше не принимает новые bytes до удаления через `Backspace` или `Delete`.

## Границы архитектуры

`kernel/console/framebuffer.c` владеет primitives отрисовки, статическими window records, z-order, pointer state и копией viewer content. `user/startgui.c` владеет ring-3 event loop, editor state, чтением VFS, отменой draft и последовательностью persistent save. `kernel/sys/syscall.c` владеет проверкой owner, user-memory copy и framebuffer setter. При завершении или принудительном уничтожении GUI owner dispatcher закрывает GUI session и очищает ownership.

| Граница | Политика |
|---|---|
| Window records | Три статических bounded records. |
| Input | Existing scheduler-safe console input path; PS/2 arrows, Home, End и Delete переводятся в internal bounded key bytes; в editor keys принадлежат draft, а не window manager. |
| Rendering | Bounded full redraw после GUI input или content update. |
| Files | Viewer читает любой доступный VFS file; editor изменяет только `disk/note`. |
| Atomicity | Save удаляет и пересоздаёт file перед записью; power-loss-safe journal пока отсутствует. |
| Mouse hardware | Не реализован; keyboard управляет visual pointer. |
| General window API | Не реализован; records остаются внутренними для framebuffer renderer. |

## Проверка milestone

Перед commit выполнены strict build и firmware regressions на QEMU Q35. BIOS и UEFI использовали один `myos.img` как IDE drive, поэтому readback подтверждает сохранение данных на том же AHCI-backed persistent partition, а не только в памяти текущего запуска.

| Проверка | Результат |
|---|---|
| `make all img` | Passed без compiler warnings и build errors. |
| `git diff --check` | Passed. |
| BIOS caret и scrolling | Passed: 24-line draft в focused `EDIT NOTE` отрисовал cyan caret; viewport ограничен 20 logical lines. |
| BIOS navigation | Passed: пять PS/2 `Up`, затем `End` и `b` переместили caret в существующую строку и изменили `a` на `ab`. |
| BIOS save | Passed: PS/2 `Ctrl-S` сохранил multiline note; `FILE VIEWER` показал тот же content с `ab`. |
| UEFI editor smoke | Passed: OVMF загрузил persistent note, открыл `EDIT NOTE` и сохранил visible caret после PS/2 `Up`. |
| Existing GUI boundaries | Retained: bounded window state, GUI owner checks, direct viewer launch and return to shell. |

Screenshots и краткие test findings находятся вне source tree в локальном `/home/ubuntu/myos-editor-validation/`; они не входят в Git commit.

## Boot UX, унаследованный из main

Automatic user-space initialization теперь реализована и интегрирована в GUI branch. После bootstrap kernel выводит трёхсекундный countdown; при отсутствии отмены он запускает `/init`, а затем можно сразу вызвать `startgui`. Это сохраняет быстрый normal path и отдельный диагностический режим без запуска GUI.

| Сценарий после загрузки | Реализованное поведение |
|---|---|
| Обычная загрузка | Kernel группирует diagnostics в четыре stage headers, выводит countdown и автоматически запускает `/init` через **3 секунды**; перед user shell framebuffer очищается. |
| Отмена | Нажатие `K` во время countdown отменяет auto-init; cancel key не передаётся user shell. |
| Kernel shell | После `K` система остаётся в diagnostic kernel shell. Команда `init` вручную запускает ту же user shell. |
| Неудача init | Если `/init` отсутствует или automatic loading не проходит, kernel выводит diagnostics и остаётся в kernel shell без retry loop. |
| Input source | Cancel path работает через существующие PS/2 keyboard и serial console input paths. |
| Проверка | BIOS normal boot, PS/2 `K` cancellation, manual `init`, isolated no-init fallback и UEFI normal boot с чистым user-shell framebuffer прошли на QEMU Q35. |

После cursor-aware editor milestone следующими GUI направлениями остаются named `disk/` files и аппаратная mouse/pointer support. Они должны сохранять отдельную GUI branch до отдельного решения о merge или release.
