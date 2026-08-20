# Спецификация файловой системы MyOS

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="FILESYSTEM_SPEC.md">🇺🇸 ENGLISH</a>
</p>


> **Статус:** MYPFS004 hierarchy и dynamic large-file storage реализованы в `feature/gui`. Его on-disk format, limits и migration contract находятся в [MYPFS004_STORAGE_RU.md](MYPFS004_STORAGE_RU.md). Этот документ остаётся источником правды для root tree, path policy, runtime projection и application layout.

## 1. Цель и границы

Новая файловая система MyOS предоставляет один логический корень `/` с настоящими каталогами, файлами и path resolution. Внутренние носители — read-only initramfs, persistent MyOS data partition и RAM — не должны превращаться в пользовательские префиксы пути. Поэтому пути `disk/...` и `tmp/...` больше не являются частью нового интерфейса.

Первая реализация намеренно ограничена. В неё входят обычные файлы, каталоги, case-preserving/case-insensitive ASCII lookup, read-only virtual runtime view и безопасная миграция. В неё не входят многопользовательская аутентификация, права uid/gid, raw device access, запись в runtime-объекты, hard links, GUI shortcuts, external filesystems и mountable чужие диски. Symbolic links являются следующим компактным расширением после стабилизации базовой иерархии; тип объекта резервируется форматом сразу, но не активируется в первом filesystem release.

## 2. Единое видимое дерево

Все системные имена задаются в нижнем регистре. Внутри любой user-visible path VFS сохраняет оригинальное написание имени, но сравнивает ASCII `A–Z` и `a–z` без различия регистра. Поэтому `README`, `Readme` и `readme` обозначают один объект и не могут сосуществовать в одном каталоге. Listing возвращает сохранённое canonical spelling объекта.

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

| Путь | Носитель | Persistent | Назначение |
|---|---|---:|---|
| `/system/core/` | Initramfs | Да, как часть boot image | Read-only базовая среда ОС: встроенные программы, resources и examples. |
| `/system/data/` | MyOS data partition | Да | Общие изменяемые данные всей машины. |
| `/system/config/` | MyOS data partition | Да | Общие конфигурации ОС, future system components и shared application defaults. |
| `/system/live/` | Kernel memory, generated on lookup | Нет | Read-only System Inventory: boot facts, статус compiled-in drivers, detected-device records и process snapshots. |
| `/apps/` | MyOS data partition | Да | Глобально установленные приложения: ELF и неизменяемые resources пакета. |
| `/users/myos/files/` | MyOS data partition | Да | Обычные личные files, включая notes и imported legacy files. |
| `/users/myos/projects/` | MyOS data partition | Да | Исходники, проекты и local build outputs. |
| `/users/myos/data/` | MyOS data partition | Да | Любые изменяемые данные главного profile. |
| `/users/myos/config/` | MyOS data partition | Да | Все конфигурации главного profile: shell, GUI, editor, preferences и application settings. |
| `/temp/` | RAM tmpfs | Нет | Temporary files, automatically cleared on reboot. |

Boot-компоненты Limine, `kernel.elf`, boot configuration и raw initramfs не дублируются в видимом дереве: они остаются на EFI/FAT boot partition и не являются обычными user files. Их read-only runtime content проецируется под `/system/core/`.

## 3. Приложения и данные

Глобальное приложение — каталог `/apps/<application>/`. Обязательным executable entry является `/apps/<application>/main.elf`; дополнительные неизменяемые app resources размещаются в `/apps/<application>/resources/`. Shell сначала ищет команду среди `/system/core/apps/`, затем ищет `/apps/<application>/main.elf`. Явный absolute path всегда имеет приоритет над поиском по имени.

Изменяемые данные не хранятся внутри пакета приложения. Для главного profile его состояние располагается в `/users/myos/data/<application>/` и `/users/myos/config/<application>/`. Machine-wide system service или shared application использует `/system/data/<application>/` и `/system/config/<application>/`. Это обеспечивает обновление или замену `/apps/<application>/` без потери личных files и settings.

