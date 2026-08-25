# Workflow разработки MyOS

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="DEVELOPMENT_WORKFLOW.md">🇺🇸 ENGLISH</a>
</p>

Это руководство определяет практический workflow разработки MyOS после интеграции QEMU-validated GUI и user-program platform в `main`.

> **Основное правило:** QEMU BIOS/UEFI validation достаточно для разработки и явно помеченных **Pre-releases**, но не доказывает новую GUI/platform stable line. Пока physical-PC validation недоступна, `console-stable` остаётся единственной stable-веткой.

## Роли веток

| Reference | Назначение | Правило изменений |
|---|---|---|
| `console-stable` | Текущий проверенный console baseline на `v0.12.1-console`. | Принимать только отдельно одобренные, узкие maintenance fixes. |
| `main` | Активная QEMU-validated integration line для GUI, VFS, SDK и native-platform work. | Принимать завершённые короткоживущие feature, fix и documentation branches. |
| `feature/<scope>` | Одна ограниченная пользовательская или platform capability. | Создавать от текущего `main`; удалять после successful merge и проверки. |
| `fix/<scope>` | Одно ограниченное исправление regression или defect. | Создавать от текущего `main`; удалять после successful merge и проверки. |
| `docs/<scope>` | Documentation или policy-only update. | Создавать от текущего `main`; удалять после merge. |
| `stable/<series>` | Будущая поддерживаемая GUI/platform maintenance line, например `stable/v0.14`. | Создавать только после выполнения всех stable criteria ниже. |
| `feature/gui` | Историческая GUI integration line. | Сохранить как read-only historical reference; новую работу не добавлять. |

## Начинайте каждое изменение от main

```bash
git switch main
git pull --ff-only
git switch -c feature/<short-scope>
```

Feature-ветка имеет один небольшой, демонстрируемый scope и явные acceptance criteria. В этой же ветке находятся implementation, relevant tests и парная English/Russian documentation. Не смешивайте новую capability с unrelated cleanup, storage-format changes или release preparation.

Перед merge запускайте checks, соответствующие изменённой поверхности. `make release-check` обязателен для user-visible, boot, VFS, persistence, GUI, ABI, SDK или native-platform changes. Изменения storage, ABI, image layout и format дополнительно требуют migration и compatibility evidence. Merge в `main` — это integration decision; он не создаёт tag или public publication.

## Создавайте Pre-release по milestone

Создавайте Pre-release только после завершённого целостного milestone, который пользователь может скачать, загрузить в QEMU и оценить. Не создавайте его после каждого merge, documentation update, refactor или незавершённой feature.

| Ситуация | Создавать Pre-release? |
|---|---|
| Documentation-only, refactor-only или internal maintenance change | Нет. |
| Незавершённая feature без законченного user flow | Нет. |
| Завершённая user-facing capability с regression и парной documentation | Да, если это осмысленный внешний checkpoint. |
| ABI, VFS/storage format, boot-image или migration change | Да, до следующей крупной доработки. |
| Конец цельного sprint | Да, если выполнены все применимые criteria ниже. |

### Checklist Pre-release

Перед immutable tag из `main`:

| Область | Обязательное evidence |
|---|---|
| Scope | Milestone завершён, а последующая работа не смешана с release preparation. |
| Clean source | `git status --short` пуст, точный source SHA зафиксирован. |
| Automation | `make release-check` проходит: clean rebuild, BIOS/UEFI smoke, BIOS GUI/native regression и UEFI persistence regression. |
| Visual QEMU check | Graphical QEMU session подтверждает readable desktop, pointer, open/close actions и return to shell. |
| Fresh persistence | Fresh image сохраняет хотя бы один user file и один installed native package после отдельного reboot. |
| Migration | При изменении storage, image layout, ABI или VFS format проходят применимые migration fixtures и second-mount readback. |
| Documentation | English и Russian README, User Guide, Developer Guide, Release Guide и release notes описывают одинаковые scope и limits. |
| Disclosure | Notes явно содержат **Pre-release**, QEMU BIOS/UEFI validation и отсутствие physical-PC validation claim. |
| Assets | Приложить `myos.iso`, `myos.img` и `SHA256SUMS.txt`; никогда не заменять опубликованные tag или asset bytes. |

Используйте новый immutable tag для каждого public checkpoint, например `v0.14.0-pre.1`, затем `v0.14.0-pre.2` для исправленного checkpoint. Pre-release требует отдельного явного подтверждения; он никогда не является automatic side effect merge.

## Создавайте новую stable line только после hardware validation

Stable-ветка — это support promise, а не синоним passing QEMU test. Не создавайте новую GUI/platform stable-ветку, пока physical-PC validation недоступна.

Будущая `stable/<series>` требует всех условий:

| Требование | Почему необходимо |
|---|---|
| Хотя бы один Pre-release с тем же primary scope | Stable-линия не должна быть первым external checkpoint новой архитектуры. |
| Нет известного blocker или regression для заявленного scope | Supported contract должен быть явным и testable. |
| Полный QEMU gate на exact candidate SHA | Сохраняет reproducible baseline. |
| Manual graphical и fresh-persistence QEMU checks | Automation не заменяет видимый user workflow. |
| Применимые migration fixtures | Защищают persistent data при declared compatibility changes. |
| Physical x86_64 PC test | Подтвердить disposable-USB boot, keyboard, framebuffer, persistent read/write и clean reboot/poweroff. |
| Отдельные scope-freeze и release decision | Не допускает accidental feature creep в stable promise. |

После создания stable line новые capabilities продолжаются в `main`. Stable maintenance использует отдельные `fix/<scope>` branches от stable line и ограничивается одобренными fixes.

## Решения по умолчанию

| Вопрос | Решение по умолчанию |
|---|---|
| Нужен ли Pre-release после каждого merge? | Нет; только после законченных milestones. |
| Может ли QEMU-only work получить Pre-release? | Да, при required QEMU evidence и explicit disclosure. |
| Может ли QEMU-only work получить stable label? | Нет. |
| Где начинается новая feature? | От последнего `main`. |
| Что делать с обычными merged feature/fix ветками? | Удалять после проверки. |
| Что делать с `feature/gui`? | Сохранить как историю; не разрабатывать там. |
| Можно ли менять published tag или artifact? | Нет; вместо этого публикуется новый immutable Pre-release. |

Для release syntax, bilingual commit rules и publication safeguards откройте [Release Guide](RELEASES_RU.md). Для build, ABI, storage и verification details — [Developer Guide](DEVELOPER_GUIDE_RU.md).
