# Политика документации MyOS

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="DOCUMENTATION_POLICY.md">🇺🇸 ENGLISH</a>
</p>

Документация — часть продукта. Изменение public behavior не считается завершённым, пока его English и Russian descriptions не обновлены в том же commit.

## Двуязычный стандарт

| Место | English | Русский |
|---|---|---|
| Корень repository | `README.md` | `README_RU.md` |
| Документация | `docs/NAME.md` | `docs/NAME_RU.md` |

Каждая Markdown page содержит центрированный reciprocal selector сразу после heading. Текущий язык выделен bold text; альтернативный язык — link. `<p align="center">` используется только для этого selector. English pages ссылаются на English counterparts, Russian pages — на `_RU` counterparts. Commands, paths, identifiers, source code, versions, SHA-256 values и URLs не переводятся.

## Проверка перед commit

Убедитесь, что каждая Markdown page имеет оба файла, оба switchers точны, English и Russian описывают один scope и limits, local links остаются внутри выбранного языка, а relevant README и guides обновлены в том же commit, что и public behavior.

## История изменений

Commit subjects — короткие English imperatives. После пустой строки body содержит равнозначные descriptions `[EN]:` и `[RU]:` именно в этом порядке. Когда Manus AI существенно помогает, финальным transparency trailer становится `Assisted-by: Manus AI`. Release descriptions используют центрированный flag-only selector и equivalent sections `## 🇺🇸 [EN] ENGLISH` и `## 🇷🇺 [RU] РУССКИЙ`. См. [Release Guide](RELEASES_RU.md).
