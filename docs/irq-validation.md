# Проверка IRQ и PS/2 в MyOS 0.4.0-dev
> **Исторический документ.** Этот файл описывает ранний development milestone и не является спецификацией текущего console release `0.12.0-dev`. Сверяйтесь с [руководством пользователя](USER_GUIDE_RU.md), [руководством разработчика](DEVELOPER_GUIDE_RU.md) и [индексом документации](README.md).


## Что было проверено

MyOS 0.4.0-dev использует legacy PIC как контроллер источников IRQ, но доставляет его сигнал через Local APIC LINT0 в режиме **ExtINT virtual-wire**. Это необходимо в обычной APIC-конфигурации QEMU: PIC/PIT работает с отключённым APIC, но без Local APIC маршрут не достигает CPU. Local APIC MMIO отображается только в одной некэшируемой странице `0xFFFFFFFFC0000000`; этот ранний mapper не является полноценной системой виртуальной памяти.

| Проверка | BIOS QEMU Q35 | UEFI QEMU Q35 + OVMF | Результат |
|---|---:|---:|---|
| Загрузка ISO | Пройдено | Пройдено | Ядро сообщает `Local APIC virtual wire: enabled`. |
| PIT IRQ0 | Пройдено | Пройдено | `ticks` увеличивается при частоте около 100 Гц. |
| PIC mask | Пройдено | Пройдено | Маска `0xFFFC`: разрешены только IRQ0 и IRQ1. |
| PS/2 scanning | Пройдено | Пройдено | Драйвер получил ACK на `0xF4` и снял маску IRQ1. |
| QMP `sendkey` | Пройдено | Пройдено | `help`, `keyboard`, `halt` набраны только через виртуальную PS/2-клавиатуру. |
| Keyboard IRQ counter | Пройдено | Пройдено | Счётчик IRQ1 вырос; переполнений кольцевого буфера нет. |

> Для теста клавиши инъецировались командой QEMU HMP `sendkey` через QMP-сокет. Это проверяет путь «виртуальная PS/2-клавиатура → PIC IRQ1 → Local APIC → IDT → IRQ dispatcher → keyboard ring buffer → shell», а не COM1-serial input.

## Наблюдаемые результаты

В BIOS-сценарии счётчик PIT вырос приблизительно от `0xA4` до `0x16C` за двухсекундный интервал. В UEFI-сценарии он вырос от `0xD01` до `0x118D` между двумя запросами `ticks`. Команда `keyboard` в UEFI показала `IRQ1` count `0x2A` и `dropped characters: 0x0` после ввода команд `ticks`, `keyboard` и `halt`.

| Подсистема | Реализованное поведение | Явное ограничение |
|---|---|---|
| PIC 8259A | Remap `0x20–0x2F`, маскирование линий, EOI master/slave. | Нет корректной логики spurious IRQ7/IRQ15. |
| Local APIC | LINT0 ExtINT, TPR 0, software-enable через SVR, LAPIC EOI. | Нет Local APIC timer, IPI, SMP или IOAPIC. |
| PIT | Channel 0, rate generator, около 100 Гц. | PIT устаревший и не является долговременным источником времени. |
| PS/2 | Enable-scanning `0xF4`, ACK/Resend, Set 1 US QWERTY, Shift, Backspace, Enter. | Нет USB HID, Caps Lock, extended keys, командной очереди или раскладок. |
| Shell | Опрос serial и keyboard-буфера; `hlt` без готового ввода. | Экранная текстовая консоль появится только в версии 0.6.0-dev. |

## Повторение теста

Стандартные быстрые проверки сохраняются:

```bash
cd /home/ubuntu/myos
make
make run
make run-uefi
```

Для автоматизированного аппаратного ввода QEMU должен быть запущен с QMP-сокетом и serial log. Затем QMP получает `qmp_capabilities`, после чего команды вида `human-monitor-command` с `sendkey h`, `sendkey e`, `sendkey l`, `sendkey p`, `sendkey ret`. После этого serial log должен содержать выполненную команду и рост `IRQ1 keyboard count`.

## Следующий рубеж

Следующим приоритетом будет расширение текущего точечного mapper до контролируемого 4-level paging: единая структура `boot_info`, резервирование страниц ядра/модулей/MMIO, kernel heap и безопасные API отображения. Затем появится текстовый терминал на framebuffer — ключевой шаг для запуска командной строки на обычном ПК без COM1.

## References

[1]: https://wiki.osdev.org/8259_PIC "OSDev Wiki — PIC remap, masks and EOI"
[2]: https://wiki.osdev.org/APIC "OSDev Wiki — Local APIC"
[3]: https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-376.html "Intel SDM Vol. 3A — ExtINT on LINT0"
[4]: https://wiki.osdev.org/Programmable_Interval_Timer "OSDev Wiki — PIT"
[5]: https://wiki.osdev.org/PS/2_Keyboard "OSDev Wiki — PS/2 keyboard"
