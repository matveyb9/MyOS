# Дорожная карта MyOS

> **Язык:** [English](ROADMAP.md) | [Русский](ROADMAP_RU.md)


> **Статус на 19 августа 2026 года.** MyOS — собственная учебно-практическая ОС для `x86_64`, написанная на freestanding C11 и x86_64 NASM. Проект не основан на Linux или BSD; Limine используется только как текущий загрузчик и поставщик boot environment. Стабильная консольная линия завершена, а новая GUI-функциональность изолирована в отдельной ветке.

## Обозначения статуса

| Маркер | Значение |
|---|---|
| `[x]` | Завершено, включено в соответствующий milestone и проверено в объёме этого этапа. |
| `[~]` | Реализовано в development branch, но этап ещё не имеет отдельного стабильного release или требует запланированного закрытия проверок. |
| `[ ]` | Запланировано; работа ещё не начата. |
| `[R&D]` | Исследовательское направление. Оно не блокирует ближайшие milestones и начнётся только при отдельном решении. |

## Текущее состояние проекта

| Линия | Назначение | Состояние |
|---|---|---|
| `console-stable` | Неподвижная опорная линия завершённой консольной ОС. | `[x]` `v0.12.1-console` на commit `b6914d4`. |
| `main` | Основная линия консольной ОС и её поддерживаемой документации. | `[x]` boot UX refinement в `0dbcc25`: stage headers, three-second auto-init и очистка экрана перед user shell. |
| `gui/bringup` | Изолированная разработка framebuffer GUI и user-program platform. | `[~]` Preview checkpoint `v0.12.2-gui-preview` и GitHub Pre-release `v0.13.0-gui-rc.1` опубликованы; MYPFS004, persistent ELF execution, внешний SDK VFS subset с live `cp` и bounded native argument/input/time toolchain реализованы без слияния в `main`. |

Версия разработки — **MyOS 0.12.2-dev**. Теги `v0.12.0-console` и `v0.12.1-console` являются историческими неизменяемыми границами и не перемещаются.

## 1. Базовая платформа и ядро

| Статус | Результат | Содержание |
|---|---|---|
| `[x]` | Загрузка на x86_64 | Higher-half kernel с Limine 12.5.2, BIOS и UEFI/OVMF paths, ISO и raw `IMG` artifacts. |
| `[x]` | Архитектурная основа | GDT, IDT, TSS, exception/IRQ handling, SYSCALL/SYSRET boundary. |
| `[x]` | Управление памятью | PMM, four-level paging, kernel heap, user address spaces и guard pages. |
| `[x]` | Вытесняемое выполнение | Round-robin scheduler, PIT 100 Hz, Local APIC virtual-wire и до 16 task slots. |
| `[x]` | Базовые драйверы | PS/2 keyboard, RTC, PIC, PCI, AHCI и ACPI S5 poweroff. |
| `[x]` | Хранение данных | Initramfs CPIO, tmpfs overlay, persistent storage и GPT disk image с изолированным data partition. |

## 2. Консольная ОС — завершённый baseline

Консольный этап завершён и закреплён выпуском **`v0.12.1-console`**. Его последующие поддерживающие улучшения в `main` не меняют границу стабильного console release без отдельного решения о новом patch release.

| Статус | Результат | Пользовательская возможность |
|---|---|---|
| `[x]` | Framebuffer text console и COM1 mirror | Диагностика доступна на физическом экране и через serial output QEMU. |
| `[x]` | Kernel diagnostic shell | Prompt `kernel>`, диагностические команды и безопасный вход в user space. |
| `[x]` | User shell `/init` | Prompt `[myos]$`, history, Up/Down, Tab completion, переменные окружения, arguments и pipes. |
| `[x]` | User utilities | `hello`, `sleeper`, `orphaner`, `safety`, `argshow`, `calc`, `pipewrite`, `piperead`, `wc`, `grep`, `edit`. |
| `[x]` | Direct calculator | Signed 64-bit arithmetic и quiet output без lifecycle messages. |
| `[x]` | Автоматический запуск | `/init` запускается через три секунды; `K` отменяет запуск и оставляет пользователя в `kernel>`. |
| `[x]` | Читаемый boot presentation | Boot log разбит на четыре stage headers; normal user-shell handoff очищает framebuffer, diagnostic path сохраняет log. |
| `[x]` | Актуальная эксплуатационная документация | Root README, user/developer/platform guides, release policy и документация Linux, Windows, macOS. |

