# GUI bring-up: native framebuffer desktop

Этот документ описывает **экспериментальный GUI** только для ветки `gui/bringup`. Он не является частью console release `v0.12.0-console` и не меняет смысл `main` или `console-stable`.

## Запуск

Сначала переключитесь на GUI branch и соберите raw image:

```bash
git switch gui/bringup
make all img
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

После kernel prompt введите:

```text
init
startgui
```

`startgui` — обычная ring-3 user program. Она открывает bounded GUI session through the existing syscall boundary. `Q` or `Esc` exits the graphical session and returns to the same user shell.

## What is implemented

| Component | Current behavior |
|---|---|
| Renderer | Native direct drawing into the Limine RGB framebuffer; no web runtime or external graphical toolkit. |
| Session ownership | One GUI owner at a time; kernel rejects a second simultaneous session. |
| Desktop | Dark desktop, top bar and bottom control bar. |
| Windows | Three bounded window records: `SYSTEM`, `NOTES` and `MONITOR`. |
| Z-order | Focused window is raised to the front; renderer redraws bounded composited state. |
| Focus | `Tab`, `Enter` or `Space` cycles visible windows. |
| Pointer | `W`, `A`, `S`, `D` moves a bounded crosshair pointer. |
| Pointer focus | `F` focuses and raises the highest visible window under the pointer. |
| Visibility | `1`, `2`, `3` toggle `SYSTEM`, `NOTES`, `MONITOR`; `X` hides/reopens the focused window without allowing all windows to disappear. |
| Reset | `R` restores the default visible window layout and z-order. |
| Exit | `Q` or `Esc` ends session, restores framebuffer text console and returns to shell. |

## Architecture boundaries

The implementation intentionally remains small. `kernel/console/framebuffer.c` owns the drawing primitives, GUI surface records, z-order array, pointer state and keyboard-event handling. `user/startgui.c` owns the ring-3 GUI session loop. The syscall dispatcher retains session-owner validation and cleanup if the owner exits or is killed.

| Limit | Current value / policy |
|---|---|
| Window records | 3 static bounded records. |
| Allocation | No dynamic GUI allocation. |
| Input source | Existing scheduler-safe console input path. |
| Rendering policy | Full redraw after a bounded GUI event. |
| Application data | `NOTES` is a demo surface only; it does not yet load or modify user files. |
| Mouse hardware | Not implemented; keyboard moves a visual pointer. |
| General window API | Not implemented; window records remain internal to framebuffer renderer. |

## Validation expectations

A GUI change should pass the following baseline before commit.

| Test | Expected result |
|---|---|
| `make all img` | Strict warning-free build. |
| BIOS QEMU | `startgui` renders desktop; focus/visibility/pointer controls work; `Q` returns to shell. |
| UEFI/OVMF | GUI launches and returns to shell without losing user input ownership. |
| Screenshot | At least one framebuffer capture is checked after an interaction that changes state. |
| Console boundary | After GUI exit, `echo gui-returned` works in the original `myos$` shell. |

The next GUI milestones should move toward event abstractions, a reusable surface contract and small file-aware graphical applications. They must preserve the separate GUI branch until an explicit merge/release decision.
