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
| `gui/bringup` | Изолированная разработка framebuffer GUI. | `[~]` содержит GUI bringup и merge `33294d1` с текущим `main`. |

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
| `[~]` | Cross-firmware closure | Повторная UEFI/OVMF проверка текущего boot-presentation merge должна закрыть этот текущий change set. |

## 4. Ближайшие GUI приоритеты

Работа выполняется последовательно, с документацией и проверкой артефактов в каждом user-visible change. Порядок ниже выбран так, чтобы сначала улучшить основной сценарий работы с текстом, затем расширить хранение и только после этого подключать новое hardware input.

| Приоритет | Статус | Работа | Критерий завершения |
|---:|---|---|---|
| 1 | `[ ]` | Cursor-aware editor with scrolling | Пользователь видит caret, может перемещаться в тексте, редактировать длинный content и пользоваться bounded scrolling без выхода за ABI limits. |
| 2 | `[ ]` | Named persistent `disk/` files | GUI может выбирать и редактировать несколько именованных persistent files, а не только `disk/note`. |
| 3 | `[ ]` | Hardware mouse/pointer support | Реальный PS/2 mouse input управляет pointer; keyboard controls остаются рабочим fallback. |
| 4 | `[ ]` | GUI reliability pass | BIOS и UEFI regression matrix, проверка возврата в console, persistent data и отсутствие регрессии `startgui`. |
| 5 | `[ ]` | Решение о GUI release boundary | Отдельно оценить readiness GUI и только тогда решить, объединять ли GUI с `main` или выпускать отдельную experimental/stable ветку. |

## 5. Следующий системный горизонт

Эти пункты не должны отвлекать от ближайшего GUI milestone. Они станут планируемыми работами только после решения о GUI release и уточнения требований.

| Статус | Направление | Правило принятия решения |
|---|---|---|
| `[ ]` | Укрепление VFS и persistent storage | Расширять после GUI file workflows, сохраняя ясные limits, validation и data safety. |
| `[ ]` | Дополнительные user applications | Добавлять только после определения практических GUI/use-case потребностей. |
| `[ ]` | Поддержка физического hardware | Проверять на реальной x86_64 машине после сохранения QEMU BIOS/UEFI regression baseline. |
| `[R&D]` | Multiboot compatibility | Исследовать как дополнительный boot protocol, если появится конкретная задача совместимости; текущий Limine path не заменять без проверки всех boot artifacts. |
| `[R&D]` | Собственный bootloader | Начать с изолированного учебного proof of concept; не заменять Limine, пока custom path не достигнет BIOS/UEFI feature parity и повторяемой validation. |
| `[ ]` | SMP, IOAPIC и расширенный timer model | Планировать при появлении задач, которые действительно требуют параллельного CPU execution. |

## 6. Границы, которые не меняются

| Решение | Статус | Причина |
|---|---|---|
| Основная архитектура только `x86_64` | `[x]` | Не добавлять 32-bit port в текущий roadmap: он дублирует low-level platform work и замедлит первый GUI release. Возможный i386 learning lab допускается только позже как отдельная ветка. |
| Limine остаётся загрузчиком текущих artifacts | `[x]` | Он обеспечивает проверенный BIOS/UEFI путь; custom bootloader и Multiboot — отдельные будущие исследования. |
| GUI не сливается в console baseline автоматически | `[x]` | `gui/bringup` остаётся отдельной экспериментальной веткой до отдельного решения о release. |
| Учебные комментарии и учебная документация — после разработки | `[x]` | Полный pedagogical pass начнётся только после завершения функциональной разработки, чтобы не превращать незавершённые детали в ложную спецификацию. |

## 7. Условие перехода к учебной редакции

После функционального завершения выбранной release scope потребуется отдельный финальный этап: объясняющие комментарии в исходниках, последовательные учебные главы, diagrams архитектуры, reproducible lab exercises и обновлённая validation guide. Этот этап намеренно не выполняется параллельно с активной разработкой.

## Следующее действие

Ближайшее практическое действие — закрыть **UEFI/OVMF regression** после последнего boot-presentation merge, затем перейти к **cursor-aware GUI editor with scrolling** в `gui/bringup`. Исходные `myos.iso` и `myos.img` продолжают собираться командой `make all img`.
