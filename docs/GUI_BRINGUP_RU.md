# GUI bring-up: native framebuffer desktop

> **Язык:** [English](GUI_BRINGUP.md) | [Русский](GUI_BRINGUP_RU.md)


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

После kernel bootstrap MyOS автоматически запускает user shell через три секунды. Нажмите `K` во время countdown, если нужна diagnostic kernel shell; в этом случае `init` сохраняется как ручной запуск user shell. Затем запустите рабочий стол MyOS. Без аргумента `startgui` открывает bounded launcher **MYOS DESKTOP**; необязательный absolute-file аргумент открывает этот файл в viewer.

```text
startgui
# Compatibility alias: startgui home
# File viewer: startgui /users/myos/files/notes/note
```

`startgui` является обычной ring-3 программой. Без аргументов она открывает bounded home view **MYOS DESKTOP**; `startgui home` остаётся compatibility alias. Его keyboard launcher предоставляет `M` для system message, `N` для notes viewer, `E` для selected note editor, `H` для возврата home и `Q` для возврата в shell. Она создаёт ограниченную GUI session через существующую syscall boundary, читает и редактирует только bounded user-space payload, а kernel получает лишь проверенные syscall requests. `startgui /users/myos/files/notes/<name>` выбирает persistent note ещё до его создания: viewer сообщит об отсутствии, а `E` откроет пустой draft, который `Ctrl-S` создаст. `Q` или `Esc` за пределами editor завершает graphical session и возвращает в тот же user shell.

## Persistent user programs

GUI branch теперь может запускать отдельные MyOS ELF64 из global persistent application packages `/apps/<name>/main.elf`. Сначала встроенная программа initramfs копируется в package, затем она запускается коротким shell name или абсолютным path как отдельный ring-3 process.

```text
install /system/core/apps/hello.elf /apps/hello/main.elf
run hello

install /system/core/apps/argshow.elf /apps/args/main.elf
run args alpha beta
```

| Граница | Правило |
|---|---|
| Install source | Existing absolute VFS file до 8 MiB. |
| Target | Только `/apps/<name>/main.elf`; `install` создаёт package directory. |
| Loader | Принимает только little-endian x86_64 ELF64 `ET_EXEC` с valid load segments и entry внутри mapped load segment. |
| Storage | MYPFS004 предоставляет до 128 persistent file/directory objects, regular files до 8 MiB и до шести extents на file; `install` копирует по 256-byte VFS chunks. |
| Failure | Invalid content, oversized source, invalid path или невозможный load безопасно отклоняются; shell остаётся usable. |

## MyOS SDK для внешней сборки

В `sdk/` теперь находится компактный public SDK для freestanding C11 user programs. Он содержит `include/myos.h`, startup object, linker script, GNU Make template и проверочный source `sdk/examples/hello.c`. Программа определяет `myos_main(uint64_t argc, const char *arguments)` вместо обычного `main`; startup object вызывает её и передаёт return code в `MYOS_SYS_EXIT`. В текущем ABI `argc` всегда равен `1`, а `arguments` — одна NUL-terminated строка после program path.

```bash
make -C sdk APP=sdk/examples/hello.c OUT=sdk/build/sdk-hello.elf
make img
```

Image build добавляет этот reference ELF в initramfs как `/system/core/examples/sdk/hello.elf`. Поэтому complete regression не нуждается в ручной модификации disk image:

```text
install /system/core/examples/sdk/hello.elf /apps/sdk-hello/main.elf
run sdk-hello external SDK validation
```

Проверочный program выводит приветствие и принятую строку аргументов. После fresh BIOS boot сохранённый `/apps/sdk-hello/main.elf` снова запускается командой `run sdk-hello`, что проверяет внешнюю сборку, loader и AHCI-backed persistent storage как единый путь. Подробный публичный contract, limits и host workflow приведены в [SDK_RU.md](SDK_RU.md).

## Текущее поведение