## 3. GUI bringup — текущий этап

GUI преднамеренно остаётся в **`gui/bringup`**. Он запускается только явной командой `startgui` из user shell; эта модель сохраняет консоль usable как baseline и даёт возможность проверять GUI независимо.

| Статус | Подэтап | Реализованный или ожидаемый результат |
|---|---|---|
| `[x]` | GUI launcher | `startgui` открывает framebuffer desktop из ring 3 и возвращает в text console после выхода. |
| `[x]` | Desktop и window manager | Тёмный desktop, три bounded windows (`SYSTEM`, `NOTES`, `MONITOR`), visibility, z-order и focus. |
| `[x]` | Keyboard interaction | Перемещение, focus switching, show/hide/reset и безопасный выход из GUI session. |
| `[x]` | Software pointer | Bounded crosshair pointer и focus верхнего окна под ним. |
| `[x]` | VFS viewer | Просмотр `/system/core/resources/motd.txt` или файла, переданного как `startgui [absolute-path]`, с ограничением контента ABI. |
| `[x]` | Persistent note editor | Загрузка `/users/myos/files/notes/note`, редактирование, `Ctrl-S` save и `Esc` cancel. |
| `[x]` | Boot UX integration | GUI branch содержит stage headers и clear перед normal user-shell entry; BIOS regression и `startgui` regression пройдены. |
| `[x]` | Cross-firmware closure | UEFI/OVMF normal boot подтвердил stage headers и чистый framebuffer перед user shell. |

## 4. Ближайшие GUI приоритеты

Работа выполняется последовательно, с документацией и проверкой артефактов в каждом user-visible change. Порядок ниже выбран так, чтобы сначала улучшить основной сценарий работы с текстом, затем расширить хранение и только после этого подключать новое hardware input.

| Приоритет | Статус | Работа | Критерий завершения |
|---:|---|---|---|
| 1 | `[x]` | Cursor-aware editor with scrolling | Caret, `Left`/`Right`/`Up`/`Down`, `Home`/`End`, `Delete` и bounded 20-line viewport реализованы; BIOS и UEFI smoke tests пройдены. |
| 2 | `[x]` | Historical pre-MYPFS003 named `disk/` files | Это завершённая historical GUI validation stage: `startgui disk/name` выбирал конкретный legacy path, `N` перебирал files, а editor сохранял selected file. Current workflow использует absolute paths под `/users/myos/files/notes/`. |
| 3 | `[x]` | Hardware mouse/pointer support | PS/2 IRQ12 packets перемещают pointer, left click фокусирует topmost window, а keyboard controls остаются fallback; BIOS и UEFI tests пройдены. |
| 4 | `[x]` | GUI reliability pass | BIOS create/save/return/relaunch, UEFI readback/append/save/return и cross-firmware AHCI persistence прошли без регрессии `startgui`. |
| 5 | `[x]` | Решение о GUI release boundary | Принято: immutable `v0.12.2-gui-preview` фиксирует tested GUI scope; `main` и `console-stable` не меняются, а `gui/bringup` продолжает следующий этап. |
| 6 | `[x]` | Pointer refresh hardening | Ordinary PS/2 and WASD fallback movement больше не repaint полный desktop: kernel restores the 11×11 pointer underlay and draws cursor at the new location. BIOS framebuffer captures, GUI note save, native program execution и UEFI remount checks пройдены. |
| 7 | `[ ]` | Первый GUI release-stabilization pass | `make smoke` автоматизирует clean raw-image BIOS/UEFI boot markers, `make regression` на disposable image проверяет BIOS GUI note save и native build/install/run, затем UEFI persistence/readback and GUI exit, а `make release-check` cleanly rebuilds artifacts and records source/artifact SHA-256. Остаётся выполнить physical x86_64 PC smoke test, зафиксировать final release scope и release notes, затем отдельно решить вопрос нового GUI tag и переноса tested commit в `main`. |