Будущий отдельный milestone может добавить personal application installation в `/users/myos/apps/<application>/`; этот путь не создаётся и не участвует в command lookup первой реализации.

## 4. Path contract

| Правило | Контракт первой реализации |
|---|---|
| Path form | User-facing API принимает absolute path, начинающийся с `/`. |
| Maximum path | 111 visible ASCII bytes плюс terminating NUL; этот предел сохраняет VFS read/write/spawn request в существующем bounded syscall-copy budget. |
| Component length | До 63 visible ASCII bytes; NUL, `/`, control characters, `.` и `..` не могут быть именем объекта. |
| Depth | Не более 8 directory components ниже `/`. |
| Lookup | ASCII case-insensitive, case-preserving. Unicode case folding не входит в первый release. |
| Navigation tokens | `.` и `..` допустимы только как элементы path resolution; `..` никогда не выходит выше `/`. |
| Type collision | Нельзя создать файл и каталог с одинаковым именем в одном parent; имена, отличающиеся только ASCII case, конфликтуют. |
| Core writes | Любая create/write/remove/rename операция под `/system/core/` отклоняется. |
| Runtime writes | Любая write/create/remove операция под `/system/live/` отклоняется. |
| Temp lifetime | Все `/temp/` objects находятся в RAM и исчезают после reboot. |

## 5. System Inventory: runtime boot, drivers, devices и processes

Boot facts, processes и devices не являются persistent files. Они остаются kernel-owned state; `/system/live/` — диагностическая VFS projection, генерируемая при read/list operation. Это следует общей идее pseudo-filesystem: Linux `procfs` показывает interface к kernel data structures и содержит PID-related virtual entries, а Windows driver model оставляет смысл «files» в device namespace конкретному driver. [1] [2]

Linux `proc(5)` прямо определяет `proc` как pseudo-filesystem interface to kernel data structures и описывает PID subdirectories как virtual process information. Microsoft Windows driver documentation указывает, что device object имеет namespace, а поддержка «file» names внутри него определяется конкретному driver. Эти модели подтверждают выбранную границу MyOS: runtime entries допускаются для read-only inspection, но process, device или compiled-in driver не становятся persistent files, и рискованные control writes не входят в первый release. [1] [2]

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

Каждая virtual record — bounded text в формате `key=value` с final newline. `/system/live/boot/info` показывает identity MyOS/architecture, Limine и firmware facts, initramfs size/file count, memory summary, framebuffer availability и persistent-storage mount state. Driver records обозначают текущую static compiled-in driver model и показывают реальные bootstrap status или bounded counters; они не являются loadable packages. Device records суммируют active AHCI storage, framebuffer display, PS/2 input и PIT/RTC clock paths. Process entries исчезают после exit. `spawn`, `wait`, `kill` и driver-specific syscalls остаются единственными способами управлять process/device state. Raw sector writes, raw framebuffer writes и commands для AHCI из ring 3 не добавляются.

Обычная user-shell команда `sysinfo` выводит те же boot, driver и device records без нового syscall или write capability.

## 6. MYPFS004: текущий persistent format

MYPFS004 использует тот же третий GPT MyOS data partition, от LBA `67584` до `262110` включительно: 194527 sectors, 99597824 bytes, примерно 94.98 MiB. Format устраняет legacy flat eight-file model и MYPFS003 fixed 64 KiB per-file reservation.

| Region relative to data start | Размер | Назначение |
|---|---:|---|
| Sector 0 | 1 sector | Primary MYPFS004 superblock и format constants. |
| Sector 1 | 1 sector | Secondary MYPFS004 superblock copy. |
| Sectors 2–33 | 32 sectors | 128 object records по 128 bytes: object ID, parent ID, type, flags, preserved spelling, size и до шести data extents. ASCII case folding выполняется при lookup, поэтому отдельная canonical name copy не хранится. |
| Sectors 34–81 | 48 sectors | Allocation bitmap для data blocks. |
| Sectors 82–(end−513) | remainder | Allocatable file payload blocks размером 512 bytes. |
| Last 513–2 sectors | 512 sectors | Reserved migration staging area; не аллоцируется обычным VFS. Его ёмкость точно покрывает 8 legacy MYPFS002 files по 32 KiB. |
| Last sector | 1 sector | Migration journal header и recovery state. |