| Компонент | Реализованное поведение |
|---|---|
| Renderer | Нативное прямое рисование в Limine RGB framebuffer без внешнего GUI runtime. |
| Владение session | Одновременно допустим ровно один GUI owner; kernel отклоняет вторую параллельную session. |
| Desktop | Тёмный desktop, верхняя строка состояния и нижняя строка доступных controls. |
| Windows | Три статические bounded window records: `SYSTEM`, `NOTES` и `MONITOR`. |
| Z-order | Focused window поднимается на передний план; focus, visibility, layout и content events выполняют bounded full redraw композиции, тогда как ordinary pointer movement обновляет только cursor region. |
| Viewer | `NOTES` отображает до 128 bytes выбранного VFS file. |
| File loading | `startgui <absolute-path>` читает первые 128 bytes указанного VFS file. |
| Desktop home | Bare `startgui` рисует fixed launcher `MYOS DESKTOP`; `startgui home` — compatibility alias; он не сканирует arbitrary paths, не создаёт tasks и не хранит unbounded state. |
| Persistent selection | `D` выбирает стандартный `/users/myos/files/notes/note`; `N` циклически выбирает следующую existing note через directory-scoped VFS enumeration. |
| Named launch | `startgui /users/myos/files/notes/<name>` выбирает конкретную personal note; title NOTES показывает basename выбранного file. |
| Editor entry | `E` открывает bounded editor для выбранной personal note; отсутствующий selected path начинает с пустого draft. |
| Editor input | Printable ASCII вставляется в позиции caret; `Enter` вставляет newline; `Backspace` удаляет byte слева, `Delete` — byte под caret. |
| Caret и navigation | `Left`/`Right` перемещают caret на byte, `Up`/`Down` — по logical lines с сохранением column, `Home`/`End` переходят к границам строки. |
| Bounded scrolling | Renderer отображает окно до 20 logical newline-separated lines; viewport автоматически следует за строкой caret. |
| Save и cancel | `Ctrl-S` заменяет выбранный file в `/users/myos/files/notes/`, записывает draft и возвращает к его viewer. `Esc` отменяет draft и перезагружает ранее сохранённое содержимое. |
| Built-in choices | `M` или `m` повторно загружает `/system/core/resources/motd.txt`; `H` возвращает к MYOS DESKTOP, а desktop-home `N` открывает bounded personal-notes route. |
| Focus | `Tab`, `Enter` или `Space` вне editor переводят focus на следующее видимое окно. |
| Hardware pointer | PS/2 mouse relative motion перемещает bounded crosshair pointer; rising edge левой кнопки фокусирует верхнее видимое окно под pointer. |
| Keyboard fallback | Строчные `W`, `A`, `S`, `D` перемещают pointer на 16 pixels; `F` сохраняет keyboard focus верхнего окна под pointer. |
| Visibility | `1`, `2`, `3` переключают `SYSTEM`, `NOTES`, `MONITOR`; `X` скрывает focused window, не позволяя скрыть все окна. |
| Reset и выход | `R` восстанавливает исходный layout и z-order. `Q` или `Esc` вне editor завершает session и возвращает framebuffer text console. |

Буква `D` используется только в верхнем регистре для выбора `/users/myos/files/notes/note`; `N` или `n` циклически выбирает следующую existing note внутри `/users/myos/files/notes/`. Это сохраняет строчную `d` как перемещение pointer вправо. В editor обычные printable keys становятся текстом draft и не передаются window manager, поэтому `D`, `N`, `Q` и другие символы можно вводить как часть заметки.

## Ограничения editor и граница ABI

`NOTES` использует дескриптор `MYOS_GUI_SET_CONTENT = 3` в `MYOS_SYS_GUI_SESSION`. Kernel принимает content request лишь от текущего GUI owner при активной session, копирует request после проверки отображения user buffer и не хранит user pointers. Framebuffer владеет собственными статическими copies title и data. GUI циклически использует directory-scoped `MYOS_SYS_VFS_LIST` для `/users/myos/files/notes/` и держит выбранный absolute path в bounded static storage. Editor удаляет выбранный file, создаёт его через unified VFS и записывает один bounded payload с offset `0`.

