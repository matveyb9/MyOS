# Политика документации MyOS

> **🌐 LANGUAGE / ЯЗЫК:** **🇷🇺 РУССКИЙ** / [🇺🇸 ENGLISH](DOCUMENTATION_POLICY.md)

Документация — часть продукта. Изменение public behavior не считается завершённым, пока его English и Russian descriptions не обновлены в том же commit.

## Двуязычный стандарт

| Место | English | Русский |
|---|---|---|
| Корень repository | `README.md` | `README_RU.md` |
| Документация | `docs/NAME.md` | `docs/NAME_RU.md` |
| Language switcher | `> **🌐 LANGUAGE / ЯЗЫК:** [🇷🇺 РУССКИЙ](NAME_RU.md) / **🇺🇸 ENGLISH**` | `> **🌐 LANGUAGE / ЯЗЫК:** **🇷🇺 РУССКИЙ** / [🇺🇸 ENGLISH](NAME.md)` |

Каждая Markdown page в корне и `docs/` содержит этот заметный widget сразу после heading. Текущий язык — bold text; другой язык — reciprocal link. Commands, paths, identifiers, source code, version numbers, SHA-256 values и URLs не переводятся.

## Что должно обновляться

| Изменение | Обновить обязательно |
|---|---|
| Build/run command, dependency, Make target или artifact | Оба root README, Platform Guide и User Guide. |
| Shell command, utility, argument, file behavior | Оба root README и оба User Guide. |
| ABI, scheduler, memory, driver или storage invariant | Оба Developer Guide; при user-visible effect — оба User Guide. |
| Branch, tag, release scope или merge rule | Обе страницы Release Guide и root README. |
| Experimental feature, known limit или safety warning | Все затронутые language pairs рядом с affected instruction. |
| Historical record | Сохранить historical banner, оба language files и link из documentation index. |

## Правила ясности

Каждая страница должна отвечать на одну основную задачу. Root README ограничивается project description, status, quick start, documentation links и footer. Detailed procedures находятся в `docs/`, а historical records явно отделены от current guides. Не дублируйте большую инструкцию в нескольких местах: root README ссылается на guide, а guide содержит detail.

## Проверка перед commit

| Проверка | Ожидаемый результат |
|---|---|
| Есть ли language pair? | Каждая новая или переименованная Markdown page имеет `NAME.md` и `NAME_RU.md`. |
| Есть ли switcher? | Обе страницы содержат точные reciprocal links после H1. |
| Совпадает ли смысл? | English и Russian sections описывают один scope, limits и status. |
| Верны ли links? | English links ведут на English docs, Russian links — на `_RU` docs. |
| Есть ли user-visible change? | Обновлены root README, relevant guides и current status if needed. |
| Изменяются ли release rules? | Обновлён Release Guide; commits and release notes follow its bilingual convention. |

## История изменений и публикация

Commit subject пишется коротким English imperative summary. После пустой строки body содержит равнозначные descriptions `[EN]:` и `[RU]:` именно в этом порядке; обе фиксируют один scope, compatibility effect и limits. Когда Manus AI существенно помогает, финальным transparency trailer становится `Assisted-by: Manus AI`. Future release notes размещают заметный widget `🌐 RELEASE LANGUAGES / ЯЗЫКИ РЕЛИЗА` под title и используют равнозначные sections `## 🇺🇸 [EN] ENGLISH` и `## 🇷🇺 [RU] РУССКИЙ`; template и required fields находятся в [Release Guide](RELEASES_RU.md).

## Термины

| Термин | Значение |
|---|---|
| **verified** | Воспроизведено на указанной configuration project validation. |
| **supported** | Ожидаемо работает при documented prerequisites, но может иметь меньшую глубину validation. |
| **experimental** | Полезный путь с явно указанными gaps или limits. |
| **historical** | Сохранённый record, не являющийся current specification. |
