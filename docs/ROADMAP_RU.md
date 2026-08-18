# Дорожная карта MyOS

> **Статус на 18 августа 2026 года.** MyOS — собственная учебно-практическая ОС для `x86_64`, написанная на freestanding C11 и x86_64 NASM. Проект не основан на Linux или BSD; Limine используется только как текущий загрузчик и поставщик boot environment. Стабильная консольная линия завершена, а новая GUI-функциональность изолирована в отдельной ветке.

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
| `gui/bringup` | Изолированная разработка framebuffer GUI и user-program platform. | `[~]` GUI scope закрыт preview checkpoint `v0.12.2-gui-preview`; persistent disk ELF execution реализован, ветка остаётся отдельной от `main` для SDK этапа. |

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
| `[x]` | VFS viewer | Просмотр `motd.txt` или файла, переданного как `startgui [file]`, с ограничением контента ABI. |
| `[x]` | Persistent note editor | Загрузка `disk/note`, редактирование, `Ctrl-S` save и `Esc` cancel. |
| `[x]` | Boot UX integration | GUI branch содержит stage headers и clear перед normal user-shell entry; BIOS regression и `startgui` regression пройдены. |
| `[x]` | Cross-firmware closure | UEFI/OVMF normal boot подтвердил stage headers и чистый framebuffer перед user shell. |

## 4. Ближайшие GUI приоритеты

Работа выполняется последовательно, с документацией и проверкой артефактов в каждом user-visible change. Порядок ниже выбран так, чтобы сначала улучшить основной сценарий работы с текстом, затем расширить хранение и только после этого подключать новое hardware input.

| Приоритет | Статус | Работа | Критерий завершения |
|---:|---|---|---|
| 1 | `[x]` | Cursor-aware editor with scrolling | Caret, `Left`/`Right`/`Up`/`Down`, `Home`/`End`, `Delete` и bounded 20-line viewport реализованы; BIOS и UEFI smoke tests пройдены. |
| 2 | `[x]` | Named persistent `disk/` files | `startgui disk/name` выбирает конкретный path, `N` циклически перебирает existing files, а editor сохраняет выбранный file; BIOS и UEFI readback пройдены. |
| 3 | `[x]` | Hardware mouse/pointer support | PS/2 IRQ12 packets перемещают pointer, left click фокусирует topmost window, а keyboard controls остаются fallback; BIOS и UEFI tests пройдены. |
| 4 | `[x]` | GUI reliability pass | BIOS create/save/return/relaunch, UEFI readback/append/save/return и cross-firmware AHCI persistence прошли без регрессии `startgui`. |
| 5 | `[x]` | Решение о GUI release boundary | Принято: immutable `v0.12.2-gui-preview` фиксирует tested GUI scope; `main` и `console-stable` не меняются, а `gui/bringup` продолжает следующий этап. |

## 5. Ближайший пост-GUI этап: собственные программы и среда разработки

GUI release decision принят: immutable preview tag фиксирует проверенный framebuffer scope, но не объявляет GUI production-ready и не меняет stable console baseline. Эта линия теперь становится ближайшим функциональным приоритетом, а не дальним исследованием. Цель — быстро перейти от встроенных программ initramfs к безопасному запуску собственных программ пользователя, затем дать практичный workflow для их создания и первой нативной компиляции в MyOS. GUI не требуется сливать в `main`, чтобы начать эту работу: preview checkpoint определяет scope, а `gui/bringup` продолжает development environment без неопределённой паузы.

| Приоритет | Статус | Работа | Критерий завершения |
|---:|---|---|---|
| 1 | `[x]` | Persistent ELF64 program execution | `install <source> <disk/bin/name>` копирует bounded ELF в persistent slot; `run disk/bin/name [arguments]` создаёт отдельный user task. Loader проверяет x86_64 ELF64 `ET_EXEC`, load segments и entry; BIOS/UEFI persistence regressions пройдены. |
| 2 | `[ ]` | MyOS SDK для внешней сборки | Репозиторий содержит public syscall headers, startup code, linker script, build template и example app; пользователь собирает программу на ПК и запускает её в MyOS без пересборки kernel. |
| 3 | `[ ]` | Developer filesystem workflow | `disk/src/` и `disk/bin/`, увеличенные безопасные limits persistent storage, console editor и shell-команды позволяют хранить, редактировать, копировать и запускать исходники/бинарники прямо в MyOS. |
| 4 | `[ ]` | Первая нативная сборка в MyOS | В MyOS появляется компактный native assembler или ограниченный C compiler с командой build, создающей запускаемый MyOS ELF64 для учебных и практических user programs. |
| 5 | `[ ]` | Расширение native toolchain | По мере готовности: более полное подмножество C, linker, базовая C-библиотека, build scripts и затем оценка портирования более крупного compiler. GCC/Clang не являются первым шагом. |

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

Ближайшее практическое действие — начать priority 2: **MyOS SDK для внешней сборки**. Он предоставит public headers, startup code, linker script, build template и example app, который пользователь сможет собрать на ПК, установить в `disk/bin/` и запустить без пересборки kernel. Preview `v0.12.2-gui-preview` не сливается в `main`; `main` и `console-stable` сохраняют console-only scope. Исходные `myos.iso` и `myos.img` продолжают собираться командой `make all img`.
