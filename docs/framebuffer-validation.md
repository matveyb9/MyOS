# Проверка framebuffer-консоли MyOS 0.7.0-dev
> **Исторический документ.** Этот файл описывает ранний development milestone и не является спецификацией текущего console release `0.12.0-dev`. Сверяйтесь с [руководством пользователя](USER_GUIDE_RU.md), [руководством разработчика](DEVELOPER_GUIDE_RU.md) и [индексом документации](README.md).


## Реализованный рубеж

MyOS 0.7.0-dev добавляет первую экранную текстовую консоль поверх framebuffer, предоставленного Limine. Консоль принимает тот же поток символов, что и COM1, поэтому serial остаётся доступным для headless QEMU и отладки, а обычный экран показывает журнал загрузки и интерактивную shell.

| Компонент | Реализация | Проверяемый результат |
|---|---|---|
| Формат | Только 32-bit RGB Limine framebuffer; проверяются `bpp`, memory model и pitch. | Неподдерживаемый формат не получает записи в VRAM, COM1 остаётся активен. |
| Геометрия | 8×8 glyph и сетка до 160×100 ячеек. | В QEMU 1280×800 получены 160×100 text cells. |
| Шрифт | Встроенные растровые glyph для ASCII letters, digits и базовой punctuation. | Загрузочный журнал, shell и hexadecimal значения читаются на screendump. |
| Output | `serial_write_char()` зеркалирует символ в framebuffer после COM1 write. | Один и тот же kernel log виден в COM1 и на экране. |
| Редактирование | Поддержаны printable ASCII, CR, LF, Backspace и игнорирование текущих ANSI clear sequences. | Ввод PS/2 и COM1 редактирует строку shell на экране. |
| Прокрутка | Сдвиг символьного буфера на строку, полный repaint и scroll counter. | `fbdemo` вызвал scroll и `fbinfo` показал ненулевой счётчик. |
| Очистка | `clear` вызывает framebuffer clear и сохраняет ANSI очистку serial terminal. | Экран очищается без вывода escape bytes как glyph. |

## Матрица проверки

| Сценарий | BIOS QEMU Q35 | UEFI QEMU Q35 + OVMF | Результат |
|---|---:|---:|---|
| Инициализация framebuffer | Пройдено | Пройдено | `MYOS FRAMEBUFFER CONSOLE`, 160×100 text cells. |
| Видимый boot log и cursor | Пройдено | Пройдено | QMP screendump показывает читаемые 8×8 glyph и `myos>` cursor. |
| Shell с PS/2 IRQ1 | Пройдено через QMP `sendkey` | Пройдено через QMP `sendkey` | Виртуальная клавиатура выполнила `fbdemo` и `fbinfo`. |
| Прокрутка | Пройдено | Пройдено | Последние `FB DEMO ROW` остаются на экране; счётчик scroll ненулевой. |
| PIT/IRQ0 после repaint | Пройдено | Пройдено | Таймер и shell продолжили работу после полной перерисовки. |
| Parallel COM1 | Пройдено | Пройдено | Serial log подтверждает те же команды и значения, что framebuffer. |

## Визуальное подтверждение

| Образ | Наблюдение |
|---|---|
| `framebuffer-bios-after-demo.png` | Видны последние строки demo, `myos> fbinfo`, `scrolls: 0x8` и текущий cursor. |
| `framebuffer-uefi.png` | Та же raster-консоль после OVMF, `scrolls: 0x16`; проверка независима от BIOS пути. |

> Расчёт адреса пикселя использует row pitch, а не предполагает `width * bytes_per_pixel`: строки framebuffer могут иметь padding. Базовая формула — `address + y * pitch + x * pixelwidth`. [1]

## Ограничения

Реализация намеренно небольшая: только printable 7-bit ASCII и базовые управляющие символы; нет Unicode, широких glyph, VT100, мыши, аппаратного cursor, двойной буферизации, GPU acceleration или смены режима. Прокрутка перерисовывает всю сетку и подходит для ранней консоли, но позднее будет оптимизирована rectangle blit или retained compositor.

## Повторение

```bash
cd /home/ubuntu/myos
make run-graphic
# либо
make run-uefi-graphic
```

В terminal с COM1 выполните `fbinfo`, `fbdemo`, `fbinfo` и `clear`. Графическое окно QEMU покажет консоль; `fbdemo` должен увеличить scroll counter.

## References

[1]: https://wiki.osdev.org/Drawing_In_a_Linear_Framebuffer "OSDev Wiki — Drawing in a Linear Framebuffer"
[2]: https://github.com/limine-bootloader/limine-protocol "Limine Boot Protocol"
