# Архитектура MyOS 0.6.0-dev

## Назначение

MyOS — учебно-практическое ядро собственной разработки для **x86_64**. Единый гибридный ISO-образ запускается через BIOS и UEFI в QEMU, а после стабилизации может тестироваться с выделенного USB-носителя. Проект последовательно строит путь от наблюдаемого консольного ядра к пользовательским процессам, файловой системе и графической среде.

> Limine подготавливает режим запуска и передаёт управление `kmain`, но не является частью MyOS. Менеджеры памяти, таблицы прерываний, драйверы, консоль и будущие пользовательские подсистемы принадлежат ядру. [1]

## Слои системы

| Слой | Каталог | Ответственность в 0.6.0-dev | Следующая граница |
|---|---|---|---|
| Загрузка | `boot/` | Higher-half ELF, заявки Limine, BIOS/UEFI меню. | Единый `boot_info` и явный список зарезервированных областей. |
| Архитектура | `kernel/arch/x86_64/` | GDT, IDT, exception/IRQ stub, CR2 helper, Local APIC virtual-wire. | Page-fault recovery policy, APIC timer, IOAPIC и SMP. |
| IRQ и драйверы | `kernel/irq/`, `kernel/drivers/` | PIC 8259A, PIT, PS/2 Set 1 и буфер клавиатуры. | USB HID, расширенные клавиши и очереди устройств. |
| Физическая память | `kernel/mm/pmm.c` | Bitmap пригодных кадров до 4 GiB, allocate/reserve/free с проверками. | Ownership tags, память выше 4 GiB, NUMA. |
| Виртуальная память | `kernel/mm/paging.c` | Собственный PML4, CR3 switch, 4 KiB mapper, Local APIC MMIO. | Unmap, user mappings, NX и защита секций. |
| Heap | `kernel/mm/heap.c` | Free list, split/coalesce, `kmalloc`/`kfree`, повторное использование. | Locks, guard zones, освобождение пустых heap-страниц. |
| Консоль | `kernel/console/` | COM1 + PS/2 shell, memory self-tests, diagnostic page fault. | Шрифт и framebuffer text console. |

## Memory ownership

В PMM две bitmap-структуры разделяют вопросы «пригоден ли кадр» и «свободен ли кадр». Только страницы, объявленные Limine как `USABLE`, становятся пригодными; frame 0 не выдаётся. `pmm_allocate_frame()` переводит свободный пригодный кадр в занятый, `pmm_reserve_frame()` запрещает его повторную выдачу, а `pmm_free_frame()` принимает только выровненный занятый пригодный адрес. Это ограничивает опасные операции и не позволяет двойному освобождению искусственно увеличить счётчик свободных кадров.

| Область | Назначение | Политика |
|---|---|---|
| Higher-half kernel | Код, данные и ранний стек. | Bootstrap entries Limine копируются в собственный PML4. |
| HHDM | Доступ к физическим frame и page tables. | Используется до появления полного самостоятельного boot-info. |
| `0xFFFFFFFFC0000000` | Local APIC MMIO. | 4 KiB, supervisor RW, PWT+PCD. |
| `0xFFFF900000000000` | Kernel heap, максимум 1 GiB. | Страницы отображаются лениво; блоки освобождаются во free list. |
| Низкая половина | Будущий user space. | MyOS пока не создаёт U/S mappings. |

## Heap

`kmalloc()` ищет подходящий свободный блок, при необходимости разделяет его, либо расширяет virtual frontier через PMM и paging. `kfree()` проверяет границы heap, magic, state и размер блока, затем вставляет блок в адресно-упорядоченный free list. Соседние блоки объединяются, поэтому серия выделений и освобождений может вернуть один повторно используемый диапазон. Страницы физически не удаляются из mapper на этом этапе: это осознанная граница перед будущим `unmap` и page ownership.

## Page fault policy

Для vector 14 MyOS считывает CR2 до serial output, поскольку следующий fault может перезаписать faulting linear address. Затем error code декодируется в non-present/protection violation, read/write, supervisor/user, reserved-bit и instruction-fetch признаки. [2] Текущая политика намеренно fail-stop: demand paging и recovery ещё не реализованы, поэтому ядро фиксирует полную причину и останавливает CPU.

> Page fault является потенциально восстановимой **fault**, но MyOS не продолжает работу после неё без проверенной политики отображения и процессов. [3]

## Диагностика shell

| Команда | Проверяемый контракт |
|---|---|
| `pmmtest` | Allocate → free → reserve → free возвращает исходный счётчик кадров. |
| `heaptest` | Многостраничный блок читается/записывается, освобождается и первый адрес повторно используется. |
| `heap` | Показывает active allocations, free blocks и reuse counter. |
| `pagefault` | Читает заведомо unmapped край heap и формирует CR2 plus decoded error code. |
| `paging`, `ticks`, `irqs` | Подтверждают PML4, Local APIC и жизнеспособность IRQ после изменений памяти. |

## Следующая техническая итерация

Приоритет следующего этапа — явно резервировать physical regions kernel/boot modules, добавить `unmap` и защитные страницы, затем перейти к текстовой console на framebuffer. Это заменит serial-first взаимодействие нормальным экранным интерфейсом, не меняя базовую архитектуру памяти.

## References

[1]: https://github.com/limine-bootloader/limine-protocol "Limine Boot Protocol — official protocol"
[2]: https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-227.html "Intel SDM Vol. 3A — Page-Fault Error Code and CR2"
[3]: https://wiki.osdev.org/Exceptions "OSDev Wiki — Exception vector 14"
