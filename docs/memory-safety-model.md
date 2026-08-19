# MyOS 0.6.0-dev Memory Safety Model

<p align="center">
  <a href="memory-safety-model_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), [developer guide](DEVELOPER_GUIDE.md) and [documentation index](README.md).


## Goal of the stage

MyOS 0.5.0-dev can allocate frames and create heap mappings, but it does not record an explicit reason for frame ownership, does not return heap memory, and prints a page fault as a regular exception. In 0.6.0-dev every operation on a physical frame will get a clear policy: a frame is either free or occupied; explicit reservation will prevent reissuing an area of a critical structure; freeing will return only an exactly-aligned occupied frame.

| Subsystem | Policy 0.6.0-dev | Not yet supported |
|---|---|---|
| PMM | Bitmap remains the source of truth; `reserve_frame` and `free_frame` are available; operations check alignment and range. | Per-frame detailed owner tag, NUMA and memory above 4 GiB. |
| Page tables | New page tables and heap pages are allocated by the PMM as occupied. | Tearing down page-table levels after `unmap`. |
| Heap | Free list of blocks inside the heap and `kfree`; reuse of freed blocks. | Coalescing all adjacent blocks, SMP locking and red zones. |
| Page fault | Vector 14 saves CR2 before potentially unsafe work, decodes the P/W/U/RSVD/I-D bits of the error code and terminates the kernel. | Demand paging, COW and fault recovery. |

## Page fault policy

The processor writes the faulting linear address into CR2 on every page fault; a subsequent fault can overwrite CR2, so the MyOS handler reads it as the very first action. The error code describes whether the mapping was present, whether the access was a write or a user access, whether a reserved bit was set and whether the access was an instruction fetch. [1] MyOS currently has no user mode and no demand paging, so any page fault is considered non-recoverable: structured diagnostics are printed and the processor halts.

> Page fault — a **fault**, i.e. in a more mature OS it can be handled and the instruction retried. MyOS 0.6 deliberately does not attempt to continue execution without a verified recovery policy. [2]

## Invariants

1. PMM must not increment the free-frame counter when freeing an already free, unaligned, or untracked address.
2. `kfree()` accepts only the address of the start of a known heap block; unknown addresses and double frees are rejected without changing the list.
3. The page fault handler reads CR2 before printing lines and before invoking complex diagnostics.
4. Tests must show that `heapfree` returns a block for reuse and that an inspection page fault produces CR2 plus the decoded reason.

## References

[1]: https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-227.html "Intel SDM Vol. 3A — Page-Fault Error Code and CR2"
[2]: https://wiki.osdev.org/Exceptions "OSDev Wiki — Exception vector 14"
