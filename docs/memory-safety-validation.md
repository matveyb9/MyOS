# MyOS 0.6.0-dev memory safety validation

<p align="center">
  <a href="memory-safety-validation_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), [developer guide](DEVELOPER_GUIDE.md) and [documentation index](README.md).


## Validation results

MyOS 0.6.0-dev strengthens memory handling on top of the custom PML4 from the previous stage. The PMM distinguishes usable and free physical frames, the free-list heap reuses freed blocks, and vector 14 prints the saved CR2 and decoded error code. These changes do not add demand paging: any page fault is diagnostic and terminates the current run.

| Check | BIOS QEMU Q35 | UEFI QEMU Q35 + OVMF | Result |
|---|---:|---:|---|
| PMM allocate/reserve/free | Passed | Passed | `pmmtest` returns the original `free_frames` and prints `passed`. |
| Heap multi-page write/read | Passed | Passed | A `4096 + 64` byte block crosses a page boundary and preserves markers. |
| Heap free/reuse | Passed | Passed | After `kfree` the address of the first 64-byte block is returned to the allocator. |
| Heap diagnostics | Passed | Passed | After the test `active allocations = 0`, `free blocks = 1`, `reuses = 1`. |
| Page fault CR2 | Passed | Passed | The controlled unmapped access reports `0xFFFF900040000000`. |
| Page fault error code | Passed | Passed | Message: non-present, read, supervisor, error code `0`. |
| PIT/IRQ after memory changes | Passed | Passed | `ticks` and IRQ0 continue to increase. |

## Observed page-fault diagnostics

The `pagefault` command reads the address just beyond the allocated heap virtual range. It should stop the kernel and produce the following human-readable report:

```text
Vector: 0x000000000000000E (Page fault)
Error code: 0x0000000000000000
Fault address (CR2): 0xFFFF900040000000
Page fault cause: non-present page; access: read; privilege: supervisor
```

The processor saves the faulting linear address in CR2, and the bits of the error code describe presence, write/read, user/supervisor, reserved-bit and instruction-fetch properties of the fault. MyOS reads CR2 before printing diagnostics because a subsequent page fault can overwrite this register. [1]

| Component | Guarantee (0.6.0-dev) | Limitation |
|---|---|---|
| PMM | `free` rejects unaligned, unusable, already-free, or out-of-range frames. | No owner tags and no memory above 4 GiB. |
| Heap | `kfree` rejects NULL, non-heap addresses, double free, bad magic, or bad size. | No locking, red-zone, or page reclamation. |
| Paging | New heap/MMIO pages are mapped via its own PML4. | No `unmap`, NX, or user mappings. |
| Fault handler | Saves CR2, decodes the cause, and fail-stop halts. | No recovery, COW, or demand paging. |

## Re-running the tests

```bash
cd /home/ubuntu/myos
make
make run
make run-uefi
```

In a normal shell run `pmmtest`, `heaptest`, `heap`, `paging`, `ticks`, and `irqs`. The `pagefault` command should be run separately because correct behavior results in a diagnostic kernel halt.

## References

[1]: https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-227.html "Intel SDM Vol. 3A — Page-Fault Error Code and CR2"
[2]: https://wiki.osdev.org/Exceptions "OSDev Wiki — Exception vector 14"
