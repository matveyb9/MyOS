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

`startgui` является обычной ring-3 программой. Она создаёт ограниченную GUI session через существующую syscall boundary, читает и редактирует только bounded user-space payload, а kernel получает лишь проверенные syscall requests. `startgui disk/<name>` выбирает persistent file ещё до его создания: viewer сообщит об отсутствии, а `E` откроет пустой draft, который `Ctrl-S` создаст. `Q` или `Esc` за пределами editor завершает graphical session и возвращает в тот же user shell.

## Persistent user programs

GUI branch теперь может запускать отдельные MyOS ELF64 из persistent namespace `disk/bin/<name>`. Сначала встроенная программа initramfs копируется в выбранный disk slot, затем она запускается обычной shell command `run` как отдельный ring-3 process.

```text
install hello disk/bin/hello
run disk/bin/hello

install argshow disk/bin/args
run disk/bin/args alpha beta
```

| Граница | Правило |
|---|---|
| Install source | Existing initramfs или VFS file до 32 KiB. |
| Target | Только `disk/bin/<name>`; это explicit executable namespace, а не general-purpose nested directory model. |
| Loader | Принимает только little-endian x86_64 ELF64 `ET_EXEC` с valid load segments и entry внутри mapped load segment. |
| Storage | До 8 persistent records по 32 KiB; `install` копирует по 128-byte syscall chunks, а VFS записывает только затронутые AHCI sectors. |
| Failure | Invalid content, oversized source, invalid path или невозможный load безопасно отклоняются; shell остаётся usable. |

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
| Persistent selection | `D` выбирает стандартный `disk/note`; `N` циклически выбирает следующий существующий persistent `disk/` file через existing VFS enumeration. |
| Named launch | `startgui disk/<name>` выбирает конкретный допустимый persistent path; title NOTES показывает `DISK:<name>`. |
| Editor entry | `E` открывает bounded editor для выбранного persistent file; отсутствующий selected path начинает с пустого draft. |
| Editor input | Printable ASCII вставляется в позиции caret; `Enter` вставляет newline; `Backspace` удаляет byte слева, `Delete` — byte под caret. |
| Caret и navigation | `Left`/`Right` перемещают caret на byte, `Up`/`Down` — по logical lines с сохранением column, `Home`/`End` переходят к границам строки. |
| Bounded scrolling | Renderer отображает окно до 20 logical newline-separated lines; viewport автоматически следует за строкой caret. |
| Save и cancel | `Ctrl-S` заменяет выбранный `disk/` file, записывает draft и возвращает к его viewer. `Esc` отменяет draft и перезагружает ранее сохранённое содержимое. |
| Built-in choices | `M` или `m` повторно загружает `motd.txt`. |
| Focus | `Tab`, `Enter` или `Space` вне editor переводят focus на следующее видимое окно. |
| Hardware pointer | PS/2 mouse relative motion перемещает bounded crosshair pointer; rising edge левой кнопки фокусирует верхнее видимое окно под pointer. |
| Keyboard fallback | Строчные `W`, `A`, `S`, `D` перемещают pointer на 16 pixels; `F` сохраняет keyboard focus верхнего окна под pointer. |
| Visibility | `1`, `2`, `3` переключают `SYSTEM`, `NOTES`, `MONITOR`; `X` скрывает focused window, не позволяя скрыть все окна. |
| Reset и выход | `R` восстанавливает исходный layout и z-order. `Q` или `Esc` вне editor завершает session и возвращает framebuffer text console. |

Буква `D` используется только в верхнем регистре для выбора `disk/note`; `N` или `n` циклически выбирает следующий существующий `disk/` file. Это сохраняет строчную `d` как перемещение pointer вправо. В editor обычные printable keys становятся текстом draft и не передаются window manager, поэтому `D`, `N`, `Q` и другие символы можно вводить как часть заметки.

## Ограничения editor и граница ABI

`NOTES` использует дескриптор `MYOS_GUI_SET_CONTENT = 3` в `MYOS_SYS_GUI_SESSION`. Kernel принимает content request лишь от текущего GUI owner при активной session, копирует request после проверки отображения user buffer и не хранит user pointers. Framebuffer владеет собственными статическими copies title и data. GUI циклически читает existing `MYOS_SYS_VFS_ENTRY` records, фильтрует допустимые `disk/` paths и держит выбранный path в static 40-byte storage. Editor использует уже существующие persistent VFS syscalls: удаляет выбранный file, создаёт его снова и записывает один bounded payload с offset `0`.

| Поле или операция | Ограничение | Назначение |
|---|---:|---|
| `MYOS_GUI_CONTENT_TITLE_MAX` | 16 bytes | NUL-terminated title для NOTES window. |
| `MYOS_GUI_CONTENT_MAX` | 128 bytes | Максимальная длина viewer content и editor draft. |
| `struct myos_gui_content_request` | 176 bytes | `length`, `flags`, `cursor`, `viewport`, `title[16]`, `data[128]`; укладывается в syscall user-copy limit 256 bytes. |
| `struct myos_tmpfs_write_request` | 208 bytes | Reused bounded write request; также укладывается в limit 256 bytes. |
| File read | До 128 bytes | Один bounded `MYOS_SYS_VFS_READ` request из ring 3. |
| Persistent selection | До 8 persistent records по 32 KiB | `N` сканирует не более 64 VFS entry indices, выбирая только допустимые existing GUI `disk/` paths. |
| Selected path | 40 bytes | GUI selected path остаётся плоским `disk/<name>`; `disk/bin/<name>` зарезервирован для executable workflow. |
| Persistent save | До 128 bytes за editor update | `PERSIST_REMOVE`, `PERSIST_CREATE`, затем bounded `PERSIST_WRITE`; GUI editor intentionally не редактирует binary ELF files. |
| Allocation | Static storage | Нет heap allocations или background operations; selected path, cursor и viewport остаются bounded state. |

