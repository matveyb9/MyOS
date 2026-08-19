# Visual verification of the framebuffer console

> **🌐 LANGUAGE / ЯЗЫК:** [🇷🇺 РУССКИЙ](framebuffer-visual-check_RU.md) / **🇺🇸 ENGLISH**

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [User Guide](USER_GUIDE.md), [Developer Guide](DEVELOPER_GUIDE.md) and the [documentation index](README.md).

The first BIOS QEMU screendump successfully confirmed that MyOS renders the boot log, the prompt `myos>` and the cursor in 8×8 raster glyphs on a dark framebuffer background. In that capture the `fbdemo` and `fbinfo` commands had not yet come into view: QMP commands to the virtual keyboard were processed asynchronously after the `screendump` request.

The next check should request a separate screendump after a delay to visually confirm scrolling and the current shell output. This is not a text-console bug: the serial journal has already confirmed the commands and the increased scroll counter.

## Confirmed BIOS frame

A repeat screendump after waiting for the QMP commands visually confirmed working scrolling: the screen contains the last lines `FB DEMO ROW`, then `myos> fbinfo` and the value `scrolls: 0x0000000000000008`. Monospaced 8×8 glyphs are legible, the dark background and light text are high-contrast, and the cursor is shown at the active shell prompt. This confirms that the text buffer, repaint after scroll and serial-to-framebuffer mirroring operate in the real QEMU framebuffer, not only in the serial log.
