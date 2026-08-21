# MyOS Filesystem Specification

<p align="center">
  <a href="FILESYSTEM_SPEC_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>


> **Status:** MYPFS004 hierarchy and dynamic large-file storage are implemented in `feature/gui`. Its on-disk format, limits and migration contract are in [MYPFS004_STORAGE.md](MYPFS004_STORAGE.md). This document remains the source of truth for the root tree, path policy, runtime projection and application layout.

## 1. Purpose and scope

The new MyOS filesystem provides a single logical root `/` with real directories, files and path resolution. Internal storage — the read-only initramfs, the persistent MyOS data partition and RAM — must not become user-visible path prefixes. Therefore the `disk/...` and `tmp/...` paths are no longer part of the new interface.

The first implementation is intentionally limited. It includes regular files, directories, case-preserving/case-insensitive ASCII lookup, a read-only virtual runtime view and safe migration. It does not include multi-user authentication, uid/gid permissions, raw device access, writable runtime objects, hard links, GUI shortcuts, external filesystems or mountable foreign disks. Symbolic links are the next compact extension after stabilizing the base hierarchy; the object type is reserved by the format immediately but not activated in the first filesystem release.

## 2. Unified visible tree

All system names are specified in lowercase. Within any user-visible path the VFS preserves the original spelling of a name but compares ASCII `A–Z` and `a–z` case-insensitively. Therefore `README`, `Readme` and `readme` denote the same object and cannot coexist in one directory. Listings return the preserved canonical spelling of the object.

```text
/
├── system/
│   ├── core/
│   │   ├── apps/
│   │   ├── resources/
│   │   └── examples/
│   ├── data/
│   ├── config/
│   └── live/
│       ├── boot/
│       ├── drivers/
│       ├── devices/
│       └── processes/
├── apps/
│   └── <application>/
│       ├── main.elf
│       └── resources/
├── users/
│   └── myos/
│       ├── files/
│       │   ├── notes/
│       │   └── imported/
│       ├── projects/
│       ├── data/
│       └── config/
└── temp/
```

| Path | Backing store | Persistent | Purpose |
|---|---|---:|---|
| `/system/core/` | Initramfs | Yes, as part of the boot image | Read-only OS base environment: built-in programs, resources and examples. |
| `/system/data/` | MyOS data partition | Yes | Machine-wide mutable data. |
| `/system/config/` | MyOS data partition | Yes | Global OS configuration, future system components and shared application defaults. |
| `/system/live/` | Kernel memory, generated on lookup | No | Read-only System Inventory: boot facts, compiled-in driver status, detected-device records and process snapshots. |
| `/apps/` | MyOS data partition | Yes | Globally installed applications: ELF and immutable package resources. |
| `/users/myos/files/` | MyOS data partition | Yes | Ordinary personal files, including notes and imported legacy files. |
| `/users/myos/projects/` | MyOS data partition | Yes | Sources, projects and local build outputs. |
| `/users/myos/data/` | MyOS data partition | Yes | Any mutable data of the main profile. |
| `/users/myos/config/` | MyOS data partition | Yes | All configuration of the main profile: shell, GUI, editor, preferences and application settings. |
| `/temp/` | RAM tmpfs | No | Temporary files, automatically cleared on reboot. |

Boot components Limine, `kernel.elf`, the boot configuration and the raw initramfs are not duplicated in the visible tree: they remain on the EFI/FAT boot partition and are not ordinary user files. Their read-only runtime content is projected under `/system/core/`.

## 3. Applications and data

A global application is the directory `/apps/<application>/`. The mandatory executable entry is `/apps/<application>/main.elf`; additional immutable app resources are placed in `/apps/<application>/resources/`. The shell first searches for a command in `/system/core/apps/`, then looks for `/apps/<application>/main.elf`. An explicit absolute path always has priority over name lookup.

Mutable data is not stored inside the application package. For the main profile its state is located in `/users/myos/data/<application>/` and `/users/myos/config/<application>/`. A machine-wide system service or shared application uses `/system/data/<application>/` and `/system/config/<application>/`. This ensures `/apps/<application>/` can be updated or replaced without losing personal files and settings.

A future separate milestone may add personal application installation in `/users/myos/apps/<application>/`; this path is not created and does not participate in command lookup in the first implementation.

## 4. Path contract

