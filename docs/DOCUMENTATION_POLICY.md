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
| Language switcher | `<p align="center"><a href="NAME_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong></p>` | `<p align="center"><strong>🇷🇺 РУССКИЙ</strong> / <a href="NAME.md">🇺🇸 ENGLISH</a></p>` |

Every Markdown page in the root and `docs/` contains this centered selector immediately after the heading. The current language is bold text; the other language is the reciprocal link. Use the shown GitHub-compatible HTML only for this alignment purpose; commands, paths, identifiers, source code, version numbers, SHA-256 values and URLs remain unchanged.

## Required updates

| Change | Must update |
|---|---|
| Build/run command, dependency, Make target or artifact | Both root READMEs, Platform Guide and User Guide. |
| Shell command, utility, argument or file behavior | Both root READMEs and both User Guides. |
| ABI, scheduler, memory, driver or storage invariant | Both Developer Guides; both User Guides when user-visible. |
| Branch, tag, release scope or merge rule | Both Release Guides and root README. |
| Experimental feature, known limit or safety warning | Every affected language pair beside the affected instruction. |
| Historical record | Retain its historical banner, both language files and a documentation-index link. |

## Clarity rules

Each page serves one primary task. The root README contains only a project description, status, quick start, documentation links and footer. Detailed procedures belong in `docs/`; historical records remain visibly separate from current guides. Do not duplicate a long procedure across multiple pages: the root README links to the guide and the guide contains the detail.

## Review before commit

| Check | Expected result |
|---|---|
| Does a language pair exist? | Every new or renamed Markdown page has `NAME.md` and `NAME_RU.md`. |
| Does the switcher exist? | Both pages contain accurate reciprocal links after the H1. |
| Does meaning match? | English and Russian sections state the same scope, limits and status. |
| Do links match language? | English links lead to English docs; Russian links lead to `_RU` docs. |
| Is the change user-visible? | Root README, relevant guides and current status are updated. |
| Did release rules change? | Release Guide is updated; commits and release notes follow its bilingual convention. |

## Change history and publication

A commit subject is a concise English imperative summary. After one blank line, its body contains equivalent `[EN]:` and `[RU]:` descriptions in that order; both state the same scope, compatibility effect and limits. When Manus AI materially assists, `Assisted-by: Manus AI` is the final transparency trailer. Future release notes place a centered flag-only selector beneath the title and use equivalent `## 🇺🇸 [EN] ENGLISH` and `## 🇷🇺 [RU] РУССКИЙ` sections; the [Release Guide](RELEASES.md) provides the template and required fields.

## Terms

| Term | Meaning |
|---|---|
| **verified** | Reproduced on the stated configuration by project validation. |
| **supported** | Expected to work with documented prerequisites, though validation depth may differ. |
| **experimental** | Useful path with explicitly stated gaps or limits. |
| **historical** | Retained record that is not the current specification. |
