# Ветки, релизы и история изменений

> **🌐 LANGUAGE / ЯЗЫК:** **🇷🇺 РУССКИЙ** / [🇺🇸 ENGLISH](RELEASES.md)

Этот документ объясняет, какую линию MyOS выбрать, какие refs нельзя изменять и как оформлять будущие commits и releases на двух языках.

## Линии проекта

| Reference | Назначение | Статус |
|---|---|---|
| `console-stable` | Проверенный baseline консольной ОС. | Стабильная линия на `v0.12.1-console`. |
| `main` | Поддерживаемая консольная линия. | Только console maintenance и документация. |
| `gui/bringup` | GUI и native-development line. | Экспериментальная, не сливается в `main` автоматически. |
| `feature/console-ux` | Историческая feature branch. | Сохранена для истории и сравнения. |

## Неизменяемые checkpoints

| Tag | Значение |
|---|---|
| `v0.12.0-console` | Исходная граница завершения console OS. |
| `v0.12.1-console` | Проверенный console UX refresh. |
| `v0.12.2-gui-preview` | Первый tested framebuffer GUI checkpoint. |
| `v0.13.0-gui-rc.1` | Опубликованный GitHub Pre-release GUI release candidate. |

> Immutable tag нельзя передвигать, заменять force-push или использовать повторно. Новый release или pre-release всегда получает **новый tag** и отдельные notes.

## Какую ветку выбрать

| Цель | Действие |
|---|---|
| Использовать проверенную консольную ОС | `git switch main` или `git switch console-stable`. |
| Изучить конкретный checkpoint | `git switch --detach <tag>`. |
| Продолжить GUI или native-development work | `git switch gui/bringup`. |
| Исправить консольную линию | Создать отдельную branch от `main`. |

## Двуязычные commits

Git subject остаётся коротким **английским** imperative summary. Это делает историю, GitHub lists и command-line output компактными и удобными для поиска. Каждое meaningful change затем получает две равнозначные, явно помеченные строки body: сначала **`[EN]:`**, затем **`[RU]:`**. Финальный trailer прозрачно отмечает сотрудничество, когда в изменении существенно помогал Manus AI.

```text
native: add labels and forward jumps

[EN]: Add bounded forward-only labels and jumps; update help, regression and paired documentation.
[RU]: Добавить ограниченные метки и переходы только вперёд; обновить help, regression и парную документацию.

Assisted-by: Manus AI
```

| Поле | Правило |
|---|---|
| Subject | English, imperative, до 72 characters; без точки в конце. |
| `[EN]` description | После пустой строки кратко указать scope или user-visible effect одним английским предложением. |
| `[RU]` description | Сразу следом привести равнозначное короткое русское предложение с тем же change и limits. |
| Collaboration | Когда Manus AI существенно помогает, сохранять `Assisted-by: Manus AI` финальным trailer. Он не заменяет identity автора repository. |
| Scope | Укажите ключевой subsystem при необходимости: `docs:`, `fs:`, `gui:`, `native:`. |
| Breaking or user-visible change | В обеих description lines явно назовите command, path, artifact или compatibility consequence. |
| Documentation | Если меняется public behavior, английская и русская страницы обновляются в том же commit. |

Та же структура применяется к documentation-only maintenance:

```text
docs: refresh bilingual navigation

[EN]: Replace legacy language switches with the prominent active-language widget.
[RU]: Заменить прежние переключатели языка заметным виджетом активного языка.

Assisted-by: Manus AI
```

## Двуязычные release notes

Release description всегда содержит два равноправных, визуально отмеченных раздела: сначала **`🇺🇸 [EN] ENGLISH`**, затем **`🇷🇺 [RU] РУССКИЙ`**. Сразу под title размещается заметный release-language widget. Оба раздела описывают один и тот же scope; не нужно смешивать два языка в одной строке или таблице.

```md
# MyOS vX.Y.Z — Short release name

> **🌐 RELEASE LANGUAGES / ЯЗЫКИ РЕЛИЗА:** **🇺🇸 [EN] ENGLISH** · **🇷🇺 [RU] РУССКИЙ**

## 🇺🇸 [EN] ENGLISH

**Status:** Release | Pre-release | Release candidate

### Highlights

- Clear user-facing result.
- Compatibility or migration note.

### Verification

- Exact checks that passed.

### Known limits

- Explicit remaining gaps.

## 🇷🇺 [RU] РУССКИЙ

**Статус:** Релиз | Предрелиз | Кандидат в релизы

### Главное

- Тот же пользовательский результат.
- Та же информация о совместимости или миграции.

### Проверка

- Те же успешно пройденные проверки.

### Известные ограничения

- Те же оставшиеся границы.
```

| Release element | Обязательное содержание |
|---|---|
| Title | Version и короткое английское название; смысл повторяет раздел `🇷🇺 [RU]`. |
| Status | Явно обозначить `Release`, `Pre-release` или `Release candidate`. |
| Scope | Что включено и что намеренно не включено. |
| Verification | Commands, firmware paths, hardware scope и status. |
| Compatibility | Storage format, migration, boot artifact или API changes. |
| Artifacts | Exact filenames и SHA-256, если artifacts приложены. |
| Limits | Known gaps; pre-release не представляется stable release. |

## Безопасная публикация

Перед новым tag выполните applicable checks, убедитесь в clean Git tree и получите явное подтверждение на Release или Pre-release. Build artifacts `myos.iso` и `myos.img` не добавляются в source history; они attach only to an explicitly approved GitHub Release or Pre-release.

```bash
make release-check
git status --short
git tag --list
git push origin main console-stable feature/console-ux gui/bringup
```

Публикация tag, GitHub Release или Pre-release выполняется только после отдельного подтверждения. Она не является побочным эффектом обычного documentation или code commit.
