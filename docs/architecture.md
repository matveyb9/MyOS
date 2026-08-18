# Архитектура MyOS 0.7.0-dev

> **Исторический документ.** Он описывает milestone `0.7.0-dev`, а не текущий console release `0.12.0-dev`. Для актуальной архитектуры используйте [руководство разработчика](DEVELOPER_GUIDE_RU.md).

## Назначение

MyOS — учебно-практическое ядро собственной разработки для **x86_64**. Гибридный ISO-образ запускается через BIOS и UEFI в QEMU. Проект развивает сначала наблюдаемое ring 0-ядро, затем подготовит изолированные процессы, файловую систему и графическую среду. Версия 0.7.0-dev впервые показывает интерактивный текст не только через COM1, но и на framebuffer.

> Limine подготавливает окружение запуска и публикует boot responses, однако консоль, память, IDT, драйверы и будущие пользовательские подсистемы принадлежат MyOS. [1]

## Слои системы

| Слой | Каталог | Ответственность в 0.7.0-dev | Следующая граница |
|---|---|---|---|
| Загрузка | `boot/` | Higher-half ELF, Limine requests, BIOS/UEFI menu. | `boot_info` и явный ownership boot-областей. |
| Архитектура | `kernel/arch/x86_64/` | GDT, IDT, CR2, Local APIC virtual-wire, ASM stubs. | Guard policy, APIC timer, IOAPIC и SMP. |
| IRQ и ввод | `kernel/irq/`, `kernel/drivers/` | PIC 8259A, PIT 100 Hz, PS/2 Set 1 и keyboard ring buffer. | USB HID, extended keyboard layout, device queues. |
| Физическая память | `kernel/mm/pmm.c` | Bitmap usable frames и checked allocate/reserve/free. | Ownership tags, >4 GiB и NUMA. |
| Виртуальная память | `kernel/mm/paging.c` | Собственный PML4, CR3 switch, 4 KiB mapping, APIC MMIO. | `unmap`, NX, supervisor/user mappings. |
| Heap | `kernel/mm/heap.c` | Free list, split/coalesce, `kmalloc` и `kfree`. | Locks, guard zones и page reclamation. |
| Serial console | `kernel/console/serial.c` | COM1 output/input, headless journal. | Panic-safe minimal fallback. |
| Framebuffer console | `kernel/console/framebuffer.c` | 8×8 raster, cell buffer, cursor, scroll и COM1 mirroring. | Unicode, optimized scroll, double buffering и UI primitives. |
| Shell | `kernel/console/shell.c` | Один input path для COM1 и PS/2; команды observability. | History, completion, user processes. |

## Output model

Serial остаётся первой точкой загрузочного журнала. После принятия 32-bit RGB Limine framebuffer `framebuffer_console_init()` создаёт символьную сетку, чистит background и включает second sink. Каждый последующий `serial_write_char()` отправляет байт в COM1, затем — в `framebuffer_console_putc()`. Это deliberately keeps headless logging even if graphical mode unavailable.

| Input/character | Serial | Framebuffer |
|---|---|---|
| Printable ASCII | COM1 transmit. | Glyph на текущей cell. |
| `\n` | Добавляет `\r` перед LF. | Переход на строку и scroll при нижней границе. |
| Backspace | Печатает `\b`, space, `\b`. | Очищает предыдущую cell. |
| ANSI clear | Передаётся terminal. | Escape sequence не рисуется; shell вызывает явный clear. |
| Неподдерживаемый UTF-8 byte | Передаётся как raw COM1 output. | Игнорируется до появления Unicode. |

## Framebuffer implementation

MyOS принимает только Limine RGB framebuffer с `bpp == 32`, проверенным memory model и pitch, достаточным для ширины. Пиксель адресуется через `pixels[y * pixels_per_row + x]`, где `pixels_per_row = pitch / 4`. Это учитывает possible padding между строками framebuffer. [2]

| Свойство | Текущая политика |
|---|---|
| Цвета | Dark navy background, off-white text, cyan cursor/accent. |
| Шрифт | Встроенный 5×7 bitmap, расположен в 8×8 cell. |
| Размер | До 160 columns × 100 rows; QEMU 1280×800 использует максимум 160×100. |
| Скролл | Сдвиг cell buffer на строку и complete repaint. |
| Cursor | Accent underline в текущей cell. |
| Очистка | Полная очистка cells и pixels; top accent line сохраняется. |

## Memory and fault policy

PMM поддерживает пригодные и свободные bitmap, heap повторно использует blocks, а vector 14 сохраняет CR2 до вывода и делает fail-stop. Эти гарантии остались необходимыми после добавления framebuffer: rendering не требует динамических allocation и не зависит от файловой системы, поэтому уже доступно в раннем bootstrap после paging.

## Visual verification

BIOS и UEFI QEMU проверялись через QMP `screendump` после ввода `fbdemo`/`fbinfo` виртуальной PS/2-клавиатурой. Снимки показывают readable glyphs, latest demo rows, cursor и ненулевой scroll counter. Serial logs independently reproduce the same shell commands and values.

## Следующая техническая итерация

Далее MyOS должен явным образом резервировать kernel and boot physical pages, добавить `unmap` и guard ranges для виртуальной памяти. После укрепления address ownership framebuffer-console может получить небольшой line editor, history и контрастный status bar, не меняя базовый serial fallback.

## References

[1]: https://github.com/limine-bootloader/limine-protocol "Limine Boot Protocol"
[2]: https://wiki.osdev.org/Drawing_In_a_Linear_Framebuffer "OSDev Wiki — Drawing in a Linear Framebuffer"
