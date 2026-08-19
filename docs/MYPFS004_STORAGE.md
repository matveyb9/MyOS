# MYPFS004: practical storage for large files

<p align="center">
  <a href="MYPFS004_STORAGE_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>


> **Status:** implemented and verified in `gui/bringup`. MYPFS004 replaces the fixed single-extent allocator MYPFS003 without changing the visible tree `/system`, `/apps`, `/users/myos` and `/temp`.

## Purpose

MYPFS003 already gives MyOS real directories, case-insensitive lookup and persistent application packages, however every regular file was preallocated a single continuous extent of 64 KiB. This is a safe baseline model, but it limits future native tools, object files, larger ELF applications and archives.

MYPFS004 introduces **dynamic multi-extent allocation**. A regular file grows as written and in normal operation is not limited by a preallocated 64 KiB slot but by the free space of the MyOS data partition and a practical per-file ceiling.

## Approved limits

| Parameter | MYPFS003 | MYPFS004 | Reason |
|---|---:|---:|---|
| Persistent objects | 128 | 128 | The same bounded node table and safe metadata migration are preserved; increasing the object table will be a separate metadata scaling effort. |
| Regular-file maximum | 64 KiB | **8 MiB** | Large enough for the early native toolchain and noticeably above SDK ELF; does not require a large kernel snapshot buffer. |
| Extents per regular file | 1 | **up to 6** | Fits in the existing 128-byte record without changing the physical metadata layout. |
| Allocation unit | 512 bytes | 512 bytes | Matches AHCI sector I/O and the current bitmap. |
| Usable data payload | около 94.69 MiB | около 94.69 MiB | The data partition layout is not moved and the existing payload remains in place. |
| Path/name/depth rules | 112 / 64 / 8 | unchanged | Do not mix large-file work with a separate path-ABI milestone. |

> MYPFS004 does not mean “infinite files.” Every filesystem has limits. This change removes the artificial fixed 64 KiB ceiling: the practical limit for a regular file becomes 8 MiB, while the total volume remains bounded by the free space of the separate MyOS data partition.

## On-disk record model

MYPFS004 retains the current placement of superblocks, 128 records, allocation bitmap, payload blocks, migration staging area and journal. Thanks to this conversion MYPFS003 → MYPFS004 does not move file bytes and does not reduce the usable data area.

Each 128-byte regular-file record contains size, parent, type, flags, six `(start_block, block_count)` extent pairs and a preserved file name. Directories have no extents. The first extent begins in the previous `start_block` field; the remaining five use previously unused bytes of the record. Payload block numbers are relative to the unchanged `PERSIST_DATA_LBA`.

```text
MYPFS004 regular-file record

0x00  object state, type, name length, flags
0x04  parent object ID
0x08  logical file size
0x10  extent[0]: start block, block count
0x18  extent[1]: start block, block count
...
0x38  extent[5]: start block, block count
0x40  preserved ASCII file name
```

Lookup remains case-preserving and ASCII case-insensitive. Directory topology, system mounts, virtual `/system/live` and tmpfs remain unchanged.

## Allocation and I/O rules

When a write extends a file, VFS computes the number of logical sectors required for the new size and reserves space batches up to 128 sectors (64 KiB). It first tries to extend the last extent if the following blocks are free. If that is not possible, the allocator finds a new contiguous free run and adds an extent. Without fragmentation six extents are usually not exhausted; if all six are used and the file needs additional non-contiguous storage, the write safely returns failure.

`read` and `write` translate a logical file sector to a physical block via an ordered extent table. Delete frees all extents. Creating an empty regular file does not reserve 8 MiB: storage is allocated only on the first write and grows on demand.

`vfs_open()` retains a smaller explicit snapshot bound for APIs that require a contiguous kernel buffer. Large-file streaming is performed through bounded offset-based `vfs_read()` and `vfs_write_file()` operations; native tools must stream data rather than request whole-file snapshots.

## Migration contract

### MYPFS003 → MYPFS004

MYPFS003 data blocks and bitmap are already compatible: every old file has exactly one extent, represented as MYPFS004 `extent[0]`. Migration changes metadata records and the format magic only; file payload bytes are not copied or moved.

Before overwriting records, VFS copies the complete 32-sector MYPFS003 node table to the existing reserved staging area and writes an `M4MG` journal header. It then writes MYPFS004 records, updates primary and secondary superblocks and clears the journal only after those metadata writes complete.

If an interruption occurs while `M4MG` exists, the next mount decodes the staged MYPFS003 node table and repeats the conversion. If the journal is absent and the superblock indicates MYPFS003, conversion starts normally. Thus no committed file payload is discarded or moved during the format upgrade.

### MYPFS001/MYPFS002 → MYPFS004

Legacy flat-format migration keeps existing path mapping and uses the same reserved staging area for legacy payload. It creates MYPFS004 root directories directly, so no intermediate persistent MYPFS003 mount is required.

| Legacy path | MYPFS004 path |
|---|---|
| `disk/bin/<name>` | `/apps/<name>/main.elf` |
| `disk/note` | `/users/myos/files/notes/note` |
| `disk/<name>` | `/users/myos/files/imported/<name>` |

## Explicitly deferred

MYPFS004 does not add symbolic-link creation, hard links, GUI shortcuts, compression, checksums, generic rename, permissions, external filesystem mounts, Unicode names or unlimited metadata. The next storage scale milestone may enlarge the node table beyond 128 objects and introduce an on-disk extent overflow table when six extents are insufficient.

## Completed milestone verification

All checks below were performed on QEMU Q35 with `myos.img` attached as `-drive if=ide,format=raw,file=...`. This is important: the AHCI persistent path is not verified with a different attachment mode.

| Check | Result |
|---|---|
| Fresh format and hierarchy | Passed: `/`, `/system`, `/apps`, `/users/myos`, `/temp` and `/system/live/processes` are present on the final image. |
| Growth and fragmentation | Passed: a temporary verifier wrote 1 MiB to `mypfs004-first.bin` with bounded 256-byte writes; an interleaved 64 KiB barrier file forced a second extent; exact pattern readback passed. |
| Persistence | Passed: a fresh BIOS mount showed `mypfs004-first.bin` sized 1,048,576 bytes, and `run wc` fully read 4,096 lines, 12,289 words and 1,048,576 bytes. |
| SDK application | Passed: `install /system/core/examples/sdk/hello.elf /apps/sdk-hello/main.elf`; the app executed with arguments before and after a fresh BIOS boot, as well as under UEFI/OVMF. |
| MYPFS003 migration | Passed: a fixture with hierarchy and a known payload converted via `M4MG`; the durable superblock became `MYPFS004`, the journal cleared, and the payload was readable after a second clean mount. |
| MYPFS002 migration | Passed: the `disk/note` fixture migrated to `/users/myos/files/notes/note`; the durable superblock became `MYPFS004`, the legacy journal cleared, and the payload was readable after a second clean mount. |
| Safety boundaries | Implemented: 8 MiB per-file ceiling, six extent entries, capacity checks and failure propagation in read/write paths. Exhaustive low-space and six-extent stress fixtures remain future hardening work. |

## References

The format and limits in this document are MyOS project design decisions. Runtime VFS boundaries remain documented in [FILESYSTEM_SPEC.md](FILESYSTEM_SPEC.md), including external references for the read-only process/device projection.
