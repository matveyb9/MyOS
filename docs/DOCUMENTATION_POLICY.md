# MyOS Documentation Policy

<p align="center">
  <a href="DOCUMENTATION_POLICY_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

Documentation is part of the product. A public-behavior change is not complete until its English and Russian descriptions are updated in the same commit.

## Bilingual standard

| Location | English | Russian |
|---|---|---|
| Repository root | `README.md` | `README_RU.md` |
| Documentation | `docs/NAME.md` | `docs/NAME_RU.md` |

Every Markdown page has a centered reciprocal selector immediately after its heading. The active language is bold text; the alternative language is a link. Use `<p align="center">` only for this selector. English pages link to English counterparts; Russian pages link to `_RU` counterparts. Commands, paths, identifiers, source code, versions, SHA-256 values and URLs remain unchanged.

## Review before commit

Check that every Markdown page has both files, both switchers are accurate, English and Russian state the same scope and limits, local links stay within the selected language, and relevant README and guides change in the same commit as public behavior.

## Change history

Commit subjects are concise English imperatives. After one blank line, the body contains equivalent `[EN]:` and `[RU]:` descriptions in that order. When Manus AI materially assists, `Assisted-by: Manus AI` is the final transparency trailer. Release descriptions use a centered flag-only selector and equivalent `## 🇺🇸 [EN] ENGLISH` and `## 🇷🇺 [RU] РУССКИЙ` sections. See [Release Guide](RELEASES.md).
