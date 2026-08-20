# Ветки, релизы и история изменений

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="RELEASES.md">🇺🇸 ENGLISH</a>
</p>

Этот документ объясняет, какую линию MyOS выбрать, какие refs нельзя изменять и как оформлять future commits и releases на двух языках.

## Линии проекта

| Reference | Назначение |
|---|---|
| `console-stable` | Проверенный console baseline на `v0.12.1-console`. |
| `main` | Поддерживаемая консольная ветка и документационный baseline. |
| `gui/bringup` | Отдельная экспериментальная GUI и native-development line. |
| `feature/console-ux` | Историческая feature branch для сравнения. |

## Неизменяемые checkpoints

`v0.12.0-console`, `v0.12.1-console`, `v0.12.2-gui-preview` и `v0.13.0-gui-rc.1` — immutable historical checkpoints. Не передвигайте, не force-push и не используйте существующий tag повторно: каждый новый release или pre-release получает новый tag и отдельные release notes.

## Двуязычные commits

Используйте короткий English imperative subject. После пустой строки добавляйте краткий русский body, начинающийся с `RU:`.

```text
docs: add bilingual navigation

RU: Добавлены парные English/Russian страницы, переключатели языка и
проверяемые внутренние ссылки.
```

Если изменение user-visible, назовите затронутые commands, paths, artifacts или compatibility consequences. English и Russian документация обновляются в том же commit.

## Двуязычные release notes

Каждый release description начинается с центрированного flag-only selector, затем содержит equivalent sections `## 🇺🇸 [EN] ENGLISH` и `## 🇷🇺 [RU] РУССКИЙ`. Укажите status (`Release`, `Pre-release` или `Release candidate`), highlights, verification, compatibility или migration notes, artifacts с SHA-256 при приложении и known limits. Не представляйте pre-release как stable release.

## Безопасная публикация

Перед новым tag, GitHub Release или Pre-release выполните applicable checks, убедитесь в clean tree и получите явное подтверждение. Generated artifacts `myos.iso` и `myos.img` не входят в source history; они attach only to explicitly approved publication.
