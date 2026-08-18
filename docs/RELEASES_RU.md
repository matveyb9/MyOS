# Releases, branches and project scope

MyOS uses Git references to keep the completed console operating system separate from subsequent GUI experiments and future work.

## Current references

| Reference | Role | Current meaning |
|---|---|---|
| `main` | Active project maintenance branch | Contains current documentation and approved console-only maintenance changes. |
| `console-stable` | Strict console baseline | Points to the final console implementation before documentation refreshes. |
| `v0.12.0-console` | Immutable annotated tag | Marks completed MyOS Console 0.12.0-dev. Never move or rewrite it. |
| `gui/bringup` | Separate GUI experiment branch | Contains early native framebuffer GUI work; not part of console release. |

## Which branch should I use?

| Goal | Use |
|---|---|
| Build and use finished console MyOS | `main` for current manuals, or `console-stable` / `v0.12.0-console` for strict original console snapshot. |
| Study exactly what was complete before GUI | `git checkout v0.12.0-console`. |
| Maintain console without GUI changes | Create a fix branch from `main` or `console-stable`, depending on whether documentation refreshes are desired. |
| Inspect experimental GUI work | `git switch gui/bringup`. |
| Publish the whole project | Push all branches and tags. |

## Safe Git commands

Show what is available:

```bash
git branch -a
git tag
git log --oneline --decorate --all --graph
```

Return to current console maintenance branch:

```bash
git switch main
```

Inspect the immutable console release:

```bash
git switch --detach v0.12.0-console
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
```

Generated files (`build/`, `myos.iso`, `myos.img`) remain untracked and should be attached to a GitHub Release when needed. They are reproducible with `make all img`.

> Do not force-push or retag `v0.12.0-console`. It is the project’s historical completion point for the console OS.
