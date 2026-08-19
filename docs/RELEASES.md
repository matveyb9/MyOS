# Branches, releases and change history

> **Language:** [English](RELEASES.md) | [Русский](RELEASES_RU.md)

This guide explains which MyOS line to choose, which references must not change, and how to describe future commits and releases in two languages.

## Project lines

| Reference | Purpose | Status |
|---|---|---|
| `console-stable` | Reviewed console operating-system baseline. | Stable line at `v0.12.1-console`. |
| `main` | Maintained console line. | Console maintenance and documentation only. |
| `gui/bringup` | GUI and native-development line. | Experimental; never merged into `main` automatically. |
| `feature/console-ux` | Historical feature branch. | Retained for history and comparison. |

## Immutable checkpoints

| Tag | Meaning |
|---|---|
| `v0.12.0-console` | Original completed-console boundary. |
| `v0.12.1-console` | Reviewed console UX refresh. |
| `v0.12.2-gui-preview` | First tested framebuffer-GUI checkpoint. |
| `v0.13.0-gui-rc.1` | Published GitHub Pre-release GUI release candidate. |

> An immutable tag is never moved, force-pushed or reused. Every new release or pre-release receives a **new tag** and its own release notes.

## Choose a branch

| Goal | Action |
|---|---|
| Use the reviewed console OS | `git switch main` or `git switch console-stable`. |
| Study a particular checkpoint | `git switch --detach <tag>`. |
| Continue GUI or native-development work | `git switch gui/bringup`. |
| Fix the console line | Create a separate branch from `main`. |

## Bilingual commits

The Git subject remains a short **English** imperative summary. This keeps history, GitHub lists and command-line output compact and searchable. A meaningful change then receives a brief **Russian** body explaining its intent and affected user-visible contracts.

```text
Add labels and forward jumps to native assembler

RU: Добавлены метки `label name:` и безусловные переходы `jump name`
только на более позднюю метку. Обновлены `help asm`, regression и
двуязычная документация.
```

| Field | Rule |
|---|---|
| Subject | English, imperative, up to 72 characters, with no trailing period. |
| Body | Leave one blank line after the subject, then use `RU:` and one to three short Russian lines. |
| Scope | Add a subsystem prefix when useful: `docs:`, `fs:`, `gui:` or `native:`. |
| Breaking or user-visible change | Name the command, path, artifact or compatibility consequence in the body. |
| Documentation | When public behavior changes, update the English and Russian pages in the same commit. |

A compact format is suitable for documentation-only maintenance:

```text
docs: add bilingual navigation

RU: Добавлены парные English/Russian страницы, переключатели языка и
проверяемые внутренние ссылки.
```

## Bilingual release notes

Every release description contains two equivalent sections: **English** first, then **Русский**. They describe the same scope. Do not mix two languages within one line or table row.

```md
# MyOS vX.Y.Z — Short release name

## English

**Status:** Release | Pre-release | Release candidate

### Highlights

- Clear user-facing result.
- Compatibility or migration note.

### Verification

- Exact checks that passed.

### Known limits

- Explicit remaining gaps.

## Русский

**Статус:** Релиз | Предрелиз | Кандидат в релизы

### Главное

- Тот же пользовательский результат.
- Та же информация о совместимости или миграции.

### Проверка

- Те же успешно пройденные проверки.

### Известные ограничения

- Те же оставшиеся границы.
```

| Release element | Required content |
|---|---|
| Title | Version and short English name; the Russian section repeats its meaning. |
| Status | State `Release`, `Pre-release` or `Release candidate` explicitly. |
| Scope | What is included and what is intentionally excluded. |
| Verification | Commands, firmware paths, hardware scope and result. |
| Compatibility | Storage-format, migration, boot-artifact or API changes. |
| Artifacts | Exact filenames and SHA-256 when artifacts are attached. |
| Limits | Known gaps; never present a pre-release as a stable release. |

## Safe publication

Before a new tag, run applicable checks, confirm a clean Git tree and obtain explicit approval for a Release or Pre-release. Generated `myos.iso` and `myos.img` artifacts do not enter source history; attach them only to an explicitly approved GitHub Release or Pre-release.

```bash
make release-check
git status --short
git tag --list
git push origin main console-stable feature/console-ux gui/bringup
```

Publishing a tag, GitHub Release or Pre-release always requires separate confirmation. It is never a side effect of an ordinary documentation or code commit.
