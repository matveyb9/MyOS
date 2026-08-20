# Framebuffer console validation for MyOS 0.7.0-dev

<p align="center">
  <a href="framebuffer-validation_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), [developer guide](DEVELOPER_GUIDE.md) and [documentation index](README.md).


## Implemented milestone

MyOS 0.7.0-dev adds the first on-screen text console on top of the Limine-provided framebuffer. The console accepts the same character stream as COM1, so the serial port remains available for headless QEMU and debugging while the normal screen shows the boot log and an interactive shell.

| Компонент | Реализация | Проверяемый результат |
|---|---|---|
| Формат | Только 32-bit RGB Limine framebuffer; проверяются `bpp`, memory model и pitch. | Неподдерживаемый формат не получает записи в VRAM, COM1 остаётся активен. |
| Геометрия | 8×8 glyph и сетка до 160×100 ячеек. | В QEMU 1280×800 получены 160×100 text cells. |
| Шрифт | Встроенные растровые glyph для ASCII letters, digits и базовой punctuation. | Загрузочный журнал, shell и hexadecimal значения читаются на screendump. |
| Output | `serial_write_char()` зеркалирует символ в framebuffer после COM1 write. | Один и тот же kernel log виден в COM1 и на экране. |
| Редактирование | Поддержаны printable ASCII, CR, LF, Backspace и игнорирование текущих ANSI clear sequences. | Ввод PS/2 и COM1 редактирует строку shell на экране. |
| Прокрутка | Сдвиг символьного буфера на строку, полный repaint и scroll counter. | `fbdemo` вызвал scroll и `fbinfo` показал ненулевой счётчик. |
| Очистка | `clear` вызывает framebuffer clear и сохраняет ANSI очистку serial terminal. | Экран очищается без вывода escape bytes как glyph. |

(Notes: Table headers and code identifiers are preserved exactly as in the original.)

## Validation matrix

| Сценарий | BIOS QEMU Q35 | UEFI QEMU Q35 + OVMF | Результат |
|---|---:|---:|---|
| Инициализация framebuffer | Пройдено | Пройдено | `MYOS FRAMEBUFFER CONSOLE`, 160×100 text cells. |
| Видимый boot log и cursor | Пройдено | Пройдено | QMP screendump показывает читаемые 8×8 glyph и `myos>` cursor. |
| Shell с PS/2 IRQ1 | Пройдено через QMP `sendkey` | Пройдено через QMP `sendkey` | Виртуальная клавиатура выполнила `fbdemo` и `fbinfo`. |
| Прокрутка | Пройдено | Пройдено | Последние `FB DEMO ROW` остаются на экране; счётчик scroll ненулевой. |
| PIT/IRQ0 после repaint | Пройдено | Пройдено | Таймер и shell продолжили работу после полной перерисовки. |
| Parallel COM1 | Пройдено | Пройдено | Serial log подтверждает те же команды и значения, что framebuffer. |

## Visual confirmation

| Образ | Наблюдение |
|---|---|
| `framebuffer-bios-after-demo.png` | Shows the last lines of the demo, `myos> fbinfo`, `scrolls: 0x8` and the current cursor. |
| `framebuffer-uefi.png` | The same raster console after OVMF, `scrolls: 0x16`; verification is independent of the BIOS path. |

> Pixel address calculation uses the row pitch rather than assuming `width * bytes_per_pixel`: framebuffer rows may have padding. Base formula — `address + y * pitch + x * pixelwidth`. [1]

## Limitations

The implementation is intentionally small: only printable 7-bit ASCII and basic control characters; no Unicode, wide glyphs, VT100, mouse, hardware cursor, double buffering, GPU acceleration or mode switching. Scrolling repaints the entire grid and is suitable for an early console, but will later be optimized with rectangle blit or a retained compositor.

## Reproduction

```bash
cd /home/ubuntu/myos
make run-graphic
# либо
make run-uefi-graphic
```

In the terminal on COM1 run `fbinfo`, `fbdemo`, `fbinfo` and `clear`. The QEMU graphical window will show the console; `fbdemo` should increase the scroll counter.

## References

[1]: https://wiki.osdev.org/Drawing_In_a_Linear_Framebuffer "OSDev Wiki — Drawing in a Linear Framebuffer"
[2]: https://github.com/limine-bootloader/limine-protocol "Limine Boot Protocol"
