# MyOS

**MyOS** — экспериментальная 64-битная операционная система для архитектуры x86_64, написанная с нуля на C11 и небольшом количестве x86_64-ассемблера. Разработка начинается с консольного ядра, проверяемого в QEMU, и целенаправленно создаёт фундамент для будущей графической среды.

> На версии `0.1.0` внешним компонентом является только загрузчик Limine. Он передаёт управление собственному ELF-ядру; все подсистемы ядра — консоль, прерывания, память, драйверы, процессы и графика — будут развиваться в этом репозитории.

## Текущий контрольный результат

Ядро запрашивает у загрузчика framebuffer, карту физической памяти, сведения о прошивке и загрузчике. После настройки GDT, IDT, PMM и собственного PML4 MyOS создаёт новые 4 KiB отображения через PML4/PDPT/PD/PT walker. PMM предоставляет проверяемые reserve/free операции, free-list heap повторно использует освобождённые блоки, а page fault vector 14 сохраняет CR2 и декодирует error code. Новая framebuffer-консоль принимает тот же output, что и COM1: встроенный 8×8 ASCII raster, символьная сетка, прокрутка, курсор и безопасная обработка текущего 32-bit RGB формата Limine. Shell доступна одновременно через COM1 и PS/2-клавиатуру; команды `fbinfo`, `fbdemo` и `clear` проверяют геометрию, прокрутку и очистку экранной консоли. Это всё ещё ring 0-прототип: нет Unicode, demand paging, user space, USB HID, IOAPIC или оконной системы.

| Компонент | Решение в версии 0.7.0-dev | Причина |
|---|---|---|
| Архитектура | x86_64, System V ABI, higher-half ELF-ядро. | Современная базовая платформа для QEMU, UEFI-прошивок и будущего GUI. |
| Защита CPU | Собственные GDT и IDT; диагностические обработчики 32 CPU-исключений. | Ранние ошибки теперь останавливают ядро с номером вектора, кодом и RIP. |
| Физическая память | Bitmap до 4 GiB с отдельным учётом пригодных кадров; проверяемые `reserve/free`. | Кадры для PML4, mapper и heap не смешиваются со служебной картой загрузчика. |
| Виртуальная память | Собственный PML4, загрузка CR3, 4 KiB walker и `invlpg` для новых отображений. | MyOS управляет собственными изменениями виртуальной памяти, сохраняя рабочие bootstrap-карты. [6] [7] |
| Kernel heap | 1 GiB supervisor-only free-list heap с 16-байтным выравниванием, разделением и слиянием блоков. | Выделения можно возвращать и повторно использовать без роста virtual frontier. |
| Page fault | Vector 14 сохраняет CR2 до вывода и декодирует error code. | Ошибка отображения содержит адрес, тип доступа и режим привилегий. [8] |
| Внешние IRQ | PIC 8259A с ремаппингом `0x20–0x2F`, Local APIC virtual-wire и EOI для обоих контроллеров. | PIT и клавиатура надёжно поступают в QEMU с включённым APIC. [3] [4] |
| Таймер и ввод | PIT IRQ0 100 Гц; PS/2 Set 1 IRQ1, US QWERTY и буфер 255 символов. | Shell пробуждается аппаратным событием, а не только serial polling. [3] [5] |
| Языки | C11 в freestanding-режиме; NASM для портов ввода-вывода и остановки CPU. | Код ядра остаётся близким к аппаратуре, но не становится полностью ассемблерным. |
| Загрузка | Limine и его native boot protocol. | Один образ поддерживает запуск в BIOS и UEFI, не добавляя на раннем этапе собственный сложный загрузчик. [1] |
| Экранная консоль | 8×8 raster, 160×100 text cells при QEMU 1280×800, scroll, cursor и parallel COM1 output. | Пользователь видит shell на обычном экране, не теряя headless-диагностику. [2] [9] |
| Диагностика | COM1 (0x3F8), framebuffer screenshots через QMP и shell-команды `fbinfo`/`fbdemo`. | Визуальный результат проверяется независимо от serial-журнала. |
| Тестирование | QEMU Q35, затем выделенный USB-накопитель. | Ошибки ядра сначала изолируются в эмуляторе. |

## Быстрый запуск

Сначала соберите гибридный загрузочный ISO-образ. При первой сборке процесс автоматически получает официальный бинарный пакет Limine, собирает его небольшую хост-утилиту и сохраняет его в `third_party/limine-binary/`.

```bash
cd /home/ubuntu/myos
make
make run
```

Команда `make run` стартует QEMU в BIOS-режиме без графического окна и выводит COM1 в текущий терминал. Ожидаемый результат содержит строку `MyOS 0.7.0-dev — x86_64 kernel`, несколько сообщений `[ok]` и приглашение `myos>`. Наберите `help`, чтобы увидеть доступные встроенные команды.

Для проверки UEFI необходим пакет OVMF. После его установки запуск выполняется так:

```bash
make run-uefi
```

Дополнительные цели описаны ниже.

