# MyOS Documentation Policy

> **Language:** [English](DOCUMENTATION_POLICY.md) | [Русский](DOCUMENTATION_POLICY_RU.md)

Documentation is part of the product. A public-behavior change is not complete until its English and Russian descriptions are updated in the same commit.

## Bilingual standard

| Location | English | Russian |
|---|---|---|
| Repository root | `README.md` | `README_RU.md` |
| Documentation | `docs/NAME.md` | `docs/NAME_RU.md` |

Every Markdown page has a reciprocal switcher immediately after its heading. English pages link to English counterparts; Russian pages link to `_RU` counterparts. Commands, paths, identifiers, source code, versions, SHA-256 values and URLs remain unchanged.

## Review before commit

Check that every Markdown page has both files, both switchers are accurate, English and Russian state the same scope and limits, local links stay within the selected language, and relevant README and guides change in the same commit as public behavior.

## Change history

Commit subjects are concise English imperatives. The body begins with `RU:` after one blank line and explains the change briefly in Russian. Release descriptions use equivalent `## English` and `## Русский` sections. See [Release Guide](RELEASES.md).
