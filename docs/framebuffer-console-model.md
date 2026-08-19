# Framebuffer Console Model for MyOS 0.7.0-dev

> **Language:** [English](framebuffer-console-model.md) | [Русский](framebuffer-console-model_RU.md)

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), [developer guide](DEVELOPER_GUIDE.md) and [documentation index](README.md).


## Goal

MyOS already obtains the Limine framebuffer and can fill the background, but all useful text still ends up on COM1. In 0.7.0-dev the new module `kernel/console/framebuffer.c` will become a second sink for kernel and shell messages: serial is retained for debugging, while the on-screen console makes commands visible on a regular monitor.

| Decision | Choice for the first stage | Reason |
|---|---|---|
| Pixel format | Only 32-bit RGB framebuffer provided by Limine. | The current QEMU path already publishes 32-bit RGB; rejecting an unknown format is safer than writing VRAM incorrectly. |
| Pixel address | `address + y * pitch + x * bytes_per_pixel`. | `pitch` specifies the distance between rows and is not required to equal `width * pixelwidth`. [2] |
| Font | Built-in monospaced 8×8 ASCII raster. | Does not require a filesystem, heap, or external assets. |
| Representation | Character grid plus repaint of needed glyphs. | Correctly handles newline, backspace and scrolling without a full framebuffer redraw. |
| Colors | Dark navy background, off-white text, cyan prompt/accent. | High-contrast minimal palette for a diagnostic kernel. |
| Scrolling | Shift lines in the text buffer and full redraw of the grid. | Simple, predictable implementation until GPU/rect blit optimization. |
| Serial | Remains active in parallel. | Supports headless QEMU, GDB and diagnostics prior to framebuffer init. |

## Limitations

In the first stage supported are printable 7-bit ASCII, `\n`, `\r`, `\b` and the ANSI clear used by the current shell. No Unicode, VT100 emulation, mouse, text selection, video mode switching, transparency or hardware acceleration.

> Framebuffer — a linear region of memory: each pixel write immediately changes the image. After paging is enabled the framebuffer address must be accessible to the kernel; Limine provided a working bootstrap mapping which MyOS preserves when creating its own PML4. [1] [2]

## References

[1]: https://github.com/limine-bootloader/limine-protocol "Limine Boot Protocol"
[2]: https://wiki.osdev.org/Drawing_In_a_Linear_Framebuffer "OSDev Wiki — Drawing in a Linear Framebuffer"
