# MyOS 0.5.0-dev Virtual Memory Model

> **🌐 LANGUAGE / ЯЗЫК:** [🇷🇺 РУССКИЙ](paging-model_RU.md) / **🇺🇸 ENGLISH**

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to [USER_GUIDE.md], [DEVELOPER_GUIDE.md] and [README.md].


## Purpose

MyOS 0.4.0-dev uses the page tables provided by the bootloader and can add a single MMIO page for the Local APIC. MyOS 0.5.0-dev will create and own its own PML4 root, preserving existing working mappings only for the transition period. Four-level paging on x86_64 represents a virtual address as PML4, PDPT, PD, PT indices and an offset inside a 4 KiB page; CR3 contains the physical address of the active top level. [1] [2]

| Область виртуальных адресов | Назначение в MyOS 0.5 | Политика |
|---|---|---|
| `0xFFFFFFFF80000000+` | Higher-half ELF kernel and static data. | Mapping is preserved; code will later be RX, read-only data — RO. |
| HHDM `offset + physical` | Direct access to physical pages, bootloader structures and the new tables. | The initial implementation maps available physical memory supervisor RW only. |
| `0xFFFFFFFFC0000000` | Local APIC MMIO. | 4 KiB, RW, PWT+PCD; non-cacheable. |
| `0xFFFF900000000000` | Kernel heap. | Mapped on demand in 4 KiB pages; supervisor RW only. |
| Lower half | Future user space. | In 0.5 remains unmapped, except for what is temporarily required for a safe transition. |

> Page tables — a hierarchy of arrays: each entry points to the next table or to a physical page. The sparse structure avoids allocating tables for unused regions of the address space. [2]

## Invariants

1. All new page-table pages are allocated only via PMM and zeroed before publishing present-entry.
2. Kernel and heap mappings do not receive the U/S flag; user access does not appear until ring 3 is implemented.
3. MMIO does not use ordinary cacheable mappings.
4. After changing an already accessible mapping, invalidation is performed via `invlpg`; after switching the PML4 CR3 is reloaded.
5. The bootloader's old tables are not freed in stage 0.5: this prevents referencing unknown structures during early migration.

## Scope of the first managed address space

The first owned PML4 will be constructed as an extension of the current working address space. This is an intentional transitional design: it preserves the higher-half kernel and HHDM, adds managed kernel heap and MMIO mappings, but does not yet claim process isolation. After `boot_info`, a page-fault policy and ring 3 appear, MyOS will be able to create independent address spaces.

## References

[1]: https://wiki.osdev.org/X86_Paging "OSDev Wiki — X86 Paging"
[2]: https://docs.kernel.org/mm/page_tables.html "Linux kernel documentation — Page Tables"