Текст переносится в пределах внутренней поверхности NOTES. Символы вне printable ASCII заменяются renderer на `?`; newline начинает следующую logical line. В editor kernel рисует cyan caret по index, переданному ring-3 программой; viewport начинается на границе logical line и удерживает строку caret в окне до 20 строк. Обновление content вызывает redraw, но не запускает layout initialization, поэтому сохраняет текущие visibility, focus и z-order window manager. Draft, который достиг 128 bytes, больше не принимает новые bytes до удаления через `Backspace` или `Delete`.

## Границы архитектуры

`kernel/console/framebuffer.c` владеет primitives отрисовки, статическими window records, z-order, pointer state и копией viewer content. `kernel/drivers/mouse.c` включает PS/2 auxiliary port, собирает bounded three-byte packets на IRQ12, отбрасывает overflow и передаёт relative movement с edge-triggered left click в framebuffer. `user/startgui.c` владеет ring-3 event loop, editor state, чтением VFS, отменой draft и последовательностью persistent save. `kernel/sys/syscall.c` владеет проверкой owner, user-memory copy и framebuffer setter. При завершении или принудительном уничтожении GUI owner dispatcher закрывает GUI session и очищает ownership.

| Граница | Политика |
|---|---|
| Window records | Три статических bounded records. |
| Input | Existing scheduler-safe console input path; PS/2 auxiliary port выдаёт three-byte packets через IRQ12. PS/2 arrows, Home, End и Delete переводятся в internal bounded key bytes; в editor keys принадлежат draft, а не window manager. |
| Rendering | Bounded full redraw после GUI input или content update. |
| Files | Viewer читает любой доступный VFS file; editor изменяет выбранный допустимый persistent `disk/` path. |
| Atomicity | Save удаляет и пересоздаёт file перед записью; power-loss-safe journal пока отсутствует. |
| Mouse hardware | PS/2 relative motion и left-button focus реализованы; higher-level actions, dragging, wheel и multi-button semantics пока отсутствуют. |
| General window API | Не реализован; records остаются внутренними для framebuffer renderer. |

## Проверка milestone

Перед commit выполнены strict build и firmware regressions на QEMU Q35. BIOS и UEFI использовали один `myos.img` как IDE drive, поэтому readback подтверждает сохранение данных на том же AHCI-backed persistent partition, а не только в памяти текущего запуска.

| Проверка | Результат |
|---|---|
| `make all img` | Passed без compiler warnings и build errors. |
| `git diff --check` | Passed. |
| BIOS direct named launch | Passed: console создала `disk/todo` с `alpha`; `startgui disk/todo` показал `DISK:TODO` и content. |
| BIOS cycle | Passed: `N` переключил `DISK:TODO` на `DISK:LOG` и `beta` через existing VFS enumeration. |
| BIOS create on save | Passed: `startgui disk/draft`, затем `E`, `x` и `Ctrl-S` создали отсутствующий selected path; viewer показал `DISK:DRAFT` и `X`. |
| BIOS selected save | Passed: `E`, append `x` и `Ctrl-S` сохранили `disk/log`; title остался `DISK:LOG`, viewer показал `BETAX`. |
| BIOS PS/2 mouse | Passed: QEMU relative mouse motion moved crosshair; left-button edge over MONITOR raised that window to foreground. |
| UEFI PS/2 mouse | Passed: OVMF reported IRQ12 enabled; identical QEMU movement and click moved crosshair and focused MONITOR. |
| BIOS reliability lifecycle | Passed: `startgui disk/reliability` created and saved `BIOSOK`; `Q` returned with status `0`; user shell `cat` read the file; the same path was relaunched and exited again. |
| UEFI persistent continuity | Passed: OVMF directly read BIOS-created `BIOSOK`, appended and saved `UEFIOK`, then user-shell `cat` read both lines after GUI exit. |
| Reliability outcome | Passed: no regression observed in GUI owner cleanup, repeatable `startgui`, keyboard input, PS/2 mouse input, return-to-console or AHCI-backed persistence. |
| BIOS persistent ELF | Passed: `install hello disk/bin/hello` stored 10,840 bytes; `run disk/bin/hello` printed its user-space message and exited with status 42. `argshow` also received `alpha beta`. |
| UEFI persistent ELF | Passed: OVMF boot read BIOS-installed `hello` and `args`; both executed with expected output and arguments. |
| Invalid persistent ELF | Passed: `disk/bin/bad` with text content was rejected by the loader without disrupting the user shell. |
| Legacy persistent migration | Passed: a valid MYPFS001 fixture preserved `disk/legacy = legacy-data` during automatic MYPFS002 layout migration; a new `disk/bin/hello` then installed and ran. |
| Existing GUI boundaries | Retained: bounded window state, GUI owner checks, direct viewer launch and return to shell. |

Screenshots и краткие test findings находятся вне source tree в локальных `/home/ubuntu/myos-mouse-validation/`, `/home/ubuntu/myos-reliability-validation/` и `/home/ubuntu/myos-disk-elf-validation/`; они не входят в Git commit.

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

GUI preview boundary зафиксирована immutable tag `v0.12.2-gui-preview`. Текущая ветка `gui/bringup` продолжает persistent user-program platform: disk ELF execution реализован; следующим priority является MyOS SDK для внешней сборки.