| Поле или операция | Ограничение | Назначение |
|---|---:|---|
| `MYOS_GUI_CONTENT_TITLE_MAX` | 16 bytes | NUL-terminated title для NOTES window. |
| `MYOS_GUI_CONTENT_MAX` | 128 bytes | Максимальная длина viewer content и editor draft. |
| `struct myos_gui_content_request` | 176 bytes | `length`, `flags`, `cursor`, `viewport`, `title[16]`, `data[128]`; укладывается в syscall user-copy limit 256 bytes. |
| `struct myos_vfs_write_request` | 384 bytes | Unified bounded write request; укладывается в syscall user-copy limit 512 bytes. |
| File read | До 256 bytes | Один bounded `MYOS_SYS_VFS_READ` request из ring 3; GUI viewer применяет собственный 128-byte content limit. |
| Persistent selection | До 64 scanned directory indices | `N` использует `MYOS_SYS_VFS_LIST` только в notes directory. |
| Selected path | До 111 ASCII bytes плюс NUL | GUI хранит absolute note path в `/users/myos/files/notes/`; `/apps/` предназначен для executable workflow. |
| Persistent save | До 128 bytes за editor update | `VFS_REMOVE`, `VFS_CREATE_FILE`, затем bounded `VFS_WRITE`; GUI editor intentionally не редактирует binary ELF files. |
| Allocation | Static storage | Нет heap allocations или background operations; selected path, cursor и viewport остаются bounded state. |

Текст переносится в пределах внутренней поверхности NOTES. Символы вне printable ASCII заменяются renderer на `?`; newline начинает следующую logical line. В editor kernel рисует cyan caret по index, переданному ring-3 программой; viewport начинается на границе logical line и удерживает строку caret в окне до 20 строк. Обновление content вызывает redraw, но не запускает layout initialization, поэтому сохраняет текущие visibility, focus и z-order window manager. Draft, который достиг 128 bytes, больше не принимает новые bytes до удаления через `Backspace` или `Delete`.

## Границы архитектуры

`kernel/console/framebuffer.c` владеет primitives отрисовки, статическими window records, z-order, pointer state и копией viewer content. `kernel/drivers/mouse.c` включает PS/2 auxiliary port, собирает bounded three-byte packets на IRQ12, отбрасывает overflow и передаёт relative movement с edge-triggered left click в framebuffer. `user/startgui.c` владеет ring-3 event loop, editor state, чтением VFS, отменой draft и последовательностью persistent save. `kernel/sys/syscall.c` владеет проверкой owner, user-memory copy и framebuffer setter. При завершении или принудительном уничтожении GUI owner dispatcher закрывает GUI session и очищает ownership.

