# Roadmap MyOS

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="ROADMAP.md">🇺🇸 ENGLISH</a>
</p>

> **Статус на 25 августа 2026 года.** MyOS — учебно-практическая ОС для `x86_64`, написанная на freestanding C11 и x86_64 NASM. Она не основана на Linux или BSD. Limine остаётся текущим bootloader и поставщиком boot environment.

## Обозначения статуса

| Маркер | Значение |
|---|---|
| `[x]` | Завершено и проверено в объёме указанного milestone. |
| `[~]` | Активная или интегрированная QEMU-validated работа, ещё не представленная отдельным публичным release. |
| `[ ]` | Запланировано; реализация не начата. |
| `[R&D]` | Исследовательское направление; начинается только отдельным решением. |

## Текущее состояние проекта

| Линия | Назначение | Состояние |
|---|---|---|
| `console-stable` | Единственный immutable stable console baseline. | `[x]` Закреплён `v0.12.1-console`; maintenance требует отдельного stable decision. |
| `main` | Активная экспериментальная QEMU-validated integration line. | `[~]` Содержит GUI, MYPFS004, native execution, SDK workflow и File Workspace. |
| `feature/gui` | Историческая pre-integration GUI line. | `[x]` Сохранена для истории, inactive и не является целью новой разработки. |

`main` сам по себе не задаёт release identifier. `v0.12.0-console`, `v0.12.1-console` и `v0.13.1-gui-preview.1` остаются неизменяемыми историческими checkpoint.

## 1. Базовая платформа и stable console baseline

| Статус | Результат | Scope |
|---|---|---|
| `[x]` | Загрузка x86_64 и hardware foundation | Higher-half kernel; Limine BIOS и UEFI/OVMF paths; ISO и raw GPT image; GDT, IDT, TSS, PMM, paging, user address spaces, scheduler, PS/2, RTC, AHCI, PCI и ACPI S5 poweroff. |
| `[x]` | Persistent logical VFS | CPIO `/system/core`, MYPFS004-backed writable roots, tmpfs `/temp`, generated read-only `/system/live`, GPT data partition и migration legacy storage. |
| `[x]` | Stable console user environment | User shell, completion, history, pipes, direct bounded file tools, text editor, diagnostics и завершённая граница `v0.12.1-console`. |
| `[x]` | QEMU BIOS/UEFI baseline | Raw-image boot smoke, persistent AHCI path и reproducible validation commands. |

Граница stable console остаётся неизменной, пока не будет принято отдельное maintenance decision.

## 2. Интегрированная GUI, VFS и native platform

Framebuffer GUI, VFS workspace и native-program platform интегрированы в **`main`**. `startgui` по-прежнему явно запускается из user shell, сохраняя console interaction model, а `console-stable` остаётся неизменным.

| Статус | Возможность | Реализованный результат |
|---|---|---|
| `[x]` | Desktop и window system | Ring-3 `startgui`, bounded windows `SYSTEM`, `NOTES` и `MONITOR`, pointer, z-order, visible close controls, standard modifier hotkeys, RTC clock и task status. |
| `[x]` | GUI editor и viewer | Writable regular files можно просматривать и редактировать через bounded GUI document ABI 16 KiB; read-only paths не входят в editor. |
| `[x]` | File Workspace | `FILES` просматривает logical VFS через four-entry pages с revalidated type и metadata rows. Writable roots поддерживают new file, new folder, delete confirmation, copy, rename и file-only move; каждый browsable root поддерживает bounded read-only search. |
| `[x]` | MOVE safety в File Workspace | MOVE сохраняет basename, меняет metadata без copy-delete, отклоняет existing target, работает только с files и остаётся в одном persistent move anchor либо hierarchy `/temp`. QEMU workflow проверяет rejection, успешное перемещение и UEFI persistence. |
| `[x]` | Persistent native applications | Проверенная загрузка ELF64, installation `/apps/<name>/main.elf` и `run <name> [arguments]`; discovered app tiles запускают verified packages. |
| `[x]` | SDK и in-OS development | Public SDK, bounded VFS subset, `asm`, `build`, `install`, constrained language `.mya` и source workflow в `/users/myos/projects/`. |
| `[x]` | Runtime inventory | Read-only records `/system/live` и `sysinfo` показывают bounded boot, driver, device и process information без нового storage format. |

Подробные contracts поддерживаются в [GUI_BRINGUP_RU.md](GUI_BRINGUP_RU.md), [FILESYSTEM_SPEC_RU.md](FILESYSTEM_SPEC_RU.md), [SDK_RU.md](SDK_RU.md), [NATIVE_BUILD_RU.md](NATIVE_BUILD_RU.md) и [RELEASE_STABILIZATION_RU.md](RELEASE_STABILIZATION_RU.md).

