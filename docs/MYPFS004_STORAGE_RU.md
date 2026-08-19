# MYPFS004: практическое хранение крупных файлов

> **🇷🇺 РУССКИЙ** / [🇺🇸 ENGLISH](MYPFS004_STORAGE.md)


> **Статус:** реализовано и проверено в `gui/bringup`. MYPFS004 заменяет fixed single-extent allocator MYPFS003, не меняя видимое дерево `/system`, `/apps`, `/users/myos` и `/temp`.

## Цель

MYPFS003 уже даёт MyOS реальные каталоги, case-insensitive lookup и persistent application packages, однако каждый regular file заранее получает один непрерывный extent на 64 KiB. Это безопасная базовая модель, но она ограничивает будущие native tools, object files, larger ELF applications и архивы.

MYPFS004 вводит **dynamic multi-extent allocation**. Размер regular file растёт по мере записи и в нормальном случае ограничивается не заранее выделенным 64 KiB slot, а свободным пространством MyOS data partition и практическим per-file ceiling.

## Утверждённые limits

| Параметр | MYPFS003 | MYPFS004 | Причина |
|---|---:|---:|---|
| Persistent objects | 128 | 128 | Сохраняется тот же bounded node table и безопасная metadata migration; увеличение object table будет отдельным масштабированием metadata. |
| Regular-file maximum | 64 KiB | **8 MiB** | Достаточно для early native toolchain и заметно выше SDK ELF; не требует крупного kernel snapshot buffer. |
| Extents на regular file | 1 | **до 6** | Укладывается в существующий 128-byte record без изменения physical metadata layout. |
| Allocation unit | 512 bytes | 512 bytes | Соответствует AHCI sector I/O и текущему bitmap. |
| Usable data payload | около 94.69 MiB | около 94.69 MiB | Layout data partition не переносится и existing payload remains in place. |
| Path/name/depth rules | 112 / 64 / 8 | без изменения | Не смешивать large-file work с отдельным path-ABI milestone. |

> MYPFS004 не означает «бесконечные файлы». Любая файловая система имеет limits. Здесь снимается именно искусственный fixed 64 KiB ceiling: практический предел regular file становится 8 MiB, а общий объём по-прежнему ограничен свободным местом отдельного MyOS data partition.

## On-disk record model

MYPFS004 сохраняет current placement of superblocks, 128 records, allocation bitmap, payload blocks, migration staging area и journal. Благодаря этому conversion MYPFS003 → MYPFS004 не перемещает file bytes и не сокращает usable data area.

Каждый 128-byte regular-file record содержит size, parent, type, flags, six `(start_block, block_count)` extent pairs и preserved file name. Каталоги не имеют extents. Первый extent начинается в прежнем поле `start_block`; remaining five используют ранее unused bytes record. Payload block numbers являются относительными к неизменному `PERSIST_DATA_LBA`.

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

Lookup продолжает быть case-preserving and ASCII case-insensitive. Directory topology, system mounts, virtual `/system/live` and tmpfs remain unchanged.

## Allocation and I/O rules

Когда write extends file, VFS вычисляет число logical sectors, необходимое для нового size, и резервирует space batches до 128 sectors (64 KiB). Она сначала пытается продлить последний extent, если следующие blocks свободны. Если это невозможно, allocator ищет новый contiguous free run и добавляет extent. Без fragmentation six extents обычно не исчерпываются; если all six used, а file needs additional non-contiguous storage, write safely returns failure.

`read` и `write` переводят logical file sector в physical block через ordered extent table. Delete освобождает все extents. Creating an empty regular file does not reserve 8 MiB: storage is allocated only on the first write and grows on demand.

`vfs_open()` retains a smaller explicit snapshot bound for APIs that require a contiguous kernel buffer. Large-file streaming is performed through bounded offset-based `vfs_read()` and `vfs_write_file()` operations; native tools must stream data rather than request whole-file snapshots.

## Migration contract

### MYPFS003 → MYPFS004

MYPFS003 data blocks and bitmap are already compatible: every old file has exactly one extent, represented as MYPFS004 `extent[0]`. Migration changes metadata records and the format magic only; file payload bytes are not copied or moved.

Before overwriting records, VFS copies the complete 32-sector MYPFS003 node table to the existing reserved staging area and writes an `M4MG` journal header. It then writes MYPFS004 records, updates primary and secondary superblocks and clears the journal only after those metadata writes complete.

If interruption occurs while `M4MG` exists, next mount decodes the staged MYPFS003 node table and repeats the conversion. If journal is absent and superblock says MYPFS003, conversion starts normally. Thus no committed file payload is discarded or moved during the format upgrade.

### MYPFS001/MYPFS002 → MYPFS004

Legacy flat-format migration keeps existing path mapping and uses the same reserved staging area for legacy payload. It creates MYPFS004 root directories directly, so no intermediate persistent MYPFS003 mount is required.

| Legacy path | MYPFS004 path |
|---|---|
| `disk/bin/<name>` | `/apps/<name>/main.elf` |
| `disk/note` | `/users/myos/files/notes/note` |
| `disk/<name>` | `/users/myos/files/imported/<name>` |

## Explicitly deferred

MYPFS004 does not add symbolic-link creation, hard links, GUI shortcuts, compression, checksums, generic rename, permissions, external filesystem mounts, Unicode names or unlimited metadata. The next storage scale milestone may enlarge the node table beyond 128 objects and introduce an on-disk extent overflow table when six extents are insufficient.

## Завершённая проверка milestone

Все проверки ниже выполнены на QEMU Q35 с `myos.img`, подключённым как `-drive if=ide,format=raw,file=...`. Это важно: AHCI persistent path не проверяется при ином attachment mode.

| Проверка | Результат |
|---|---|
| Fresh format и hierarchy | Passed: `/`, `/system`, `/apps`, `/users/myos`, `/temp` и `/system/live/processes` доступны на final image. |
| Growth и fragmentation | Passed: temporary verifier записал 1 MiB в `mypfs004-first.bin` bounded 256-byte writes; interleaved 64 KiB barrier file вынудил second extent; exact pattern readback passed. |
| Persistence | Passed: fresh BIOS mount показал `mypfs004-first.bin` размером 1,048,576 bytes, а `run wc` полностью прочитал 4,096 lines, 12,289 words и 1,048,576 bytes. |
| SDK application | Passed: `install /system/core/examples/sdk/hello.elf /apps/sdk-hello/main.elf`; app executed with arguments before and after a fresh BIOS boot, а также через UEFI/OVMF. |
| MYPFS003 migration | Passed: fixture с hierarchy и known payload перешёл через `M4MG`; durable superblock стал `MYPFS004`, journal очистился, payload читался после second clean mount. |
| MYPFS002 migration | Passed: fixture `disk/note` migrated to `/users/myos/files/notes/note`; durable superblock стал `MYPFS004`, legacy journal очистился, payload читался после second clean mount. |
| Safety boundaries | Implemented: 8 MiB per-file ceiling, six extent entries, capacity checks и failure propagation в read/write paths. Exhaustive low-space and six-extent stress fixtures остаются будущим hardening work. |

## References

The format and limits in this document are MyOS project design decisions. Runtime VFS boundaries remain documented in [FILESYSTEM_SPEC_RU.md](FILESYSTEM_SPEC_RU.md), including external references for the read-only process/device projection.
