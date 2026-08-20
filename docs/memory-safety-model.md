# MyOS 0.6.0-dev Memory Safety Model

<p align="center">
  <a href="memory-safety-model_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), [developer guide](DEVELOPER_GUIDE.md) and the [documentation index](README.md).


## Milestone objective

MyOS 0.5.0-dev can allocate frames and create heap mappings, but it does not record an explicit reason for frame ownership, does not return heap memory, and prints a page fault like an ordinary exception. In 0.6.0-dev every operation on a physical frame will have a clear policy: a frame is either free or occupied; explicit reservation will not allow reallocation of a region of critical data structures; freeing will return only an exactly aligned occupied frame.

| Subsystem | Policy in 0.6.0-dev | Not yet supported |
|---|---|---|
| PMM | The bitmap remains the source of truth; `reserve_frame` and `free_frame` are available; operations check alignment and range. | Detailed per-frame owner tagging, NUMA and memory above 4 GiB. |
| Page tables | New page tables and heap pages are allocated by PMM as occupied. | Removal of page-table levels after `unmap`. |
| Heap | A free list of blocks inside the heap and `kfree`; reuse of freed blocks. | Coalescing of all adjacent blocks, SMP locking and red zones. |
| Page fault | Vector 14 saves CR2 before potentially unsafe work, decodes the P/W/U/RSVD/I-D bits of the error code and halts the kernel. | Demand paging, COW and fault recovery. |

## Page fault policy

The processor writes the linear address of the fault into CR2 on every page fault; a subsequent fault can overwrite CR2, so the MyOS handler reads it as its first action. The error code describes whether the mapping was present, whether the access was a write or a user access, whether a reserved bit was set, and whether the access was an instruction fetch. [1] MyOS currently has no user mode and no demand paging, so any page fault is considered non-recoverable: structured diagnostics are printed and the processor is stopped.

> Page fault — specifically a **fault**, meaning in a more mature OS it can be handled and the instruction retried. MyOS 0.6 deliberately does not attempt to continue execution without a verified recovery policy. [2]

## Invariants

1. PMM must not increase the free-frame count when freeing an already free, unaligned, or untracked address.
2. `kfree()` accepts only the start address of a known heap block; unknown addresses and double-free are rejected without modifying the list.
3. The page fault handler captures CR2 before printing lines and before performing complex diagnostics.
4. Tests must show that `heapfree` returns a block for reuse and that an inspected page fault produces CR2 plus a decoded reason.

## References

[1]: https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-227.html "Intel SDM Vol. 3A — Page-Fault Error Code and CR2"
[2]: https://wiki.osdev.org/Exceptions "OSDev Wiki — Exception vector 14"
