# Документация MyOS

`README.md` в корне repository — короткая главная страница проекта: описание, basic build/run commands and links. Все подробные материалы находятся в этой папке.

## Актуальная документация

| Документ | Для кого | Что отвечает |
|---|---|---|
| [USER_GUIDE_RU.md](USER_GUIDE_RU.md) | Обычные пользователи | Сборка, QEMU, shell, files, utilities and USB safety. |
| [PLATFORMS_RU.md](PLATFORMS_RU.md) | Users of Linux, Windows and macOS | Host prerequisites, WSL, MSYS2, QEMU and support status. |
| [DEVELOPER_GUIDE_RU.md](DEVELOPER_GUIDE_RU.md) | Developers | Source tree, kernel architecture, ABI, storage invariants and validation. |
| [RELEASES_RU.md](RELEASES_RU.md) | Everyone using Git | Branches, tags, console boundary, GUI branch and publication commands. |
| [ROADMAP_RU.md](ROADMAP_RU.md) | Everyone following development | Completed milestones, current GUI work, priorities and deferred research. |
| [DOCUMENTATION_POLICY_RU.md](DOCUMENTATION_POLICY_RU.md) | Maintainers | Mandatory same-commit documentation update rules. |
| [GUI_BRINGUP_RU.md](GUI_BRINGUP_RU.md) | `gui/bringup` users | Experimental framebuffer GUI, VFS viewer, note editor and GUI roadmap. |
| [NATIVE_BUILD_RU.md](NATIVE_BUILD_RU.md) | User-program authors | Restricted in-MyOS assembler, project-to-package workflow, bounds and validation. |

## How to choose a guide

| Need | Start with |
|---|---|
| «Как запустить MyOS?» | `../README.md`, then `USER_GUIDE_RU.md`. |
| «Как установить инструменты на Windows/macOS/Linux?» | `PLATFORMS_RU.md`. |
| «Как устроены kernel, syscall или filesystem?» | `DEVELOPER_GUIDE_RU.md`. |
| «Почему есть несколько branches?» | `RELEASES_RU.md`. |
| «Что уже завершено и что будет дальше?» | `ROADMAP_RU.md`. |
| «Как пользоваться experimental GUI?» | `GUI_BRINGUP_RU.md` после перехода на `gui/bringup`. |
| «Как написать и собрать первую программу прямо в MyOS?» | `NATIVE_BUILD_RU.md`. |
| «Как не допустить устаревшей документации?» | `DOCUMENTATION_POLICY_RU.md`. |

## Historical development notes

The files below are preserved records from early milestones `0.3.0-dev`–`0.7.0-dev`. Each now has an explicit **Исторический документ** banner. They are useful for tracking design evolution, but they are not current build instructions or specifications for console release `0.12.0-dev`.

| File | Historical topic |
|---|---|
| `architecture.md` | Early architecture and framebuffer console plan. |
| `validation.md` | Early artifact and boot validation; references obsolete `myos.hdd`. |
| `interrupt-model.md`, `irq-validation.md` | Initial PIC/APIC/PIT/PS2 work. |
| `paging-model.md`, `paging-validation.md` | Initial owned PML4 and heap work. |
| `memory-safety-model.md`, `memory-safety-validation.md` | Early memory-safety milestone. |
| `framebuffer-console-model.md`, `framebuffer-validation.md`, `framebuffer-visual-check.md` | First framebuffer text-console milestone. |
| `research-apic-virtual-wire.md` | APIC virtual-wire research reference. |
| `architecture-decision-32bit.md` | Active decision record: MyOS remains x86_64-only. |

## Maintenance rule

Any change that affects the build, run flow, host support, shell, user-visible behavior, storage, public ABI, safety warning, branch or release must update the relevant current document in the same commit. Full rules are in [DOCUMENTATION_POLICY_RU.md](DOCUMENTATION_POLICY_RU.md).