| Rule | First-release contract |
|---|---|
| Path form | The user-facing API accepts an absolute path beginning with `/`. |
| Maximum path | 111 visible ASCII bytes plus a terminating NUL; this limit preserves VFS read/write/spawn requests within the existing bounded syscall-copy budget. |
| Component length | Up to 63 visible ASCII bytes; NUL, `/`, control characters, `.` and `..` cannot be an object name. |
| Depth | No more than 8 directory components below `/`. |
| Lookup | ASCII case-insensitive, case-preserving. Unicode case folding is not included in the first release. |
| Navigation tokens | `.` and `..` are allowed only as elements of path resolution; `..` never goes above `/`. |
| Type collision | You cannot create a file and a directory with the same name in the same parent; names that differ only by ASCII case conflict. |
| Core writes | Any create/write/remove/rename operation under `/system/core/` is rejected. |
| Runtime writes | Any write/create/remove operation under `/system/live/` is rejected. |
| Temp lifetime | All `/temp/` objects reside in RAM and disappear after reboot. |
| GUI File Workspace | `startgui` → `FILES` begins at `/users/myos/`, may enumerate and traverse every logical VFS directory up to `/`, and does not expose raw boot partitions. |
| GUI mutation boundary | The 1 KiB GUI editor opens only selected existing regular files under `/users/myos/`, `/temp/`, `/system/data/` or `/system/config/`; it does not make `/system/core/`, `/system/live/` or `/apps/` writable. |

### File Workspace v1

The GUI file manager is a ring-3 navigation client, not another filesystem backend. It displays a bounded path tail, a parent row, paging controls and four VFS-enumerated rows. Before entering a directory or opening a file, ring 3 repeats enumeration for the clicked slot and builds a child path only from a printable entry name without `/`. This gives the user free read-only traversal of the visible hierarchy while preserving the VFS type and write policy. The graphical workflow deliberately omits create, rename, delete, copy/move, package install and raw-device operations; shell tools remain the authoritative interface for those mutations.

## 5. System Inventory: runtime boot, drivers, devices and processes

Boot facts, processes and devices are not persistent files. They remain kernel-owned state; `/system/live/` is a diagnostic VFS projection generated on read/list operations. This follows the general pseudo-filesystem idea: Linux `procfs` exposes an interface to kernel data structures and contains PID-related virtual entries, while the Windows driver model leaves the meaning of “files” in a device namespace to the particular driver. [1] [2]

Linux `proc(5)` explicitly defines `proc` as a pseudo-filesystem interface to kernel data structures and describes PID subdirectories as virtual process information. Microsoft Windows driver documentation notes that a device object has a namespace and that support for “file” names within it is determined by the specific driver. These models confirm the boundary chosen for MyOS: runtime entries are allowed for read-only inspection, but a process, device or compiled-in driver does not become a persistent file, and risky control writes are not included in the first release. [1] [2]

```text
/system/live/
├── boot/
│   └── info
├── drivers/
│   ├── framebuffer
│   ├── keyboard
│   ├── mouse
│   ├── ahci
│   ├── acpi
│   ├── pit
│   ├── rtc
│   └── pci
├── devices/
│   ├── storage
│   ├── display
│   ├── input
│   └── clock
└── processes/
    └── <pid>/
        └── info
```

Each virtual record is bounded `key=value` text and ends with a newline. `/system/live/boot/info` reports the MyOS/architecture identity, Limine and firmware facts, initramfs size/file count, memory summary, framebuffer availability and persistent-storage mount state. Driver records identify the current static, compiled-in driver model and report real bootstrap status or bounded counters; they are not loadable packages. Device records summarize the active AHCI storage, framebuffer display, PS/2 input and PIT/RTC clock paths. Process entries disappear after exit. `spawn`, `wait`, `kill` and driver-specific syscalls remain the only ways to control process/device state. Raw sector writes, raw framebuffer writes and AHCI commands from ring 3 are not added.

The ordinary user shell command `sysinfo` prints the same boot, driver and device records without introducing a new syscall or write capability.

## 6. MYPFS004: current persistent format

MYPFS004 uses the same third GPT MyOS data partition, from LBA `67584` to `262110` inclusive: 194527 sectors, 99597824 bytes, approximately 94.98 MiB. The format eliminates the legacy flat eight-file model and the MYPFS003 fixed 64 KiB per-file reservation.