| Команда | Назначение |
|---|---|
| `make` или `make iso` | Собрать гибридный BIOS/UEFI ISO-образ `myos.iso`. |
| `make run` | Собрать и запустить ISO в QEMU с BIOS в headless serial-режиме. |
| `make run-graphic` | Запустить BIOS QEMU с графическим окном framebuffer и COM1 в терминале. |
| `make run-uefi` | Собрать и запустить ISO в QEMU с UEFI/OVMF в headless serial-режиме. |
| `make run-uefi-graphic` | Запустить UEFI/OVMF QEMU с графическим окном framebuffer и COM1 в терминале. |
| `make hdd` | Сформировать сырой образ `myos.hdd` для выделенного тестового носителя. |
| `make debug` | Запустить QEMU остановленным и открыть GDB-сервер на TCP-порту 1234. |
| `make inspect` | Показать ELF-заголовки, program headers и секции ядра. |
| `make clean` | Удалить результаты сборки, сохранив загрузочную зависимость. |

## Отладка

В одном терминале выполните `make debug`. В другом терминале откройте отладчик:

```bash
gdb build/kernel.elf
(gdb) target remote :1234
(gdb) break kmain
(gdb) continue
```

## Дорожная карта

| Версия | Содержание | Критерий завершения |
|---|---|---|
| `0.1.0` | Загрузка, COM1, framebuffer и карта памяти. | ISO запускается с BIOS и UEFI QEMU и стабильно выводит журнал. |
| `0.2.0-dev` | Интерактивная serial-shell: `help`, `version`, `meminfo`, `firmware`, `echo`, `clear`, `halt`. | Команды принимаются и выполняются через COM1 в BIOS и UEFI QEMU. |
| `0.3.0-dev` | GDT, IDT, 32 обработчика исключений и bitmap physical memory manager. | `crash` формирует понятный divide-error; `alloc` возвращает доступный физический кадр. |
| `0.4.0-dev` | PIC/APIC virtual-wire, PIT/таймер, IRQ-диспетчер, точечный MMIO paging и PS/2 Set 1. | В QEMU BIOS и UEFI растут IRQ0 ticks, а `sendkey` доставляет команды через IRQ1. |
| `0.5.0-dev` | Собственный PML4, CR3 switch, 4 KiB mapper, MMIO policy и kernel heap. | BIOS и UEFI QEMU выполняют `heaptest`; `paging` показывает управляемый PML4 и Local APIC mapping. |
| `0.6.0-dev` | PMM reserve/free, free-list heap и page-fault diagnostics. | BIOS и UEFI QEMU проходят `pmmtest` и `heaptest`; `pagefault` печатает CR2 plus decoded cause. |
| `0.7.0-dev` | 8×8 framebuffer-консоль, scroll, cursor, `fbinfo` и `fbdemo`. | BIOS и UEFI QEMU screendump показывает читаемый shell и подтверждённую прокрутку. |
| `0.8.0-dev` | Резервирование страниц ядра, guard policy и user-space address-region plan. | Ядро сможет безопасно отличать системные и будущие пользовательские отображения. |
| `0.9.0-dev` | Планировщик, ring 3, системные вызовы, ELF и initramfs. | Минимальная пользовательская программа выполняется из initramfs. |
| `1.0.0` | VFS и простая постоянная файловая система. | Система монтирует файлы и запускает программу по имени. |
| `1.1.0` | 2D-драйвер framebuffer, мышь, окна и композитор. | Демонстрационная графическая оболочка запускается поверх стабильного ядра. |

## Безопасное тестирование на реальном ПК

Физический запуск будет выполняться только после стабильных QEMU-тестов. Для этого нужен отдельный USB-носитель, не содержащий ценных данных: операция записи образа полностью перезапишет выбранное устройство. Не следует устанавливать MyOS на системный диск, рассчитывать на Secure Boot или тестировать раннее ядро без возможности отключить питание.

## Лицензирование

Лицензия самого MyOS будет выбрана до появления пользовательских программ и сторонних библиотек. Заголовок Limine Protocol распространяется с разрешительной лицензией 0BSD; сведения о загрузчике и его возможностях приведены в официальном репозитории. [1] [2]

## References

[1]: https://github.com/limine-bootloader/limine "Limine — официальный репозиторий загрузчика"
[2]: https://github.com/limine-bootloader/limine-protocol "Limine Boot Protocol — официальная спецификация и заголовок"
[3]: https://wiki.osdev.org/8259_PIC "OSDev Wiki — 8259 PIC"
[4]: https://wiki.osdev.org/APIC "OSDev Wiki — APIC"
[5]: https://wiki.osdev.org/PS/2_Keyboard "OSDev Wiki — PS/2 Keyboard"
[6]: https://wiki.osdev.org/X86_Paging "OSDev Wiki — X86 Paging"
[7]: https://docs.kernel.org/mm/page_tables.html "Linux kernel documentation — Page Tables"
[8]: https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-227.html "Intel SDM Vol. 3A — Page-Fault Error Code and CR2"
[9]: https://wiki.osdev.org/Drawing_In_a_Linear_Framebuffer "OSDev Wiki — Drawing in a Linear Framebuffer"