## 5. Ближайший пост-GUI этап: собственные программы и среда разработки

GUI release decision принят: immutable preview tag фиксирует проверенный framebuffer scope, но не объявляет GUI production-ready и не меняет stable console baseline. После checkpoint реализованы persistent ELF execution, MyOS SDK, MYPFS004, restricted native `asm`/`build` workflow, pointer-refresh hardening, `make smoke`, isolated `make regression` и clean-tree `make release-check`. Следующий merge-oriented приоритет — завершить GUI release-stabilization pass: остаются physical x86_64 PC smoke test, final release scope и explicit decision о новом tag. GUI не требуется сливать в `main`, чтобы продолжать development environment, но stable merge не должен предшествовать этой проверке.

| Приоритет | Статус | Работа | Критерий завершения |
|---:|---|---|---|
| 1 | `[x]` | Persistent ELF64 program execution | `install <absolute-source> /apps/<name>/main.elf` копирует bounded ELF в global application package; `run <name> [arguments]` создаёт отдельный user task. Loader проверяет x86_64 ELF64 `ET_EXEC`, load segments и entry. |
| 2 | `[x]` | MyOS SDK для внешней сборки | Public header, startup code, linker script, build template и example app находятся в `sdk/`; host-built ELF устанавливается в `/apps/<name>/main.elf` и запускается без пересборки kernel. Подробности и validation — в [SDK_RU.md](SDK_RU.md). |
| 3 | `[x]` | Developer filesystem workflow | Реализован MYPFS003: real directories, lower-case unified root, `/system/core`, `/system/data`, `/system/config`, `/apps`, `/users/myos`, `/temp` и read-only `/system/live`. Поддержаны absolute paths, ASCII case-preserving/case-insensitive lookup, `/apps` packages, shell `ls`/`mkdir`/`touch`/`write`/`rm`, MYPFS001/MYPFS002 migration и legacy disk namespace removal. [FILESYSTEM_SPEC_RU.md](FILESYSTEM_SPEC_RU.md) фиксирует contract. |
| 4 | `[x]` | MYPFS004 dynamic large-file storage | Regular files растут лениво до 8 MiB, используют до шести extents и 64 KiB allocation batches; AHCI command DMA frames освобождаются на всех exit paths. Пройдены fragmented 1 MiB exact readback, fresh-boot streamed read, MYPFS003 `M4MG` migration, MYPFS002 migration и BIOS/UEFI SDK execution. [MYPFS004_STORAGE_RU.md](MYPFS004_STORAGE_RU.md) фиксирует contract. |
| 5 | `[x]` | Первая нативная сборка в MyOS | Реализованы `asm` и public shell wrapper `build`: bounded `.mya` source из `/users/myos/projects/` превращается в loader-valid x86_64 ELF64, затем `install` packages it as `/apps/<name>/main.elf`. BIOS build/run, fresh remount и UEFI execution прошли. [NATIVE_BUILD_RU.md](NATIVE_BUILD_RU.md) фиксирует syntax и bounds. |
| 6 | `[x]` | Labels и forward-only jumps | `.mya` поддерживает `label name:` и `jump name`; identifiers bounded, labels уникальны, а target обязан располагаться строго позже перехода. BIOS package execution вывела только code до jump с authored status `23`; backward target отклонён, а BIOS-created package повторно выполнен в UEFI. [NATIVE_BUILD_RU.md](NATIVE_BUILD_RU.md) фиксирует syntax, limits и diagnostics. |
| 7 | `[x]` | Bounded conditional control flow | `.mya` теперь поддерживает `set <0..255>`, `jump_if_zero name` и `jump_if_nonzero name` вместе с labels и безусловными jumps. Conditional и ordinary targets остаются строго forward; missing condition и backward targets отклоняются. BIOS true/false paths, rejected cases и UEFI persistence покрыты [NATIVE_BUILD_RU.md](NATIVE_BUILD_RU.md) и `make regression`. |
| 8 | `[x]` | Общий in-OS текстовый редактор | Direct `edit <absolute-file>` предоставляет multi-line cursor editing для ordinary mutable VFS files и `.mya` source. Он имеет document limit 4 KiB, explicit save/discard controls и bounded VFS I/O. BIOS ordinary-text readback, editor-authored program build/run и UEFI persistence покрыты [TEXT_EDITOR_RU.md](TEXT_EDITOR_RU.md) и `make regression`. |
| 9 | `[x]` | Bounded native arguments, input, RTC time и exact comparison | `.mya` поддерживает `args`, `input`, `time` и `jump_if <0..255> name`. `args` передаёт existing bounded string из `run <name> [arguments]`; `input` отфильтровывает terminal `CR`/`LF` и передаёт один condition byte; `time` выводит RTC `HH:MM:SS`; все targets остаются строго forward. Generated ELF использует fixed private RW data segment размером 32 bytes для entry pointer и input/time scratch. `make regression` проверяет empty и forwarded BIOS arguments, input paths и valid time output, затем persistent UEFI argument/input/time execution. [NATIVE_BUILD_RU.md](NATIVE_BUILD_RU.md) фиксирует contract. |
| 10 | `[x]` | SDK VFS subset и practical copy tool | Public SDK добавляет fixed-size VFS read/create-file/write/remove wrappers. SDK-built live app `cp` копирует editor-authored persistent file размером 305 bytes через 256-byte request boundary, отклоняет existing destination и сохраняет exact target data через UEFI. [SDK_RU.md](SDK_RU.md) фиксирует ABI и user contract. |
| 11 | `[x]` | Bounded mouse-first desktop launcher | Bare `startgui` показывает fixed clickable tiles `SYSTEM`, `NOTES`, `EDIT NOTE` и top-bar exit `X`; `startgui home` — compatibility alias. `M`/`N`/`E`/`H`/`Q` остаются hotkey fallback. Launcher использует existing single GUI session и bounded viewer/editor state. `make regression` проверяет default M/H/N/H/Q navigation, QMP PS/2 click activation `NOTES` с PPM framebuffer transition в BIOS и UEFI, а также retained BIOS alias. [GUI_BRINGUP_RU.md](GUI_BRINGUP_RU.md) фиксирует contract. |

