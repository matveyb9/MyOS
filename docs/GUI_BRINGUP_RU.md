# GUI bring-up: native framebuffer desktop

Этот документ описывает **экспериментальный GUI** только для ветки `gui/bringup`. Он не входит в console release `v0.12.0-console` и не изменяет назначение веток `main` или `console-stable`. GUI остаётся нативным x86_64-компонентом MyOS: он рисуется непосредственно в RGB framebuffer, без web runtime, внешнего graphical toolkit или динамической памяти.

## Запуск

Сначала необходимо переключиться на GUI-ветку и собрать raw image. Для проверки доступа к persistent storage образ следует подключать к QEMU как IDE-диск.

```bash
git switch gui/bringup
make all img
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

После kernel prompt нужно запустить user shell, а затем graphical viewer. Без аргумента viewer загружает `motd.txt`; необязательный аргумент задаёт путь к читаемому файлу VFS.

```text
init
startgui
# либо: startgui motd.txt
```

`startgui` является обычной ring-3 программой. Она создаёт ограниченную GUI session через существующую syscall boundary, читает файл VFS в user space и передаёт только bounded payload владельцу GUI session. `Q` или `Esc` завершает graphical session и возвращает в тот же user shell.

## Текущее поведение

| Компонент | Реализованное поведение |
|---|---|
| Renderer | Нативное прямое рисование в Limine RGB framebuffer без внешнего GUI runtime. |
| Владение session | Одновременно допустим ровно один GUI owner; kernel отклоняет вторую параллельную session. |
| Desktop | Тёмный desktop, верхняя строка состояния и нижняя строка доступных controls. |
| Windows | Три статические bounded window records: `SYSTEM`, `NOTES` и `MONITOR`. |
| Z-order | Focused window поднимается на передний план; каждый event вызывает bounded full redraw композиции. |
| Viewer | `NOTES` стал read-only graphical VFS viewer, а не демонстрационной поверхностью. |
| File loading | `startgui [file]` читает первые 128 байт указанного VFS file; без аргумента используется `motd.txt`. |
| Built-in choices | `M` или `m` повторно загружает `motd.txt`; `D` загружает `disk/note`. При отсутствии файла отображается `UNABLE TO READ FILE`. |
| Focus | `Tab`, `Enter` или `Space` переводят focus на следующее видимое окно. |
| Pointer | Строчные `W`, `A`, `S`, `D` перемещают bounded crosshair pointer. `F` фокусирует верхнее видимое окно под pointer. |
| Visibility | `1`, `2`, `3` переключают `SYSTEM`, `NOTES`, `MONITOR`; `X` скрывает focused window, не позволяя скрыть все окна. |
| Reset и выход | `R` восстанавливает исходный layout и z-order. `Q` или `Esc` завершает session и возвращает framebuffer text console. |

Буква `D` используется только в верхнем регистре для выбора persistent file. Это сохраняет строчную `d` как перемещение pointer вправо и исключает конфликт между viewer и window-manager controls.

## Граница ABI и безопасности

Viewer использует дескриптор `MYOS_GUI_SET_CONTENT = 3` в `MYOS_SYS_GUI_SESSION`. Kernel принимает запрос лишь от текущего GUI owner при активной session, копирует user request только после проверки отображения user buffer и проверяет максимальный размер title и payload. Kernel не получает user pointers на хранение: framebuffer владеет собственными статическими copies заголовка и данных.

| Поле ABI | Ограничение | Назначение |
|---|---:|---|
| `MYOS_GUI_CONTENT_TITLE_MAX` | 16 bytes | NUL-terminated title для NOTES window. |
| `MYOS_GUI_CONTENT_MAX` | 128 bytes | Максимальный read-only file payload, отрисовываемый viewer. |
| `struct myos_gui_content_request` | 152 bytes | `length`, `title[16]`, `data[128]`; укладывается в syscall user-copy limit 256 bytes. |
| File read | До 128 bytes | Один bounded `MYOS_SYS_VFS_READ` request из ring 3. |
| Renderer | Static storage | Нет dynamic allocation, scrolling или записи файла. |

Текст переносится в пределах внутренней поверхности NOTES. Символы вне printable ASCII заменяются на `?`; newline начинает следующую строку. Обновление content делает redraw, но не вызывает layout initialization, поэтому сохраняет текущие visibility, focus и z-order window manager.

## Границы архитектуры

`kernel/console/framebuffer.c` владеет primitives отрисовки, статическими window records, z-order, pointer state и копией viewer content. `user/startgui.c` владеет ring-3 event loop, чтением VFS и подготовкой bounded ABI request. `kernel/sys/syscall.c` владеет проверкой owner, user-memory copy и вызовом framebuffer setter. При завершении или принудительном уничтожении GUI owner dispatcher закрывает GUI session и очищает ownership.

| Граница | Политика |
|---|---|
| Window records | Три статических bounded records. |
| Allocation | Dynamic GUI allocation отсутствует. |
| Input | Используется существующий scheduler-safe console input path. |
| Rendering | Bounded full redraw после GUI input или content update. |
| Files | Viewer только читает VFS; создание, изменение и scrolling не реализованы. |
| Mouse hardware | Не реализован; keyboard управляет visual pointer. |
| General window API | Не реализован; records остаются внутренними для framebuffer renderer. |

## Проверка milestone

Перед commit выполнены strict build и firmware regressions на QEMU Q35. В BIOS и UEFI путь использовал `myos.img` как IDE drive, потому что так тестируется тот же AHCI/persistent-storage routing, что нужен `disk/note`.

| Проверка | Результат |
|---|---|
| `make all img` | Passed без compiler warnings и build errors. |
| `git diff --check` | Passed. |
| BIOS Q35 | Passed: `init`, `startgui`, чтение `motd.txt`, screenshot framebuffer. |
| BIOS state preservation | Passed: после скрытия `SYSTEM` команды `D`, затем `M` обновляли NOTES, не возвращая hidden SYSTEM и не сбрасывая z-order. |
| UEFI/OVMF | Passed: GUI запущен, `motd.txt` отрисован, screenshot проверен. |
| Shell argument | Passed в UEFI: `startgui motd.txt` передал путь ring-3 viewer и отрисовал тот же файл. |
| Persistent file absence | Expected: на свежем image `D` показал `UNABLE TO READ FILE`, поскольку `disk/note` ещё не создан. |

Захваченные screenshots для BIOS и UEFI находятся в локальном каталоге `validation/` рабочей копии и не являются частью исходного tree. Следующий логичный GUI milestone — небольшой text input для создания и изменения `disk/` files либо аппаратная поддержка mouse/pointer. Оба направления должны сохранить отдельную GUI branch до отдельного решения о merge или release.
