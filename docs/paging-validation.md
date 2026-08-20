# Paging and kernel heap validation in MyOS 0.5.0-dev

<p align="center">
  <a href="paging-validation_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), [developer guide](DEVELOPER_GUIDE.md) and the [documentation index](README.md).


## Implemented milestone

MyOS 0.5.0-dev creates its own PML4 page via the PMM, copies existing bootstrap Limine entries into it, and loads the physical address of the new root into `CR3`. This transition preserves the tested higher-half and HHDM mappings while bringing all new MyOS changes under its own control. A four-level walker creates PDPT, PD and PT on demand, maps 4 KiB pages and executes `invlpg` after changing an accessible virtual address. [1] [2]

| Компонент | Реализация | Проверяемый признак |
|---|---|---|
| Собственный корень | Отдельная PML4-страница, выделенная PMM и загруженная в CR3. | Команда `paging` показывает новый физический адрес PML4. |
| 4 KiB mapper | PML4 → PDPT → PD → PT; новые таблицы обнуляются перед публикацией present-entry. | В `paging` растёт счётчик созданных MyOS mappings. |
| Local APIC | Одна некэшируемая MMIO-страница в `0xFFFFFFFFC0000000`. | `paging` переводит её в `0xFEE00000` в QEMU. |
| Kernel heap | 1 GiB supervisor-only диапазон `0xFFFF900000000000`; 16-byte alignment; страницы выдаются лениво. | `heaptest` пересекает страницу и возвращает корректно записанные байты. |
| TLB | Новое PML4 активируется через CR3; поздние изменения сбрасывают одну TLB-запись `invlpg`. | PIT, Local APIC и PS/2 продолжают работать после switch. |

> This release does not free the heap or create independent user address spaces. The bootstrap loader tables are intentionally not freed: an early transition prioritizes preserving a working environment over aggressive memory reclamation.

## Verification matrix

| Scenario | BIOS QEMU Q35 | UEFI QEMU Q35 + OVMF | Result |
|---|---:|---:|---|
| Switching CR3 to own PML4 | Passed | Passed | Kernel continues serial output and services IRQs. |
| Local APIC mapping relocation | Passed | Passed | `paging` shows physical `0xFEE00000`. |
| Heap multi-page test | Passed | Passed | Allocated 64 bytes and `4096 + 64` bytes; read/write passed. |
| Heap statistics | Passed | Passed | After `heaptest`: used `0x1080`, mapped pages `0x2`, allocations `0x2`. |
| PIT IRQ0 after CR3 switch | Passed | Passed | `ticks` increases; IRQ0 counter is non-zero. |
| PS/2 IRQ1 after CR3 switch | Passed via QMP `sendkey` | Regression preserved from 0.4 | `heaptest`, `keyboard`, `halt` entered via the virtual keyboard; no buffer errors. |

## Limitations and next work

| Limitation | Reason | Next work |
|---|---|---|
| Bootstrap entries are copied wholesale. | A safe way to preserve the boot path is required. | Introduce `boot_info` and explicit reservation of kernel/module pages. |
| Heap is a monotonic bump allocator. | A simple trusted base is needed. | Add a free list, `kfree` and protection against metadata corruption. |
| No page-fault policy. | The exception handler is currently diagnostic only. | Decode the error code and implement guard/unmapped policy. |
| No user mappings. | Ring 3 is not yet present. | Reserve the lower-half addresses and U/S mappings before introducing processes. |
| No framebuffer terminal. | Validation remains serial-first. | Add a font and a text console as the next visual stage. |

## Repeat basic validation

```bash
cd /home/ubuntu/myos
make
make run
make run-uefi
```

In the serial shell run `paging`, `heap`, `heaptest`, `heap`, `ticks` and `irqs`. Expect to see a successful `Heap multi-page write/read test passed.`, an active PML4, the Local APIC mapping, and a growing PIT counter.

## References

[1]: https://wiki.osdev.org/X86_Paging "OSDev Wiki — x86 Paging"
[2]: https://docs.kernel.org/mm/page_tables.html "Linux kernel documentation — Page Tables"