> **Приоритет пользователя:** собственные программы и первый native build workflow не откладываются до сети, SMP, USB или собственного bootloader. После GUI release decision они образуют ближайшую линию функциональной разработки.

## 6. Последующий системный горизонт

| Статус | Направление | Правило принятия решения |
|---|---|---|
| `[ ]` | Дополнительные user applications | Развивать поверх SDK и executable workflow, начиная с practical developer tools. |
| `[ ]` | Поддержка физического hardware | Проверять на реальной x86_64 машине после сохранения QEMU BIOS/UEFI regression baseline. |
| `[ ]` | Пользователи и права доступа | Вводить после базового user-program workflow: uid/gid, owners, file permissions, login/session model. |
| `[ ]` | Сеть | Начать с QEMU-supported Ethernet driver и минимального IPv4 path после согласования user-program execution и storage contracts. |
| `[R&D]` | Multiboot compatibility | Исследовать как дополнительный boot protocol, если появится конкретная задача совместимости; текущий Limine path не заменять без проверки всех boot artifacts. |
| `[R&D]` | Собственный bootloader | Начать с изолированного учебного proof of concept; не заменять Limine, пока custom path не достигнет BIOS/UEFI feature parity и повторяемой validation. |
| `[ ]` | SMP, IOAPIC и расширенный timer model | Планировать при появлении задач, которые действительно требуют параллельного CPU execution. |

## 7. Границы, которые не меняются

