# Политика актуальности документации MyOS

Документация — часть проекта, а не завершающий необязательный этап. Каждый commit, который меняет публичное поведение MyOS, обязан обновлять соответствующие документы **в том же commit**.

## Обязательное правило

> Если изменение влияет на то, как пользователь собирает, запускает, использует, тестирует или понимает MyOS, документация обновляется одновременно с кодом.

Не допускается считать README или manual «временно устаревшими» после принятого изменения. Если функциональность экспериментальная, это должно быть явно отмечено в документации вместе с веткой, ограничениями и безопасным способом проверки.

## Матрица обновлений

| Изменение | Обновить обязательно |
|---|---|
| Build command, dependency, Make target или artifact name | `README.md`, `PLATFORMS_RU.md`, при необходимости `USER_GUIDE_RU.md`. |
| Shell command, utility, argument, environment variable или file behavior | `README.md` и `USER_GUIDE_RU.md`. |
| Syscall, ABI structure, scheduler, memory, driver или storage invariant | `DEVELOPER_GUIDE_RU.md`; при user-visible effect — также user guide. |
| Supported host platform, package name or QEMU invocation | `PLATFORMS_RU.md` и concise table in `README.md`. |
| Branch, tag, release scope, merge policy | `RELEASES_RU.md` и `README.md`. |
| Security or destructive-operation warning | `USER_GUIDE_RU.md`, `PLATFORMS_RU.md` and any affected command example. |
| Historical document becomes obsolete | Add an explicit historical banner and link to the current manual in `docs/README.md`. |

## Review checklist before commit

Before committing a feature or maintenance change, answer the following questions.

| Check | Expected answer |
|---|---|
| Has a public command or output changed? | Update the user-facing guide and README command tables. |
| Has a build/run instruction changed? | Update README and platform manual. |
| Has an internal interface or invariant changed? | Update developer guide. |
| Did a new platform become tested or untested? | Update the support matrix. |
| Is a dangerous action shown? | Keep the warning beside the command. |
| Did a previous guide become historical? | Mark it explicitly rather than silently leaving it misleading. |
| Do internal links still resolve? | Validate them before commit. |

## Documentation structure

| Location | Role |
|---|---|
| `README.md` | Project overview and concise first-run guide. It must remain easy to read from a GitHub repository page. |
| `docs/USER_GUIDE_RU.md` | Detailed plain-language guide for ordinary users. |
| `docs/PLATFORMS_RU.md` | Host platform setup and support status. |
| `docs/DEVELOPER_GUIDE_RU.md` | Source architecture, ABI and contributor workflow. |
| `docs/RELEASES_RU.md` | Branches, tags and release boundaries. |
| `docs/README.md` | Documentation map and explicit separation of current versus historical files. |
| Historical `docs/*.md` | Earlier decision or validation records; not authoritative instructions. |

## Terminology

Use these terms consistently.

| Term | Meaning |
|---|---|
| **console release** | Completed text-mode MyOS milestone at `v0.12.0-console`. |
| **current project** | All current Git branches, including separate GUI experiments. |
| **verified** | Reproduced on the listed configuration by project validation. |
| **supported** | Expected to work with documented prerequisites, but may not have the same validation depth. |
| **experimental** | Useful path with known gaps or limited validation; state the gaps. |
| **historical** | A preserved early document that must not be used as the current specification. |

## Language

Current user-facing documentation is maintained in Russian because that is the project owner’s working language. Technical identifiers, shell commands, file names and code symbols remain in their exact English/source form.
