# Модель framebuffer-консоли MyOS 0.7.0-dev

> **Язык:** [English](framebuffer-console-model.md) | [Русский](framebuffer-console-model_RU.md)

> **Исторический документ.** Этот файл описывает ранний development milestone и не является спецификацией текущего console release `0.12.0-dev`. Сверяйтесь с [руководством пользователя](USER_GUIDE_RU.md), [руководством разработчика](DEVELOPER_GUIDE_RU.md) и [индексом документации](README_RU.md).


## Цель

MyOS уже получает Limine framebuffer и умеет заполнять фон, но весь полезный текст остаётся в COM1. В 0.7.0-dev новый модуль `kernel/console/framebuffer.c` станет вторым sink для сообщений kernel и shell: serial сохраняется для отладки, а экранная консоль делает команды видимыми на обычном мониторе.

| Решение | Выбор для первого этапа | Причина |
|---|---|---|
| Pixel format | Только 32-bit RGB framebuffer, предоставленный Limine. | Текущий QEMU-путь уже публикует 32-bit RGB; отказ от неизвестного формата безопаснее неправильной записи VRAM. |
| Адрес пикселя | `address + y * pitch + x * bytes_per_pixel`. | `pitch` задаёт расстояние между строками и не обязан совпадать с `width * pixelwidth`. [2] |
| Шрифт | Встроенный моноширинный 8×8 ASCII raster. | Не требует файловой системы, heap или внешних активов. |
| Представление | Символьная сетка плюс repaint нужных glyph. | Корректно обрабатывает новую строку, backspace и прокрутку без полного framebuffer redraw. |
| Цвета | Тёмный navy background, off-white text, cyan prompt/accent. | Контрастная минимальная палитра для диагностического ядра. |
| Прокрутка | Сдвиг строк текстового буфера и полный redraw сетки. | Простая предсказуемая реализация до оптимизации GPU/rect blit. |
| Serial | Остаётся активным параллельно. | Поддерживает headless QEMU, GDB и диагностику до framebuffer init. |

## Ограничения

На первом этапе поддерживаются printable 7-bit ASCII, `\n`, `\r`, `\b` и ANSI clear, используемый текущей shell. Нет Unicode, VT100 эмуляции, мыши, выбора текста, смены видеорежима, прозрачности или аппаратного ускорения.

> Framebuffer — линейная область памяти: каждая запись пикселя непосредственно изменяет изображение. После включения paging адрес framebuffer должен быть доступен ядру; Limine предоставил рабочее bootstrap mapping, которое MyOS сохраняет при создании собственного PML4. [1] [2]

## References

[1]: https://github.com/limine-bootloader/limine-protocol "Limine Boot Protocol"
[2]: https://wiki.osdev.org/Drawing_In_a_Linear_Framebuffer "OSDev Wiki — Drawing in a Linear Framebuffer"