## 3. Validation, branch и publication policy

Работа ведётся небольшими user-visible milestones. Compact полностью validated change может коммититься прямо в `main`; short isolated branch остаётся подходящим для более рискованной VFS/ABI работы, экспериментов или multipart changes.

| Статус | Правило | Evidence или decision gate |
|---|---|---|
| `[x]` | GUI/VFS integration | Интегрирована в `main` после полного `make release-check`. |
| `[x]` | File Workspace completion | BIOS/UEFI QEMU regression покрывает create, folder, delete confirmation, copy, rename, file-only move, search и MOVE no-overwrite behavior. |
| `[x]` | Обычный development gate | До commit user-visible milestone используется релевантная build или QEMU regression evidence. |
| `[~]` | Будущий Pre-release | Рассматривается только после coherent group meaningful changes и нового `make release-check`; создание требует отдельного explicit confirmation. |
| `[ ]` | Будущий stable release | Требует physical x86_64 PC smoke test в дополнение к QEMU baseline. Это не блокирует QEMU-only development или scoped Pre-release. |

Обычная feature work не создаёт release, tag или history rewrite. Подробный workflow записан в [DEVELOPMENT_WORKFLOW_RU.md](DEVELOPMENT_WORKFLOW_RU.md).

## 4. Текущий development focus

Следующая функциональная работа должна улучшить практический end-to-end path:

```text
/users/myos/projects/  →  build  →  install  →  run
```

| Приоритет | Статус | Направление | Принцип завершения |
|---:|---|---|---|
| 1 | `[~]` | Project и developer workflow | Делать small, visible, bounded improvements, которые упрощают совместное создание source, editing, build, installation и execution. Не добавлять новый VFS primitive, если он не требуется workflow. |
| 2 | `[ ]` | Follow-on user-facing tools | Выбирать следующую utility или GUI step только если она прямо усиливает established project workflow. |
| 3 | `[ ]` | Coherent Pre-release review | Возвращаться к Pre-release после нескольких related milestones, а не после каждого commit. |
| 4 | `[ ]` | Physical-PC validation | При появлении hardware выполнить disposable-media x86_64 smoke test, затем отдельно решить, уместна ли stable-release work. |

> **Правило приоритета:** улучшения project и developer workflow не откладываются до networking, SMP, USB или custom bootloader.

## 5. Последующий системный горизонт

| Статус | Направление | Правило принятия решения |
|---|---|---|
| `[ ]` | Users и access control | Вводить uid/gid, ownership, permissions и login/session concepts только после зрелого basic user-program workflow. |
| `[ ]` | Networking | Начать с QEMU-supported Ethernet driver и minimal IPv4 path после согласования execution и storage contracts. |
| `[ ]` | SMP, IOAPIC и extended timer model | Планировать только при появлении concrete workloads, которым нужно parallel CPU execution. |
| `[R&D]` | Multiboot compatibility | Исследовать только для конкретной compatibility need; не заменять текущий Limine path без проверки каждого artifact. |
| `[R&D]` | Custom bootloader | Начать как isolated educational proof of concept; не заменять Limine до BIOS/UEFI parity и repeatable validation. |

## 6. Границы, которые не меняются

| Решение | Статус | Причина |
|---|---|---|
| Основная архитектура — только x86_64 | `[x]` | 32-bit port дублирует low-level platform work и находится вне выбранного scope. |
| Limine остаётся текущим loader | `[x]` | Он предоставляет validated BIOS/UEFI boot path. |
| `console-stable` и `main` имеют разные роли | `[x]` | `console-stable` — единственный stable console baseline; `main` — активная QEMU-validated integration line. |
| `feature/gui` историческая | `[x]` | Ветка сохранена для истории и больше не развивается. |
| Pedagogical edition следует после functional completion | `[x]` | Explanatory comments, chapters, diagrams и labs — отдельный этап, чтобы unfinished behavior не стал ложной спецификацией. |

## 7. Следующее действие

Выбрать один узкий project/developer-workflow milestone из текущего baseline `main`, реализовать его с bounded contract и синхронной EN/RU documentation, проверить в QEMU на подходящей глубине, а затем отдельно решить вопрос публикации проверенного commit. Обновление этого Roadmap не требует Pre-release.

После сознательного закрытия functional release scope отдельный этап pedagogical edition сможет добавить source comments, последовательные учебные главы, architecture diagrams, reproducible lab exercises и updated validation guide.
