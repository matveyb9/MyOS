# MyOS Development Workflow

<p align="center">
  <a href="DEVELOPMENT_WORKFLOW_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

This guide defines the practical MyOS development workflow after the QEMU-validated GUI and user-program platform was integrated into `main`.

> **Core rule:** QEMU BIOS/UEFI validation is sufficient for development and explicitly labelled **Pre-releases**, but it does not establish a new GUI/platform stable line. Until physical-PC validation is available, `console-stable` remains the only stable branch.

## Branch roles

| Reference | Purpose | Change policy |
|---|---|---|
| `console-stable` | Current reviewed console baseline at `v0.12.1-console`. | Accept only separately approved, narrowly scoped maintenance fixes. |
| `main` | Active QEMU-validated integration line for GUI, VFS, SDK and native-platform work. | Receive completed short-lived feature, fix and documentation branches. |
| `feature/<scope>` | One bounded user-facing or platform capability. | Create from current `main`; delete after successful merge and verification. |
| `fix/<scope>` | One bounded regression or defect correction. | Create from current `main`; delete after successful merge and verification. |
| `docs/<scope>` | Documentation or policy-only update. | Create from current `main`; delete after successful merge. |
| `stable/<series>` | Future supported GUI/platform maintenance line, for example `stable/v0.14`. | Create only after all stable criteria below are met. |
| `feature/gui` | Historical GUI integration line. | Preserve as a read-only historical reference; do not add new work. |

## Start every change from main

```bash
git switch main
git pull --ff-only
git switch -c feature/<short-scope>
```

A feature branch has one small, demonstrable scope and explicit acceptance criteria. The same branch contains implementation, relevant tests, and matching English/Russian documentation. Avoid combining a new capability with unrelated cleanup, storage-format changes, or release preparation.

Before merge, run the checks that match the changed surface. `make release-check` is mandatory for user-visible, boot, VFS, persistence, GUI, ABI, SDK, or native-platform changes. Storage, ABI, image-layout, and format changes additionally require their migration and compatibility evidence. A merge into `main` is an integration decision; it does not create a tag or public publication.

## Create a Pre-release at a milestone

Create a Pre-release only after a completed, coherent milestone that users can download, boot in QEMU, and evaluate. Do **not** create one after every merge, documentation update, refactor, or unfinished feature.

| Situation | Create a Pre-release? |
|---|---|
| Documentation-only, refactor-only, or internal maintenance change | No. |
| Incomplete feature with no finished user flow | No. |
| Completed user-facing capability with regression and paired documentation | Yes, when it is a meaningful external checkpoint. |
| ABI, VFS/storage format, boot-image, or migration change | Yes, before subsequent major development. |
| End of a coherent sprint | Yes, if every applicable criterion below is met. |

### Pre-release checklist

Before an immutable tag from `main`:

| Area | Required evidence |
|---|---|
| Scope | The milestone is complete and subsequent work is not mixed into release preparation. |
| Clean source | `git status --short` is empty and the exact source SHA is recorded. |
| Automation | `make release-check` passes: clean rebuild, BIOS/UEFI smoke, BIOS GUI/native regression, and UEFI persistence regression. |
| Visual QEMU check | A graphical QEMU session confirms readable desktop, pointer, open/close actions, and return to shell. |
| Fresh persistence | A fresh image preserves at least one user file and one installed native package across a separate reboot. |
| Migration | When storage, image layout, ABI, or VFS format changes, applicable migration fixtures and second-mount readback pass. |
| Documentation | English and Russian README, User Guide, Developer Guide, Release Guide, and release notes state the same scope and limits. |
| Disclosure | Notes explicitly state **Pre-release**, QEMU BIOS/UEFI validation, and no physical-PC validation claim. |
| Assets | Attach `myos.iso`, `myos.img`, and `SHA256SUMS.txt`; never replace published tag or asset bytes. |

Use a new immutable tag for every public checkpoint, for example `v0.14.0-pre.1`, then `v0.14.0-pre.2` for a corrected checkpoint. A Pre-release requires separate explicit approval; it is never an automatic side effect of a merge.

## Create a new stable line only after hardware validation

A stable branch is a support promise, not a synonym for a passing QEMU test. Do not create a new GUI/platform stable branch while physical-PC validation is unavailable.

A future `stable/<series>` requires all of the following:

| Requirement | Reason |
|---|---|
| At least one Pre-release with the same primary scope | A stable line must not be the first external checkpoint for a new architecture. |
| No known blocker or regression for the stated scope | The supported contract must be explicit and testable. |
| Full QEMU gate on the exact candidate SHA | Preserves the reproducible baseline. |
| Manual graphical and fresh-persistence QEMU checks | Automation does not replace a visible user workflow. |
| Applicable migration fixtures | Protects persistent data across declared compatibility changes. |
| Physical x86_64 PC test | Confirm disposable-USB boot, keyboard, framebuffer, persistent read/write, and clean reboot/poweroff. |
| Separate scope-freeze and release decision | Prevents accidental feature creep into a stable promise. |

After creating a stable line, new capabilities continue in `main`. Stable maintenance uses separate `fix/<scope>` branches from the stable line and remains limited to approved fixes.

## Default decisions

| Question | Default answer |
|---|---|
| Should every merge receive a Pre-release? | No; only completed milestones do. |
| Can QEMU-only work receive a Pre-release? | Yes, with the required QEMU evidence and explicit disclosure. |
| Can QEMU-only work receive a stable label? | No. |
| Where does a new feature start? | The latest `main`. |
| What happens to ordinary merged feature/fix branches? | Delete them after verification. |
| What happens to `feature/gui`? | Preserve it as history; do not develop there. |
| Can a published tag or artifact be modified? | No; publish a new immutable Pre-release instead. |

For release syntax, bilingual commit rules, and publication safeguards, see the [Release Guide](RELEASES.md). For build, ABI, storage, and verification details, see the [Developer Guide](DEVELOPER_GUIDE.md).
