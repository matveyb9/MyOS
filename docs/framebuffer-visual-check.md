# Visual check of the framebuffer console

<p align="center">
  <a href="framebuffer-visual-check_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Consult the [user guide](USER_GUIDE.md), [developer guide](DEVELOPER_GUIDE.md) and [documentation index](README.md).


The initial BIOS QEMU screendump successfully confirmed that MyOS renders the boot log, the `myos>` prompt, and the cursor in 8×8 raster glyphs on a dark framebuffer background. In this capture the `fbdemo` and `fbinfo` commands had not yet appeared: QMP commands to the virtual keyboard were processed asynchronously after the `screendump` request.

The next check should request a separate screendump after a delay to visually confirm scrolling and the current shell output. This is not a text console bug: the serial journal has already confirmed the commands and the increased scroll counter.

## Confirmed BIOS frame

A repeated screendump after waiting for the QMP commands visually confirmed scrolling in operation: the screen contains the last lines `FB DEMO ROW`, followed by `myos> fbinfo` and the value `scrolls: 0x0000000000000008`. The monospaced 8×8 glyphs are legible, the dark background and light text are high-contrast, and the cursor is displayed at the active shell prompt. This confirms that the text buffer, repaint after scroll, and serial-to-framebuffer mirroring work in the real QEMU framebuffer, not only in the serial log.