| Граница | Политика |
|---|---|
| Window records | Три статических bounded records. |
| Input | Existing scheduler-safe console input path; PS/2 auxiliary port выдаёт three-byte packets через IRQ12. PS/2 arrows, Home, End и Delete переводятся в internal bounded key bytes; в editor keys принадлежат draft, а не window manager. |
| Rendering | Full desktop composition выполняется при content update, focus, visibility или layout change. Обычное pointer movement восстанавливает bounded 11×11 cursor underlay и рисует cursor в новом месте без полного redraw. |
| Files | Viewer читает любой доступный absolute VFS file; editor изменяет выбранную note в `/users/myos/files/notes/`. |
| Atomicity | Save удаляет и пересоздаёт file перед записью; MYPFS004 сохраняет bounded metadata and allocation state, а полная application-level atomic replace пока не реализована. |
| Mouse hardware | PS/2 relative motion и left-button focus реализованы. Обычное движение использует bounded cursor-only refresh; higher-level actions, dragging, wheel и multi-button semantics пока отсутствуют. |
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
| Legacy persistent ELF baseline | Superseded by MYPFS003: disk/bin ELF workflow was migrated into `/apps/<name>/main.elf`; loader validation remains unchanged. |
| Invalid persistent ELF | Passed: text content at an application `main.elf` target is rejected by the loader without disrupting the user shell. |
| Legacy persistent migration | Passed: prior MYPFS001→MYPFS002 migration remains historical; current MYPFS002→MYPFS003 fixture preservation is recorded below. |
| External MyOS SDK host build | Passed: `make -C sdk APP=sdk/examples/hello.c OUT=sdk/build/sdk-hello.elf` produced a static x86_64 `ELF64 ET_EXEC` with valid loadable segments. |
| MYPFS003 root and runtime | Passed (BIOS): `/system`, `/apps`, `/users/myos`, `/temp`, `/system/live/processes` and `cat /system/live/processes/3/info` returned expected virtual state. |
| MYPFS003 user workflow | Passed (BIOS): `mkdir /users/myos/projects/demo`, persistent write/read, mixed-case `/UsErS/MyOs` lookup, and `/apps` package installation all worked. |
| SDK install, arguments and persistence | Passed: `/system/core/examples/sdk/hello.elf` installed as `/apps/sdk-hello/main.elf`, `run sdk-hello external SDK validation` printed its argument string; fresh BIOS boot ran the persisted app again. |
| MYPFS003 → MYPFS004 migration | Passed (BIOS): fixture hierarchy and payload migrated through durable `M4MG` recovery marker; `MYPFS004` superblock и cleared journal confirmed before second clean mount. |
| MYPFS002 legacy migration | Passed (BIOS): `disk/note` fixture migrated to `/users/myos/files/notes/note`; `MYPFS004` superblock, cleared journal and second-mount readback confirmed. |
| MYPFS004 large-file I/O | Passed (BIOS): 1 MiB fragmented two-extent pattern write/readback, fresh-mount `wc` of all 1,048,576 bytes, SDK install/run after reboot и UEFI persisted SDK execution. |
| Pointer refresh hardening | Passed: two 1280×800 BIOS framebuffer captures before/after keyboard pointer movement differed in only 726 PPM byte positions, consistent with old/new 11×11 cursor regions; desktop composition remained intact. |
| Automated `make regression` | Passed: disposable-image harness created/edited/saved a BIOS GUI note, navigated default `startgui` through M/H/N/H/Q и returned cleanly, затем verified retained alias `startgui home`, copied paced editor-authored 305-byte file SDK `cp` через VFS request boundary, verified exact data и overwrite refusal, built/installed/ran legacy native packages, checked empty и forwarded `args` output, exact `input` match and fallback paths plus valid RTC `HH:MM:SS` output, rejected invalid forward-only control flow, затем UEFI повторил desktop-home navigation, прочитал persisted files и copied data, повторно запустил installed input/time/argument packages и корректно entered/exited GUI. See [RELEASE_STABILIZATION_RU.md](RELEASE_STABILIZATION_RU.md). |
| GUI note and native workflow | Passed: BIOS GUI editor changed persistent note `base` → `base!`; the same note and a BIOS-built native program were read/executed under UEFI. |
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

GUI preview boundary зафиксирована immutable tag `v0.12.2-gui-preview`, а GitHub Pre-release `v0.13.0-gui-rc.1` опубликован отдельно. Текущая ветка `gui/bringup` содержит MYPFS004 hierarchy, 8 MiB dynamic large-file storage, `/apps` ELF execution, MyOS SDK для внешней сборки с public bounded VFS wrappers и его live no-overwrite `cp` developer tool, а также restricted in-OS `asm`/`build` workflow с bounded `args` forwarding из `run <name> [arguments]`, labels, явными `set` values, single-byte `input`, RTC `time`, exact `jump_if <0..255>` comparison и forward-only branches. Generated image добавляет только fixed private RW data segment размером 32 bytes: entry argument pointer и input/time scratch storage. Ветка также содержит bounded keyboard-driven desktop launcher, открываемый `startgui` (с `startgui home` как alias), общий console [Текстовый редактор](TEXT_EDITOR_RU.md) и cursor-only GUI pointer refresh. GUI editor остаётся notes-focused feature; direct `edit <absolute-file>` — общий file editor. Каталожная структура была совместно согласована с пользователем и зафиксирована в [FILESYSTEM_SPEC_RU.md](FILESYSTEM_SPEC_RU.md); будущая native-platform work должна сохранять completed bounded execution contract.