Persistent object types текущей реализации: `directory` и `regular file`. Формат также резервирует values для `symbolic link`, `virtual` и `mount root`, чтобы расширение не требовало нового on-disk revision. Regular file получает storage лениво и растёт 64 KiB batches; запись сначала продлевает последний extent, иначе добавляет новый contiguous run. Один file имеет до шести extents и ceiling 8 MiB. Offset-based VFS I/O переводит logical sector через ordered extent table.

## 7. Migration в MYPFS004

Mount автоматически обрабатывает MYPFS003 и legacy MYPFS001/MYPFS002. Переход MYPFS003 → MYPFS004 сохраняет hierarchy и data blocks: VFS копирует 32-sector old node table в reserved staging area, записывает journal `M4MG`, переводит single extent каждого file в `extent[0]`, обновляет оба superblocks и очищает journal. Если mount находит `M4MG`, он завершает conversion из staged metadata.

Legacy flat format не сохраняет `disk/` как visible alias. Его payload сначала помещается в reserved tail staging area, затем MYPFS004 создаёт approved root tree и files по следующему mapping:

| Legacy path | MYPFS004 path |
|---|---|
| `disk/bin/<name>` | `/apps/<name>/main.elf` |
| `disk/note` | `/users/myos/files/notes/note` |
| `disk/<name>` | `/users/myos/files/imported/<name>` |

Legacy migration journal `M3MG` фиксирует staged records. При следующем mount migration повторяется или завершается из staging area; после successful persistent write journal очищается.

## 8. Initramfs projection и SDK compatibility

Build system продолжает помещать base user programs в initramfs, но CPIO names меняются на logical `/system/core/` paths. Ожидаемые mappings:

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

Текущий внешний SDK остаётся поддержанным: `make -C sdk` по-прежнему создаёт static x86_64 ELF64 ET_EXEC. Workflow установки меняется с `install sdk/hello disk/bin/sdk-hello` на `install /system/core/examples/sdk/hello.elf /apps/sdk-hello/main.elf`; запуск меняется на `run /apps/sdk-hello/main.elf` или short command lookup `sdk-hello`.

## 9. Required API changes

Current separate persistent/tmpfs syscall families are replaced by unified path operations: lookup/stat, list directory, create file, create directory, write file, remove object and rename object. The current `vfs_get_entry(index)` flat enumeration becomes directory-scoped list semantics. Kernel owns source selection, read-only mount policy and object-type validation; user programs must not choose an underlying storage provider based on a `disk/` or `tmp/` prefix.

The initial release exposes regular files and directories through bounded syscalls. Symbolic-link creation/readlink is postponed; runtime objects are read-only. New shell commands must use ordinary absolute paths: `list`, `make-dir`, `touch`, `write`, `remove`, `build`, `run`, `install`, `startgui` and `edit`. Their final spellings remain a shell UX decision, but all must route through the unified VFS API.

## 10. Deferred work

Personal app installation, actual login/accounts and permissions, hard links, GUI shortcuts, external mountable volumes, raw device access, writable runtime controls, Unicode naming and a full native C compiler remain intentionally outside the current hierarchy release. Restricted native assembly source and its project-to-package workflow are implemented; see [NATIVE_BUILD_RU.md](NATIVE_BUILD_RU.md). MYPFS004 multi-extent allocation is implemented; its limits, migration contract and validation record are in [MYPFS004_STORAGE_RU.md](MYPFS004_STORAGE_RU.md).

## References

[1] [Linux proc(5): process and system information pseudo-filesystem](https://man7.org/linux/man-pages/man5/proc.5.html)

[2] [Microsoft: controlling access to a device namespace](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/controlling-device-namespace-access)
