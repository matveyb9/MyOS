# Branches, releases and change history

<p align="center">
  <a href="RELEASES_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

This guide explains which MyOS line to choose, which references must not change, and how to describe future commits and releases in two languages.

## Project lines

| Reference | Purpose |
|---|---|
| `console-stable` | Reviewed console baseline at `v0.12.1-console`. |
| `main` | Maintained console branch and documentation baseline. |
| `gui/bringup` | Separate experimental GUI and native-development line. |
| `feature/console-ux` | Historical feature branch retained for comparison. |

## Immutable checkpoints

`v0.12.0-console`, `v0.12.1-console`, `v0.12.2-gui-preview` and `v0.13.0-gui-rc.1` are immutable historical checkpoints. Never move, force-push or reuse an existing tag; every new release or pre-release receives a new tag and its own release notes.

## Bilingual commits

Use a concise English imperative subject. After one blank line, add equivalent `[EN]:` and `[RU]:` descriptions. When Manus AI materially assists, retain `Assisted-by: Manus AI` as the final transparency trailer.

```text
docs: refresh bilingual navigation

[EN]: Center the paired language selector and document the same navigation standard.
[RU]: Центрировать парный selector языка и описать тот же navigation standard.

Assisted-by: Manus AI
```

Name affected commands, paths, artifacts or compatibility consequences whenever a change is user-visible. Update English and Russian documentation in the same commit.

## Bilingual release notes

Each release description starts with a centered flag-only selector, then contains equivalent `## 🇺🇸 [EN] ENGLISH` and `## 🇷🇺 [RU] РУССКИЙ` sections. Include status (`Release`, `Pre-release` or `Release candidate`), highlights, verification, compatibility or migration notes, artifacts with SHA-256 when attached, and known limits. Do not present a pre-release as a stable release.

## Safe publication

Run applicable checks, confirm a clean tree and obtain explicit approval before creating any new tag, GitHub Release or Pre-release. Generated `myos.iso` and `myos.img` artifacts do not enter source history; attach them only to an explicitly approved publication.