| Решение | Статус | Причина |
|---|---|---|
| Основная архитектура только `x86_64` | `[x]` | Не добавлять 32-bit port в текущий roadmap: он дублирует low-level platform work и замедлит первый GUI release. Возможный i386 learning lab допускается только позже как отдельная ветка. |
| Limine остаётся загрузчиком текущих artifacts | `[x]` | Он обеспечивает проверенный BIOS/UEFI путь; custom bootloader и Multiboot — отдельные будущие исследования. |
| GUI не сливается в console baseline автоматически | `[x]` | `gui/bringup` остаётся отдельной экспериментальной веткой до отдельного решения о release. |
| Учебные комментарии и учебная документация — после разработки | `[x]` | Полный pedagogical pass начнётся только после завершения функциональной разработки, чтобы не превращать незавершённые детали в ложную спецификацию. |

## 8. Условие перехода к учебной редакции

После функционального завершения выбранной release scope потребуется отдельный финальный этап: объясняющие комментарии в исходниках, последовательные учебные главы, diagrams архитектуры, reproducible lab exercises и обновлённая validation guide. Этот этап намеренно не выполняется параллельно с активной разработкой.

## Следующее действие

Native build workflow, labels/forward-only jumps, bounded conditional control flow, SDK VFS copy tooling, native input/time, bounded default desktop launcher `startgui`, общий text editor, GUI pointer-refresh hardening и automated release-stabilization baseline завершены в `gui/bringup`: MYPFS004 VFS предоставляет единый корень с `/system`, `/apps`, `/users/myos` и `/temp`, а `build` собирает bounded `.mya` source в native ELF64. `args` передаёт existing bounded string из `run <name> [arguments]`, не добавляя variables или general writable memory. `input` принимает один не-`CR`/`LF` byte как condition; `set <0..255>` предоставляет явную альтернативу; `jump_if_zero`, `jump_if_nonzero` и `jump_if <0..255>` сохраняют все targets строго forward. `time` выводит одну RTC line в формате `HH:MM:SS`. Fixed private RW ELF segment размером 32 bytes сохраняет entry argument pointer и syscall scratch storage, но не создаёт general writable program data. SDK header также предоставляет fixed-size VFS read/create/write/remove wrappers; его live tool `cp` копирует files 256-byte chunks, требует new destination с existing parent и никогда не перезаписывает её. Bare `startgui` — fixed mouse-first launcher: его три tile rectangles и top-bar exit rectangle вызывают existing bounded actions, а `M`/`N`/`E`/`H`/`Q` остаются hotkey fallback. `startgui home` остаётся alias, а не general window API или application installer. Direct `edit <absolute-file>` предоставляет cursor-based multi-line editing для ordinary files и `.mya` source с explicit save/discard и all-in-memory document limit 4 KiB. Disposable-image gate `make regression` покрывает BIOS default navigation `startgui` через M/H/N/H/Q и clean return, QMP PS/2 click activation centered tile `NOTES` с PPM framebuffer transition, плюс retained alias `startgui home`, ordinary-text readback, paced 305-byte SDK `cp` copy через VFS request boundary с overwrite rejection, editor-authored program build/run, empty и forwarded arguments, legacy и exact input branches, valid RTC time output, invalid-control-flow rejection и повтор desktop-home navigation в UEFI плюс persistence copied file и installed native packages. `install` явно переносит output в `/apps/<name>/main.elf`, после чего `run <name>` создаёт отдельный ring-3 task. `make smoke` подтверждает raw-image BIOS/UEFI boot markers, а `make release-check` создаёт clean-rebuild source/artifact evidence. Следующий merge-oriented GUI milestone по-прежнему требует physical x86_64 PC smoke test, final release scope и explicit decision о новом immutable GUI tag. Будущая native work должна сохранять bounded language и execution contract; личная установка приложений (`/users/myos/apps`) остаётся отдельным будущим расширением. Preview `v0.12.2-gui-preview` и Pre-release `v0.13.0-gui-rc.1` не сливаются автоматически в `main`; `main` и `console-stable` сохраняют console-only scope. Исходные `myos.iso` и `myos.img` продолжают собираться командой `make all img`.
