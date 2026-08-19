# Framebuffer console validation for MyOS 0.7.0-dev

> **Language:** [English](framebuffer-validation.md) | [Русский](framebuffer-validation_RU.md)

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), [developer guide](DEVELOPER_GUIDE.md) and [documentation index](README.md).


## Implemented milestone

MyOS 0.7.0-dev adds a first on-screen text console on top of the framebuffer provided by Limine. The console accepts the same character stream as COM1, so the serial port remains available for headless QEMU and debugging, while the normal display shows the boot log and an interactive shell.

| Component | Implementation | Verified outcome |
|---|---|---|
| Format | Only 32-bit RGB Limine framebuffer; `bpp`, memory model and pitch are validated. | Unsupported formats do not receive writes to VRAM; COM1 remains active. |
| Geometry | 8×8 glyphs and a grid up to 160×100 cells. | On QEMU 1280×800 a 160×100 text cell grid was obtained. |
| Font | Built-in raster glyphs for ASCII letters, digits and basic punctuation. | Boot log, shell and hexadecimal values are readable in the screendump. |
| Output | `serial_write_char()` mirrors the character to the framebuffer after the COM1 write. | The same kernel log is visible on COM1 and on-screen. |
| Editing | Supports printable ASCII, CR, LF, Backspace and ignores current ANSI clear sequences. | PS/2 and COM1 input edit the shell line on the screen. |
| Scrolling | Line-shift of the character buffer, full repaint and a scroll counter. | `fbdemo` caused scrolling and `fbinfo` showed a non-zero counter. |
| Clearing | `clear` triggers a framebuffer clear and preserves ANSI clearing of the serial terminal. | The screen is cleared without rendering escape bytes as glyphs. |

## Validation matrix

| Scenario | BIOS QEMU Q35 | UEFI QEMU Q35 + OVMF | Result |
|---|---:|---:|---|
| Framebuffer initialization | Passed | Passed | `MYOS FRAMEBUFFER CONSOLE`, 160×100 text cells. |
| Visible boot log and cursor | Passed | Passed | QMP screendump shows readable 8×8 glyphs and the `myos>` cursor. |
| Shell with PS/2 IRQ1 | Passed via QMP `sendkey` | Passed via QMP `sendkey` | The virtual keyboard executed `fbdemo` and `fbinfo`. |
| Scrolling | Passed | Passed | The last `FB DEMO ROW` entries remain on screen; the scroll counter is non-zero. |
| PIT/IRQ0 after repaint | Passed | Passed | Timer and shell continued operating after a full repaint. |
| Parallel COM1 | Passed | Passed | Serial log confirms the same commands and values as the framebuffer. |

## Visual confirmation

| Image | Observation |
|---|---|
| `framebuffer-bios-after-demo.png` | Shows the final lines of the demo, `myos> fbinfo`, `scrolls: 0x8` and the current cursor. |
| `framebuffer-uefi.png` | The same raster console after OVMF, `scrolls: 0x16`; verification is independent of the BIOS path. |

> The pixel address calculation uses the row pitch, rather than assuming `width * bytes_per_pixel`: framebuffer lines may have padding. The basic formula is `address + y * pitch + x * pixelwidth`. [1]

## Limitations

The implementation is intentionally small: only printable 7-bit ASCII and basic control characters; no Unicode, wide glyphs, VT100, mouse, hardware cursor, double buffering, GPU acceleration or mode switching. Scrolling repaints the entire grid and is suitable for an early console, but will later be optimized with rectangle blits or a retained compositor.

## Reproduction

```bash
cd /home/ubuntu/myos
make run-graphic
# либо
make run-uefi-graphic
```

In the terminal attached to COM1 run `fbinfo`, `fbdemo`, `fbinfo` and `clear`. The QEMU graphical window will show the console; `fbdemo` should increase the scroll counter.

## References

[1]: https://wiki.osdev.org/Drawing_In_a_Linear_Framebuffer "OSDev Wiki — Drawing in a Linear Framebuffer"
[2]: https://github.com/limine-bootloader/limine-protocol "Limine Boot Protocol"
