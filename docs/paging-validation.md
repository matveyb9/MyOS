# Validation of paging and kernel heap in MyOS 0.5.0-dev

> **Language:** [English](paging-validation.md) | [Русский](paging-validation_RU.md)

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), [developer guide](DEVELOPER_GUIDE.md) and the [documentation index](README.md).


## Implemented milestone

MyOS 0.5.0-dev creates its own PML4 page via the PMM, copies existing Limine bootstrap entries into it, and loads the physical address of the new root into `CR3`. This transition preserves the proven higher-half and HHDM mappings while bringing all new MyOS changes under its own control. A four-level walker creates PDPT, PD and PT on demand, maps 4 KiB pages and performs `invlpg` after modifying the affected virtual address. [1] [2]

| Component | Implementation | Verification indicator |
|---|---|---|
| Own root | A separate PML4 page allocated from the PMM and loaded into CR3. | The `paging` command shows the new physical PML4 address. |
| 4 KiB mapper | PML4 → PDPT → PD → PT; new tables are zeroed before publishing the present entry. | The `paging` command increments the count of created MyOS mappings. |
| Local APIC | One uncached MMIO page at `0xFFFFFFFFC0000000`. | `paging` maps it to `0xFEE00000` in QEMU. |
| Kernel heap | 1 GiB supervisor-only range `0xFFFF900000000000`; 16-byte alignment; pages are issued lazily. | `heaptest` crosses a page boundary and returns correctly written bytes. |
| TLB | The new PML4 is activated via CR3; later changes invalidate a single TLB entry with `invlpg`. | PIT, Local APIC and PS/2 continue to work after the switch. |

> This release does not free the heap nor create independent user address spaces. The bootloader's bootstrap tables are intentionally not freed: the early transition prioritizes keeping a working environment over aggressive memory reclamation.

## Validation matrix

| Scenario | BIOS QEMU Q35 | UEFI QEMU Q35 + OVMF | Result |
|---|---:|---:|---|
| Switch CR3 to the new PML4 | Passed | Passed | The kernel continues serial output and handles IRQs. |
| Remap Local APIC | Passed | Passed | `paging` shows physical `0xFEE00000`. |
| Heap multi-page test | Passed | Passed | 64 bytes and `4096 + 64` bytes allocated; read/write passed. |
| Heap statistics | Passed | Passed | After `heaptest`: used `0x1080`, mapped pages `0x2`, allocations `0x2`. |
| PIT IRQ0 after CR3 switch | Passed | Passed | `ticks` increases; IRQ0 counter is non-zero. |
| PS/2 IRQ1 after CR3 switch | Passed via QMP `sendkey` | Regression retained from 0.4 | `heaptest`, `keyboard`, `halt` entered via the virtual keyboard; no buffer errors. |

## Limitations and next steps

| Limitation | Reason | Next steps |
|---|---|---|
| Bootstrap entries are copied in full. | The boot path must be preserved safely. | Introduce `boot_info` and explicit reservation of kernel/module pages. |
| Heap is a monotonic bump allocator. | A simple trusted base is required. | Add a free list, `kfree` and protection against metadata corruption. |
| No page-fault policy. | The exception handler is currently diagnostic. | Decode the error code and implement guard/unmapped policies. |
| No user mappings. | Ring 3 is not yet present. | Reserve the lower half of addresses and U/S mappings before processes. |
| No framebuffer terminal. | Validation remains serial-first. | Add a font and a text console as the next visual stage. |

## Re-running the basic validation

```bash
cd /home/ubuntu/myos
make
make run
make run-uefi
```

In the serial shell run `paging`, `heap`, `heaptest`, `heap`, `ticks` and `irqs`. Expect a successful `Heap multi-page write/read test passed.`, an active PML4, a mapped Local APIC and a growing PIT counter.

## References

[1]: https://wiki.osdev.org/X86_Paging "OSDev Wiki — x86 Paging"
[2]: https://docs.kernel.org/mm/page_tables.html "Linux kernel documentation — Page Tables"
