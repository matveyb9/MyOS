# Проверка безопасности памяти MyOS 0.6.0-dev

## Контрольный результат

MyOS 0.6.0-dev усиливает память поверх собственного PML4 из предыдущего этапа. PMM различает пригодные и свободные физические кадры, free-list heap повторно использует освобождённые blocks, а vector 14 печатает сохранённый CR2 и decoded error code. Эти изменения не добавляют demand paging: любой page fault является диагностическим и завершает текущий запуск.

| Проверка | BIOS QEMU Q35 | UEFI QEMU Q35 + OVMF | Результат |
|---|---:|---:|---|
| PMM allocate/reserve/free | Пройдено | Пройдено | `pmmtest` возвращает исходный `free_frames` и печатает `passed`. |
| Heap multi-page write/read | Пройдено | Пройдено | Блок `4096 + 64` байта пересекает page boundary и сохраняет маркеры. |
| Heap free/reuse | Пройдено | Пройдено | После `kfree` адрес первого 64-byte блока повторно возвращён allocator. |
| Heap diagnostics | Пройдено | Пройдено | После теста `active allocations = 0`, `free blocks = 1`, `reuses = 1`. |
| Page fault CR2 | Пройдено | Пройдено | Контролируемый unmapped access сообщает `0xFFFF900040000000`. |
| Page fault error code | Пройдено | Пройдено | Сообщение: non-present, read, supervisor, error code `0`. |
| PIT/IRQ после memory changes | Пройдено | Пройдено | `ticks` и IRQ0 продолжают расти. |

## Наблюдаемая page-fault диагностика

Команда `pagefault` читает адрес ровно за выделенным heap virtual range. Она должна остановить ядро и сформировать следующий смысловой отчёт:

```text
Vector: 0x000000000000000E (Page fault)
Error code: 0x0000000000000000
Fault address (CR2): 0xFFFF900040000000
Page fault cause: non-present page; access: read; privilege: supervisor
```

Процессор сохраняет faulting linear address в CR2, а биты error code описывают presence, write/read, user/supervisor, reserved-bit и instruction fetch свойства fault. MyOS читает CR2 до вывода диагностики, потому что следующий page fault может перезаписать этот регистр. [1]

| Компонент | Гарантия 0.6.0-dev | Ограничение |
|---|---|---|
| PMM | `free` отвергает невыравненный, непригодный, свободный или вне диапазона кадр. | Нет owner tags и памяти выше 4 GiB. |
| Heap | `kfree` отвергает NULL, не-heap address, double free, плохой magic или плохой размер. | Нет lock, red-zone или page reclamation. |
| Paging | Новые heap/MMIO страницы маппятся через собственный PML4. | Нет `unmap`, NX и user mappings. |
| Fault handler | Сохраняет CR2, декодирует cause и fail-stop halt. | Нет recovery, COW или demand paging. |

## Повторение теста

```bash
cd /home/ubuntu/myos
make
make run
make run-uefi
```

В обычной shell выполните `pmmtest`, `heaptest`, `heap`, `paging`, `ticks` и `irqs`. Команда `pagefault` должна выполняться отдельно, потому что корректное поведение заканчивается диагностической остановкой ядра.

## References

[1]: https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-227.html "Intel SDM Vol. 3A — Page-Fault Error Code and CR2"
[2]: https://wiki.osdev.org/Exceptions "OSDev Wiki — Exception vector 14"
