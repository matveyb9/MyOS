# Политика документации MyOS

> **Язык:** [English](DOCUMENTATION_POLICY.md) | [Русский](DOCUMENTATION_POLICY_RU.md)

Документация — часть продукта. Изменение public behavior не считается завершённым, пока его English и Russian descriptions не обновлены в том же commit.

## Двуязычный стандарт

| Место | English | Русский |
|---|---|---|
| Корень repository | `README.md` | `README_RU.md` |
| Документация | `docs/NAME.md` | `docs/NAME_RU.md` |

Каждая Markdown page содержит reciprocal switcher сразу после heading. English pages ссылаются на English counterparts, Russian pages — на `_RU` counterparts. Commands, paths, identifiers, source code, versions, SHA-256 values и URLs не переводятся.

## Проверка перед commit

Убедитесь, что каждая Markdown page имеет оба файла, оба switchers точны, English и Russian описывают один scope и limits, local links остаются внутри выбранного языка, а relevant README и guides обновлены в том же commit, что и public behavior.

## История изменений

Commit subjects — короткие English imperatives. Body после пустой строки начинается с `RU:` и кратко объясняет изменение по-русски. Release descriptions используют equivalent sections `## English` и `## Русский`. См. [Release Guide](RELEASES_RU.md).
