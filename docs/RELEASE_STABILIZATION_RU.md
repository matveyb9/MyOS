# GUI release stabilization checklist

> **Язык:** [English](RELEASE_STABILIZATION.md) | [Русский](RELEASE_STABILIZATION_RU.md)


> **Статус:** активный checklist для ветки `gui/bringup`. Он не создаёт release tag и не разрешает перенос в `main` автоматически. Его задача — дать воспроизводимые доказательства перед отдельным решением о первом stable GUI release.

## Автоматизированные проверки

| Команда | Что проверяет | Изоляция |
|---|---|---|
| `make smoke` | Raw-image BIOS и UEFI boot markers, persistent AHCI mount и automatic `[myos]$` entry. | Использует `myos.img`; не записывает пользовательские test files. |
| `make regression` | BIOS default navigation `startgui` через M/H/N/H/Q с clean return плюс retained alias `startgui home`, GUI note create/edit/save, paced console-editor ordinary text save/readback, SDK `cp` copying 305-byte file через 256-byte VFS boundary с overwrite rejection, editor-authored `.mya` source → `build` → `install` → `run`, conditional rejection cases, затем UEFI повтор desktop-home navigation, text/copied-file/native persistence и clean GUI enter/exit. | Создаёт temporary copy `myos.img` и удаляет её после проверки. Рабочий image пользователя не изменяется. |
| `make release-check` | Требует clean Git tree, rebuilds ISO/IMG from scratch, runs `make smoke` и `make regression`, затем печатает exact source commit и SHA-256 обоих artifacts. | Local-only: не создаёт tag, GitHub Release, Pre-release или remote push. |

`make smoke`, `make regression` и `make release-check` требуют QEMU и OVMF. `make regression` uses fixed Q35 configuration and `-drive if=ide,format=raw`, because this is the supported persistent AHCI path. `make release-check` additionally requires a clean Git tree and itself runs a clean `make all img`.

```bash
make release-check
```

Ожидаемый итог:

```text
boot smoke: BIOS passed
boot smoke: UEFI passed
interactive regression: BIOS GUI/native workflow passed
interactive regression: UEFI persistence workflow passed
release candidate: source commit <full-SHA>
release candidate: artifacts
<SHA-256>  myos.iso
<SHA-256>  myos.img
release candidate: automated checks passed
```

## Ручные release gates

Автоматизированные commands намеренно ограничены. До нового GUI release tag необходимо вручную подтвердить следующее:

| Gate | Требуемое evidence |
|---|---|
| Framebuffer visual check | Desktop, windows, pointer, focus, note editor and return to shell остаются readable in a graphical QEMU session. |
| Fresh persistent workflow | На fresh `myos.img` создать note и native package, затем отдельно перезагрузить guest и проверить оба результата. |
| Migration fixtures | MYPFS003→MYPFS004 и MYPFS002→MYPFS004 fixtures завершают recovery/migration и читаются после second clean mount. |
| Physical x86_64 PC | Boot от disposable USB, keyboard input, framebuffer output и clean poweroff/reboot smoke. Это нельзя заменить QEMU. |
| Release scope | Зафиксировать known limitations, release notes и exact immutable tag target. `make release-check` предоставляет reproducible source SHA и artifact SHA-256, но не создаёт public release. |

## Что не доказывают эти tests

`make smoke` and `make regression` do not establish networking, USB HID, SMP, production security, long-duration stress reliability, physical-hardware compatibility or a full native C toolchain. They are release-stabilization evidence for the currently implemented GUI, включая bounded default desktop launcher, MYPFS004, public SDK VFS copy workflow and restricted native development workflow only.

## Publication rule

После того как automated и manual gates завершены, создание публичного Pre-release или release всё ещё требует отдельного explicit confirmation. Новый immutable tag нельзя retag or force-push; `main` не должен обновляться до отдельного merge decision.
