# MyOS 0.5.0-dev Virtual Memory Model

> **Language:** [English](paging-model.md) | [Русский](paging-model_RU.md)

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), the [developer guide](DEVELOPER_GUIDE.md) and the [documentation index](README.md).


## Purpose

MyOS 0.4.0-dev uses page tables provided by the bootloader and can add a single Local APIC MMIO page. MyOS 0.5.0-dev will create and own its own PML4 root, keeping existing working mappings only for the transition period. Four-level paging on x86_64 represents a virtual address as PML4, PDPT, PD, PT indices and an offset within a 4 KiB page; CR3 contains the physical address of the active top level. [1] [2]

| Virtual address region | Purpose in MyOS 0.5 | Policy |
|---|---|---|
| `0xFFFFFFFF80000000+` | Higher-half ELF kernel and static data. | Preserved mapping; kernel code will later become RX, read-only data — RO. |
| HHDM `offset + physical` | Direct access to physical pages, bootloader structures and new tables. | The initial implementation maps available physical memory as supervisor RW only. |
| `0xFFFFFFFFC0000000` | Local APIC MMIO. | 4 KiB, RW, PWT+PCD; uncached. |
| `0xFFFF900000000000` | Kernel heap. | Mapped on demand in 4 KiB pages; supervisor RW only. |
| Low half | Future user space. | In 0.5 remains unmapped, except what is temporarily necessary for a safe transition. |

> Page tables are a hierarchy of arrays: each entry points to the next table or to a physical page. The sparse structure allows not allocating tables for unused areas of the address space. [2]

## Invariants

1. All new page-table pages are allocated only via PMM and zeroed before publishing a present-entry.
2. Kernel and heap mappings do not get the U/S flag; user access does not appear until ring 3 is implemented.
3. MMIO does not use ordinary cacheable mappings.
4. After changing an already accessible mapping, invalidation is performed via `invlpg`; after switching the PML4, CR3 is reloaded.
5. Old bootloader tables are not freed at stage 0.5: this prevents using unknown structures during early migration.

## Scope of the first managed address space

The first owned PML4 will be built as an extension of the current working space. This is an intentional transitional design: it preserves the higher-half kernel and HHDM, adds managed kernel heap and MMIO mappings, but does not yet claim process isolation. After `boot_info`, a page-fault policy and ring 3 are present, MyOS will be able to create independent address spaces.

## References

[1]: https://wiki.osdev.org/X86_Paging "OSDev Wiki — X86 Paging"
[2]: https://docs.kernel.org/mm/page_tables.html "Linux kernel documentation — Page Tables"
