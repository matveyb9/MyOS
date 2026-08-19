# GUI release stabilization checklist

> **Статус:** активный checklist для ветки `gui/bringup`. Он не создаёт release tag и не разрешает перенос в `main` автоматически. Его задача — дать воспроизводимые доказательства перед отдельным решением о первом stable GUI release.

## Автоматизированные проверки

| Команда | Что проверяет | Изоляция |
|---|---|---|
| `make smoke` | Raw-image BIOS и UEFI boot markers, persistent AHCI mount и automatic `[myos]$` entry. | Использует `myos.img`; не записывает пользовательские test files. |
| `make regression` | BIOS GUI note create/edit/save, native `.mya` source → `build` → `install` → `run`, затем UEFI remount/readback, persisted native run и clean GUI enter/exit. | Создаёт temporary copy `myos.img` и удаляет её после проверки. Рабочий image пользователя не изменяется. |

Перед запуском обе команды требуют QEMU, OVMF и `myos.img`; `make regression` использует fixed Q35 configuration and `-drive if=ide,format=raw`, because this is the supported persistent AHCI path.

```bash
make all img
make smoke
make regression
```

Ожидаемый итог:

```text
boot smoke: BIOS passed
boot smoke: UEFI passed
interactive regression: BIOS GUI/native workflow passed
interactive regression: UEFI persistence workflow passed
```

## Ручные release gates

Автоматизированные commands намеренно ограничены. До нового GUI release tag необходимо вручную подтвердить следующее:

| Gate | Требуемое evidence |
|---|---|
| Framebuffer visual check | Desktop, windows, pointer, focus, note editor and return to shell остаются readable in a graphical QEMU session. |
| Fresh persistent workflow | На fresh `myos.img` создать note и native package, затем отдельно перезагрузить guest и проверить оба результата. |
| Migration fixtures | MYPFS003→MYPFS004 и MYPFS002→MYPFS004 fixtures завершают recovery/migration и читаются после second clean mount. |
| Physical x86_64 PC | Boot от disposable USB, keyboard input, framebuffer output и clean poweroff/reboot smoke. Это нельзя заменить QEMU. |
| Release scope | Зафиксировать known limitations, release notes, artifact SHA-256 и exact immutable tag target. |

## Что не доказывают эти tests

`make smoke` and `make regression` do not establish networking, USB HID, SMP, production security, long-duration stress reliability, physical-hardware compatibility or a full native C toolchain. They are release-stabilization evidence for the currently implemented GUI, MYPFS004 and restricted native development workflow only.

## Publication rule

После того как automated и manual gates завершены, создание публичного Pre-release или release всё ещё требует отдельного explicit confirmation. Новый immutable tag нельзя retag or force-push; `main` не должен обновляться до отдельного merge decision.