| Region relative to data start | Size | Purpose |
|---|---:|---|
| Sector 0 | 1 sector | Primary MYPFS004 superblock and format constants. |
| Sector 1 | 1 sector | Secondary MYPFS004 superblock copy. |
| Sectors 2–33 | 32 sectors | 128 object records of 128 bytes: object ID, parent ID, type, flags, preserved spelling, size and up to six data extents. ASCII case folding is performed at lookup, so a separate canonical name copy is not stored. |
| Sectors 34–81 | 48 sectors | Allocation bitmap for data blocks. |
| Sectors 82–(end−513) | remainder | Allocatable file payload blocks of 512 bytes. |
| Last 513–2 sectors | 512 sectors | Reserved migration staging area; not allocated by the normal VFS. Its capacity exactly covers 8 legacy MYPFS002 files of 32 KiB. |
| Last sector | 1 sector | Migration journal header and recovery state. |

Persistent object types in the current implementation: `directory` and `regular file`. The format also reserves values for `symbolic link`, `virtual` and `mount root` so that extension does not require a new on-disk revision. A regular file is allocated lazily and grows in 64 KiB batches; a write first extends the last extent, otherwise a new contiguous run is added. A single file has up to six extents and a ceiling of 8 MiB. Offset-based VFS I/O maps logical sectors through the ordered extent table.

## 7. Migration to MYPFS004

Mount automatically handles MYPFS003 and legacy MYPFS001/MYPFS002. The MYPFS003 → MYPFS004 transition preserves the hierarchy and data blocks: the VFS copies the 32-sector old node table into the reserved staging area, writes a `M4MG` journal, moves the single extent of each file into `extent[0]`, updates both superblocks and clears the journal. If a mount finds `M4MG`, it completes the conversion from the staged metadata.

The legacy flat format does not preserve `disk/` as a visible alias. Its payload is first placed in the reserved tail staging area, then MYPFS004 creates the approved root tree and files according to the following mapping:

| Legacy path | MYPFS004 path |
|---|---|
| `disk/bin/<name>` | `/apps/<name>/main.elf` |
| `disk/note` | `/users/myos/files/notes/note` |
| `disk/<name>` | `/users/myos/files/imported/<name>` |

The legacy migration journal `M3MG` records staged records. On the next mount the migration is repeated or completed from the staging area; after a successful persistent write the journal is cleared.

## 8. Initramfs projection and SDK compatibility

The build system continues to place base user programs in the initramfs, but CPIO names change to logical `/system/core/` paths. Expected mappings:

| Current initramfs name | New logical path |
|---|---|
| `init` | `/system/core/apps/init.elf` |
| `hello` | `/system/core/apps/hello.elf` |
| `calc` | `/system/core/apps/calc.elf` |
| `edit` | `/system/core/apps/edit.elf` |
| `startgui` | `/system/core/apps/startgui.elf` |
| `install` | `/system/core/apps/install.elf` |
| `motd.txt` | `/system/core/resources/motd.txt` |
| `sdk/hello` | `/system/core/examples/sdk/hello.elf` |

The current external SDK remains supported: `make -C sdk` still produces a static x86_64 ELF64 ET_EXEC. The install workflow changes from `install sdk/hello disk/bin/sdk-hello` to `install /system/core/examples/sdk/hello.elf /apps/sdk-hello/main.elf`; execution changes to `run /apps/sdk-hello/main.elf` or short command lookup `sdk-hello`.

## 9. Required API changes

Current separate persistent/tmpfs syscall families are replaced by unified path operations: lookup/stat, list directory, create file, create directory, write file, remove object and rename object. The current `vfs_get_entry(index)` flat enumeration becomes directory-scoped list semantics. The kernel owns source selection, read-only mount policy and object-type validation; user programs must not choose an underlying storage provider based on a `disk/` or `tmp/` prefix.

The initial release exposes regular files and directories through bounded syscalls. Symbolic-link creation/readlink is postponed; runtime objects are read-only. New shell commands must use ordinary absolute paths: `list`, `make-dir`, `touch`, `write`, `remove`, `build`, `run`, `install`, `startgui` and `edit`. Their final spellings remain a shell UX decision, but all must route through the unified VFS API.

## 10. Deferred work

Personal app installation, actual login/accounts and permissions, hard links, GUI shortcuts, external mountable volumes, raw device access, writable runtime controls, Unicode naming and a full native C compiler remain intentionally outside the current hierarchy release. Restricted native assembly source and its project-to-package workflow are implemented; see [NATIVE_BUILD.md](NATIVE_BUILD.md). MYPFS004 multi-extent allocation is implemented; its limits, migration contract and validation record are in [MYPFS004_STORAGE.md](MYPFS004_STORAGE.md).

## References

[1] [Linux proc(5): process and system information pseudo-filesystem](https://man7.org/linux/man-pages/man5/proc.5.html)

[2] [Microsoft: controlling access to a device namespace](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/controlling-device-namespace-access)
