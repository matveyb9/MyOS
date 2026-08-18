# Releases, branches and project scope

MyOS uses Git references to keep completed console releases separate from subsequent GUI experiments and future work. A new console patch release may advance `main` and `console-stable`, but it never rewrites an earlier immutable tag.

## Current references

| Reference | Role | Current meaning |
|---|---|---|
| `main` | Active console-maintenance branch | Contains approved console-only maintenance changes, including shell UX and signed `calc` improvements. |
| `console-stable` | Latest stable console baseline | Points to the reviewed source snapshot released as `v0.12.1-console`. |
| `v0.12.1-console` | Immutable annotated tag | Marks the refreshed MyOS Console 0.12.1 release with console UX improvements. |
| `v0.12.0-console` | Immutable annotated tag | Preserves the original completed MyOS Console 0.12.0 release. Never move or rewrite it. |
| `v0.12.2-gui-preview` | Immutable annotated preview tag | Marks the tested framebuffer GUI scope at the first experimental GUI checkpoint; it is not a stable console or production GUI release. |
| `gui/bringup` | Separate GUI development branch | Contains the tagged GUI preview and continues with persistent user-program execution, MyOS SDK and native development work. It is not part of a console release. |

## Which branch should I use?

| Goal | Use |
|---|---|
| Build and use the latest finished console MyOS | `main`, `console-stable` or `v0.12.1-console`. |
| Study the original console completion point before UX refreshes | `git checkout v0.12.0-console`. |
| Maintain current console without GUI changes | Create a fix branch from `main` or `console-stable`. |
| Inspect the fixed GUI preview checkpoint | `git switch --detach v0.12.2-gui-preview`. |
| Continue GUI and user-program development | `git switch gui/bringup`. |
| Publish the whole project | Push all intended branches, console tags and the GUI preview tag. |

## Safe Git commands

Show the available references:

```bash
git branch -a
git tag
git log --oneline --decorate --all --graph
```

Return to the current console maintenance branch:

```bash
git switch main
```

Inspect the stable refreshed console release:

```bash
git switch --detach v0.12.1-console
```

Inspect the original console release instead:

```bash
git switch --detach v0.12.0-console
```

Inspect the immutable experimental GUI preview:

```bash
git switch --detach v0.12.2-gui-preview
```

Create a safe branch before making a change:

```bash
git switch main
git switch -c docs/my-change
```

## GitHub publication

When a remote GitHub repository exists, publish all intended references explicitly:

```bash
git push -u origin main
git push origin console-stable
git push origin gui/bringup
git push origin v0.12.0-console
git push origin v0.12.1-console
git push origin v0.12.2-gui-preview
```

Generated files (`build/`, `myos.iso`, `myos.img`) remain untracked and should be attached to a GitHub Release when needed. They are reproducible with `make all img`.

> Do not force-push or retag any release checkpoint. `v0.12.0-console` is the historical first completion point; `v0.12.1-console` is the reviewed console UX refresh; `v0.12.2-gui-preview` is the fixed experimental GUI scope. The preview does not change `main` or `console-stable` and must not be presented as a production GUI release.
