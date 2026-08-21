# Руководство разработчика MyOS

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="DEVELOPER_GUIDE.md">🇺🇸 ENGLISH</a>
</p>




Этот документ описывает текущую development line **`feature/gui`** MyOS. Он предназначен для разработчиков, системных программистов и читателей, которым нужна карта исходного кода и технические инварианты. Стабильная console boundary остаётся immutable тегом `v0.12.1-console`; текущие GUI, SDK и MYPFS004 changes не переносятся в неё без отдельного release decision.

> `main` и `console-stable` сохраняют console scope. GUI experiments, MYPFS004 и user-program platform развиваются только в `feature/gui` и не должны менять console release автоматически.

## 1. Build и test contract

### Host dependencies

| Инструмент | Использование |
|---|---|
| `gcc`, `ld`, `nasm`, `make` | Сборка freestanding C11 kernel, user ELF и ASM. |
| `xorriso`, `mtools` | Создание ISO и FAT EFI partition. |
| `sgdisk` | Создание GPT raw image. |
| `qemu-system-x86_64` | BIOS и UEFI regression. |
| OVMF | UEFI firmware for QEMU. |

### Make targets

| Target | Output / purpose |
|---|---|
| `make` or `make all` | Собирает hybrid BIOS/UEFI ISO `myos.iso`. |
| `make img` | Пересоздаёт 128 MiB raw GPT image `myos.img`. |
| `make run` | BIOS ISO test in headless serial mode. |
| `make run-graphic` | BIOS ISO test with framebuffer window. |
| `make run-uefi` | UEFI ISO test in headless serial mode. |
| `make run-uefi-graphic` | UEFI ISO test with framebuffer window. |
| `make smoke` | Headless BIOS and UEFI raw-image boot smoke: checks firmware marker, persistent AHCI mount and automatic `[myos]$` entry. |
| `make regression` | Создаёт disposable raw-image copy; проверяет QMP-injected PS/2 `Alt+Tab` focus, `Alt+F4` закрытие focused MONITOR, `Esc` viewer return, `Alt+F4` editor cancel-to-viewer и `Ctrl+Q` clean exit в BIOS и UEFI, затем mouse activation compact tiles `NOTES` и `FILES` (включая full current-path title и FILES parent navigation), controls закрытия `SYSTEM`/`MONITOR`, подъём MONITOR по title bar, viewer close-to-home, editor cancel-to-viewer и запуск discovered installed-app tile с PPM framebuffer transitions. Он также проверяет read-only System Inventory tree `/system/live/` и `sysinfo` в обоих firmware paths, запускает direct bounded `tree` exploration с retained `run tree` compatibility, direct case-insensitive `find` search с retained `run find` compatibility, direct two-line `head` и `tail` views, ASCII `sort` и `stat` type/size lookup по CPIO-backed logical-VFS paths (включая direct-command help и retained `run` compatibility в BIOS), сохраняет alias `startgui home`, проверяет GUI note load/save/readback 16 KiB через шестьдесят четыре 256-byte VFS chunks, seeded from deterministic initramfs fixture, paced editor-authored 305-byte direct shell `cp` copy через VFS boundary, exact readback, no-overwrite behavior и retained `run cp` compatibility rejection, затем direct `wc` на persisted file 259 bytes, чьё final word пересекает boundary chunk 256 bytes, с retained `run wc` compatibility, console-editor source persistence, legacy native branches, empty и forwarded `args` output, exact-match/fallback behavior инструкции `input`, modular byte arithmetic `add`/`sub`/`mul`, safe unsigned `div`, private-slot `cmp`, RTC output `HH:MM:SS` и rejection cases. |
| `make release-check` | Requires a clean Git tree, rebuilds ISO/IMG, runs smoke/regression and prints the source commit plus artifact SHA-256; does not tag or publish. |
| `make debug` | Starts QEMU paused with GDB server on TCP 1234. |
| `make inspect` | Prints ELF headers and sections. |

The raw image is the authoritative storage test target because AHCI read/write and persistent files require a Q35 IDE-attached raw disk:

```bash
make all img
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c -serial stdio -display none
```

For UEFI, provide mutable OVMF variables and boot the same raw image:

```bash
cp /usr/share/OVMF/OVMF_VARS_4M.fd /tmp/myos-vars.fd
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/tmp/myos-vars.fd \
  -drive if=ide,format=raw,file=myos.img \
  -boot c -serial stdio -display none
```

## 2. Repository map

| Path | Responsibility |
|---|---|
| `boot/` | Higher-half linker script and Limine boot menu/configuration. |
| `include/` | Kernel/user ABI contracts: scheduling, paging, syscalls, VFS, pipes, AHCI and architecture helpers. |
| `kernel/main.c` | Limine requests, bootstrap ordering, diagnostics and handoff to kernel shell. |
| `kernel/arch/x86_64/` | GDT, IDT, TSS, APIC, syscall entry and low-level NASM primitives. |
| `kernel/console/` | COM1 mirror, framebuffer text console and kernel shell. |
| `kernel/mm/` | PMM, four-level page tables, user address spaces and kernel heap. |
| `kernel/sched/` | Round-robin scheduler, task lifecycle, context switch and input waits. |
| `kernel/sys/` | SYSCALL/SYSRET dispatcher, user-copy validation, ABI enforcement и immutable bootstrap-state inventory handoff. |
| `kernel/loader/` | newc CPIO reader, ELF64 loading and bounded process spawn. |
| `kernel/fs/` | Read-only initramfs VFS, MYPFS/tmpfs backends и generated `/system/live/` System Inventory records. |
| `kernel/ipc/` | Bounded one-way pipe table and endpoint lifetime. |
| `kernel/drivers/` | PIT, PS/2 keyboard, RTC, PIC, PCI and AHCI. |
| `user/` | Ring-3 shell, freestanding user programs and initramfs payload. |
| `sdk/` | Public freestanding user ABI, build template, examples и live SDK-built `cp` VFS developer tool. |
| `tools/mkcpio.py` | Deterministic user-program/initramfs packaging helper. |

## 3. Bootstrap and execution model

The boot path is deliberately simple and linear.

```text
Limine
  -> kernel_entry (NASM)
  -> kmain
  -> serial + framebuffer console
  -> GDT / IDT / TSS / syscall MSRs
  -> PMM + paging + heap
  -> ACPI / PIC / PIT / PS2 / RTC / PCI / AHCI
  -> initramfs + VFS + persistent mount
  -> scheduler and kernel workers
  -> three-second auto-init countdown
  -> `K` cancellation to kernel shell, or `/init` user process
  -> ring-3 user shell
```

Limine supplies the framebuffer, memory map, firmware information, RSDP and initramfs module. MyOS keeps a higher-half kernel around `0xffffffff80000000`, uses the HHDM supplied by Limine during bootstrap and owns a four-level PML4 for its mappings. User programs run in separate address spaces and enter the kernel through `SYSCALL/SYSRET`.

### Privilege and task model

| Item | Current value / policy |
|---|---|
| Target architecture | x86_64 only; no 32-bit compatibility target. |
| Task slots | 16 total scheduler slots. |
| Kernel stack | 64 KiB per task. |
| Ring-3 user stack | 16 KiB: четыре mapped pages по 4 KiB непосредственно ниже `INIT_STACK_TOP`, с одной unmapped guard page ниже. |
| Scheduling | PIT IRQ0 at 100 Hz; round-robin READY task selection. |
| User mapping range | `0x0000000000001000`–`0x00007FFFFFFFFFFF`. |
| Kernel heap | 1 GiB virtual reservation at `0xFFFF900000000000`. |
| Process states | UNUSED, READY, RUNNING, SLEEPING, ZOMBIE, WAITING and INPUT. |

Loader выделяет четыре user-stack frames независимо для `/init` и каждого spawned user program, сначала резервирует lower unmapped guard, а все mapped frames освобождает только в соответствующем pre-task failure path. После создания task её address space владеет frames. Scheduler updates TSS RSP0 and activates the task address space on each context switch. `wait`, `sleep`, console input and pipe reads block through scheduler state rather than busy-waiting.

## 4. Syscall boundary

`include/syscall.h` is the shared user/kernel ABI. The dispatcher in `kernel/sys/syscall.c` validates descriptor values, limits and user address ranges before copying. Обычные user buffers копируются через page-aware `copy_from_user` и `copy_to_user`; larger fixed GUI content request использует собственный page-aware mapped-range copy helper. Direct user pointers are not trusted.

The release includes write/read, process lifecycle, task info, VFS read/enumeration, RTC/uptime, bounded spawn arguments, tmpfs/persistent-file operations, pipe operations, reboot and poweroff. The most important limits are:

| Limit | Value |
|---|---:|
| Generic write syscall payload | 512 bytes |
| GUI content request | 16 528 bytes: четыре 64-bit fields, NUL-terminated title 112 bytes и content 16 384 bytes; dedicated active-session mapped-range copy |
| GUI viewer/editor content | 16 KiB (16 384 bytes); до шестидесяти четырёх VFS transfers по 256 bytes |
| Spawn path | 112 bytes including terminator capacity |
| Spawn argument storage | 128 bytes |
| Unified VFS read/write chunk | 256 bytes |
| Legacy tmpfs/persistent compatibility write chunk | 128 bytes |
| Pipe channels | 4 |
| Pipe capacity per channel | 256 bytes |

Native assembler использует существующий blocking syscall `MYOS_SYS_READ` для получения одного байта и `MYOS_SYS_RTC_TIME` для чтения `myos_rtc_time`; этот milestone не добавляет новый syscall number. Generated ELF содержит RX image по `0x400000` и fixed RW private-data mapping размером 32 bytes по `0x401000`. Entry prologue сохраняет loader-supplied argument pointer в bytes `0..7`; `args` сканирует не более existing payload `MYOS_SPAWN_ARGUMENTS_MAX - 1` размером 127 bytes и выводит его только при non-empty string. Bytes `8..23` — input/time scratch, а bytes `24..31` — восемь private slots `store`/`load`. `set`, `input` и `load` устанавливают zero-extended byte condition в `EBX`; `add` и `sub` генерируют `add bl, imm8` и `sub bl, imm8`, а `mul` генерирует `mov eax, ebx; imul eax, eax, imm32; movzx ebx, al` и `div` — `mov eax, ebx; xor edx, edx; mov ecx, imm32; div ecx; movzx ebx, al`. `mul` сохраняет low byte modulo 256; source-level `div` требует `1..255`, поэтому его unsigned byte quotient не может overflow или вызвать trap. `cmp <0..7>` генерирует `cmp bl, byte [absolute slot]; setne bl; movzx ebx, bl`, оставляя initialized zero byte при equality или one byte при inequality без открытия mutable memory за fixed private slots. Syscall entry не сохраняет general argument registers через dispatcher, поэтому emitter заново загружает scratch pointer после каждого syscall, прежде чем снова использовать эту память.

SDK повторно публикует additive fixed-size subset этой ABI в `sdk/include/myos.h`: VFS read, create-file, write и remove wrappers с 256-byte request payloads. SDK-built app `cp` использует только эти public wrappers; direct shell `cp` вызывает это приложение, а `run cp` остаётся compatibility form. Он создаёт только absent target с existing parent, никогда не перезаписывает её и удаляет только partial target, созданный им при failure copy.

The source of truth is always the structures and constants in `include/syscall.h`, not this table, when changing the ABI.

## 5. Filesystem and storage design

### VFS layers

The VFS lookup order and implementation are in `kernel/fs/vfs.c`.

| Namespace | Backend | Lifetime |
|---|---|---|
| `/system/core/` | Read-only newc CPIO | Built into image. |
| `/system/data/`, `/system/config/`, `/apps/`, `/users/myos/` | MYPFS004 persistent hierarchy over guarded AHCI data LBAs | Survives reboot of the same `myos.img`. |
| `/temp/` | In-memory bounded tmpfs | Lost at reboot. |
| `/system/live/` | Kernel-generated virtual System Inventory projection | Только current boot; read-only; boot, compiled-in driver, device и process records. |

`kernel/main.c` публикует measured Limine/bootstrap facts в small state holder `kernel/sys/inventory.c` после завершения probes. `kernel/fs/vfs.c` объединяет этот immutable state с existing driver counters и генерирует bounded `key=value` records под `/system/live/boot`, `/system/live/drivers` и `/system/live/devices`; новый syscall, persistent node, raw-device handle или write path не вводятся. Обычная user-shell команда `sysinfo` читает records через existing VFS read ABI.

MYPFS004 has 128 persistent object records, dynamic multi-extent regular-file allocation, six extents per file and an 8 MiB per-file ceiling. Empty files reserve no payload. Growth is batched at 128 sectors (64 KiB); offset-based VFS calls stream large files through 256-byte user ABI chunks. See [FILESYSTEM_SPEC_RU.md](FILESYSTEM_SPEC_RU.md) and [MYPFS004_STORAGE_RU.md](MYPFS004_STORAGE_RU.md) for the public and on-disk contracts.

### Raw image invariant

`myos.img` is a 128 MiB GPT disk image with three partitions.

| Partition | LBA range | Type / purpose |
|---|---:|---|
| 1 | 34–2047 | BIOS boot partition for Limine. |
| 2 | 2048–67583 | EFI FAT partition containing boot files. |
| 3 | 67584–262110 | Isolated MyOS data partition. |

`ahci_write_data_sector()` accepts only data-partition LBAs. MYPFS004 uses two superblocks, 32 record sectors, 48 bitmap sectors, data blocks, a 512-sector migration staging tail and final journal sector. AHCI commands allocate four DMA frames; every read and write exit path must release them. Any change to image layout, AHCI guard constants or persistent metadata must update all contracts together and repeat BIOS/UEFI tests.

## 6. Input, console and user shell

COM1 output is mirrored to the framebuffer text console. Keyboard driver обрабатывает PS/2 Set 1 US QWERTY characters, декодирует `Alt+Tab`, `Alt+F4` и `Ctrl+Q` в explicit bounded tokens, предоставляет bounded internal character-injection helper для mouse-generated GUI actions и будит tasks в состоянии `INPUT`. `Alt+Tab` и `Alt+F4` намеренно используют low control tokens, поэтому existing validation GUI syscall `0..127` не расширяется; `Ctrl+Q` и mouse top-bar `X` непосредственно потребляет GUI owner как explicit session-exit actions. Framebuffer сопоставляет четыре compact fixed launcher tiles (`SYSTEM`, `NOTES`, `EDIT NOTE`, `FILES`), до четырёх discovered package tiles, bounded FILES browser rows с fixed type/name/byte-size metadata или window-chrome hit rectangles с internal mouse-action tokens. Package actions занимают bounded range `MYOS_INPUT_GUI_ACTION_APP_BASE`; `MYOS_INPUT_GUI_ACTION_FILES` запускает browser, а его actions parent/previous/four entry/next/create используют отдельный bounded range. Kernel допускает только directories `/apps` с non-empty regular `main.elf`; ring 3 независимо повторно перечисляет и проверяет selected package перед spawn, завершением GUI и ожиданием child. Для FILES ring 3 владеет absolute current directory и page index, повторно перечисляет каждый selected entry перед joining printable child name без `/`, допускает navigation только в существующей logical VFS и принимает create prompt до 63 printable ASCII bytes без `/` только для `/users/myos`, `/temp`, `/system/data` или `/system/config`; creation вызывает только unified empty-file VFS operation перед переходом в existing editor. Полный NUL-terminated current directory также копируется в fixed GUI title field 112 bytes; compositor использует compact title glyph spacing, поэтому title bar File Workspace при supported 1280×800 отображает весь bounded logical path без изменения pointer geometry. `Alt+F4` возвращает state-specific результат закрытия focused window через тот же bounded GUI input path. Второй input queue или general pointer IPC не создаётся. Каждое non-launcher content update поднимает internal record `NOTES` перед redraw, поэтому активный viewer/editor остаётся видим. Compositor получает normalized RTC `HH:MM:SS` и allocated/runnable counts fixed scheduler на 16 slots при GUI begin и каждом content update; он рисует эти display-only values как top clock и footer status `TASKS`/`RUN` без нового ABI state. PIT IRQ0 обновляет только top-bar clock rectangle 72×32 раз в configured second, восстанавливая и повторно рисуя pointer 11×11 вокруг bounded update; он не перерисовывает окна и не poll-ит ring 3. Task values остаются snapshots content updates. После bootstrap `kernel/console/shell.c` ждёт три секунды `K` от PS/2 или COM1: без отмены `/init` запускается автоматически; `K` сохраняет diagnostic kernel shell, где `init` всё ещё запускает ту же user shell вручную. Если `/init` недоступен или automatic loading не проходит, kernel сообщает об этом и остаётся в kernel shell без retry loop.

The user shell provides deterministic history navigation and unique-prefix Tab completion. Its command list and command semantics are the source of truth for end-user documentation. Changes to `command_help()`, `execute_command()` or a user program should be reflected in `docs/USER_GUIDE_RU.md` and `README.md`.

## 7. Validation baseline

Before committing a console change, at minimum perform:

| Test | Expected result |
|---|---|
| `make all img` | Strict `-Werror` build and both artifacts complete. |
| `make smoke` | Reproducible raw-image BIOS and UEFI markers pass: expected firmware, persistent AHCI mount and automatic `[myos]$` entry. |
| `make regression` | Disposable-image QMP PS/2 `Alt+Tab` focus, `Alt+F4` закрытие focused MONITOR, `Esc` viewer return, `Alt+F4` editor cancel-to-viewer и `Ctrl+Q` clean exit проходят в BIOS и UEFI; также проходят mouse activation compact tiles NOTES и FILES, transitions title текущего пути File Workspace при parent и `/system` navigation, FILES parent navigation, controls закрытия SYSTEM/MONITOR, подъём MONITOR по title bar, viewer close-to-home, editor cancel-to-viewer и запуск discovered installed-app tile с PPM framebuffer transitions; launcher capture дополнительно требует non-uniform text regions clock и task status в fixed QEMU geometry 1280×800, а unchanged launcher после 1.75 seconds должен показать transition clock region без content input, а FILES browser capture требует visible current-path title, transitions title region при parent и `/system` navigation и fixed-column byte-size metadata в его first entry row. Read-only System Inventory tree `/system/live/` и `sysinfo` output проходят в обоих firmware paths. Direct bounded `tree` exploration с BIOS direct-tree help и retained `run tree` compatibility, direct case-insensitive `find` search с BIOS direct-find help и retained `run find` compatibility, direct two-line `head` preview с BIOS direct-head help и retained `run head` compatibility, direct two-line `tail` preview с BIOS direct-tail help и retained `run tail` compatibility, direct `sort` ASCII ordering с BIOS direct-sort help и retained `run sort` compatibility, и direct `stat` type/size lookup с BIOS direct-stat help и retained `run stat` compatibility, проходят по CPIO-backed logical-VFS paths. Native `stackprobe` touches 12 KiB automatic buffer и verifies checksum `1566720` в обоих firmware paths, доказывая mapping всех четырёх user-stack pages. Retained alias `startgui home`, GUI note editing, File Workspace mouse creation и GUI-editor save zero-byte file `/users/myos/guinew` с UEFI type/size persistence, paced editor-authored 305-byte direct shell `cp` copy через VFS chunk boundary, exact readback, direct overwrite rejection и retained `run cp` compatibility rejection, exact line/word/byte output direct `wc` для persisted boundary case 259 bytes с retained `run wc` compatibility, direct `grep` output короткой matching line при пропуске matching line, пересекающей limit retained line 127 bytes, с retained `run grep` compatibility, установленный VFS create/write example `sdk-write` с exact payload readback и no-overwrite rule, editor source workflow, legacy zero/nonzero branches, `store`/`load` private-variable branch и rejected slot `8`, modular branch `(250 + 8 - 2) mod 256` с `add`/`sub` и rejected uninitialized `add`, persisted branch `MULDIV`, проверяющий `((200 * 2 mod 256) + 57) / 3 = 67`, с rejected `div 0`, persisted branch `BITWISE`, проверяющий `not`, `and 63` и `or 128` от byte `240` к byte `143`, с rejected uninitialized `not` и `and 256`, persisted branch `XOR`, проверяющий `170 xor 255 xor 85 = 0`, с rejected uninitialized `xor` и `xor 256`, persisted branch `SHIFT`, проверяющий `3 shl 5 shr 4 = 6`, с rejected uninitialized, zero и out-of-range shifts, persisted private-slot comparison package `EQ`/`NE`, проверяющий и zero, и nonzero result с rejected uninitialized и slot-`8` `cmp`, empty и forwarded native `args`, native `input` exact-match/fallback paths, valid RTC `HH:MM:SS` output и rejected targets также проходят. UEFI повторяет полную GUI modifier/mouse surface, чтение persisted text/copied target и SDK-write payload, package execution включая persisted SDK writer и arithmetic package, и clean GUI return. |
| `make release-check` | Clean source tree, clean rebuild, `make smoke`, `make regression`, source commit and SHA-256 artifacts all pass; no tag or remote publication occurs. |
| BIOS raw image | Limine boot, automatic `/init` after three seconds, then user shell. |
| BIOS cancellation | `K` during countdown keeps kernel shell; manual `init` reaches user shell. |
| UEFI raw image | Equivalent automatic startup and user shell through OVMF. |
| Fallback check | Missing or failed `/init` leaves diagnostic kernel shell without retry loop. |
| Process check | `run hello`, `spawn sleeper 1`, `ps`, `wait` or `kill`. |
| Filesystem check | Create/write/read/remove an absolute-path `/temp/` file and a persistent `/users/myos/...` file; list `/system/live` и read `/system/live/boot/info` или run `sysinfo`. |
| Persistence check | Reboot the same `myos.img`, then read a previous persistent absolute-path file or run an installed `/apps/<name>/main.elf`. |
| Large-file check | Stream a fragmented multi-extent file; remount and read every byte through bounded VFS requests. |
| Migration check | Boot deterministic MYPFS003 and MYPFS002 fixtures, then confirm durable `MYPFS004` superblock, cleared journal and second-mount payload readback. |
| IPC check | `pipe sample`; использовать direct `wc` или direct `grep` для file. `run wc` и `run grep` остаются compatible. |

`make smoke` is a boot baseline. `make regression` extends it with GUI, persistent storage, the direct console-editor workflow and restricted native workflow evidence, but it deliberately uses a disposable image copy and therefore does not replace focused migration fixtures or a manual physical-PC check. `make release-check` is the local reproducibility gate before release discussion; it only produces evidence and never creates a tag or performs network publication. For storage code, test both firmware paths on **the same image**: write in BIOS, then read in UEFI. Never test raw AHCI writes on a host block device unless an isolated disposable test device is explicitly intended. The current release gate order is in [RELEASE_STABILIZATION_RU.md](RELEASE_STABILIZATION_RU.md).

## 8. Git workflow

| Reference | Rule |
|---|---|
| `v0.12.0-console` | Do not move or rewrite. It freezes the completed console OS. |
| `main` | Console release branch. Restrict changes to release fixes, documentation and explicitly approved console maintenance. |
| `console-stable` | Optional maintenance line at the same baseline. Cherry-pick tested fixes deliberately. |
| `feature/gui` | Separate GUI experimentation. Do not merge it into `main` unless a future GUI release is explicitly approved. |

A normal GitHub publication should push `main`, `console-stable` and the annotated console tag. Build artifacts `myos.iso` and `myos.img` are release attachments, not Git-tracked source files.

## 9. Known technical limits

This is a console milestone, not a production OS. Current non-goals include networking, USB HID, SMP, IOAPIC routing, NVMe, demand paging, dynamic linker, Unix ABI compatibility, package management, full filesystem semantics, Secure Boot and production security hardening. AHCI is deliberately limited to one bounded sector operation and the known isolated data range.

Bounded mouse-first desktop launcher, открываемый bare `startgui` (с `startgui home` как alias), имеет четыре compact fixed tiles с File Workspace v1; `FILES` начинает в `/users/myos/`, показывает полный current logical path в title окна, проходит logical VFS по four-entry pages, безопасно открывает read-only virtual/system records и вызывает completed 16 KiB GUI editor только для writable roots. До четырёх verified package tiles `/apps/<name>/main.elf`, его per-window controls подъёма по title bar и закрытия, standard keyboard fallback `Alt+Tab`/`Alt+F4`/`Esc`/`Ctrl+Q` без legacy single-letter GUI commands, SDK VFS subset, direct shell `cp`, `wc`, `grep`, `tree`, `find`, `head`, `sort`, `tail` и `stat`, backed by live developer tools, native build, bounded control-flow, argument forwarding, input/time и general text-editor milestones завершены: `asm` формирует x86_64 `ET_EXEC` с fixed private data segment из `.mya` source; shell `build` предоставляет project workflow; `args`, `input`, `time`, `set <0..255>`, `not`, `and <0..255>`, `or <0..255>`, `xor <0..255>`, `shl <1..7>`, `shr <1..7>`, `add <0..255>`, `sub <0..255>`, `mul <0..255>`, `div <1..255>`, `store <0..7>`, `load <0..7>`, `cmp <0..7>`, `label name:`, `jump name`, `jump_if_zero name`, `jump_if_nonzero name` и `jump_if <0..255> name` компилируются в bounded forward-only code с восемью private byte variables. `not`/`and`/`or`/`xor` требуют initialized byte accumulator и обновляют его восемь bits; `shl`/`shr` требуют его и логически сдвигают byte на 1–7 positions; `add`/`sub`/`mul` также требуют его и wrap modulo 256; `div` имеет то же prerequisite, отклоняет zero и сохраняет unsigned integer quotient; `cmp` сравнивает этот accumulator с private slot и превращает equality в zero, а inequality в one. Direct `edit <absolute-file>` предоставляет cursor-based multi-line editing для ordinary files и `.mya` source с all-in-memory limit 4 KiB; его contract описан в [TEXT_EDITOR_RU.md](TEXT_EDITOR_RU.md). Будущая native work должна сохранять established storage, ABI и control-flow limits; C frontend или general linker не планируются до стабилизации этих limits. Не переносить GUI, MYPFS004 или native-toolchain work в `main` либо `console-stable` без explicit release decision.

## 10. Documentation maintenance

Documentation changes are part of feature maintenance. Any change to build/run behavior, public shell behavior, ABI, storage layout, host support, branch policy or safety guidance must update the corresponding documentation in the same commit. The authoritative checklist is [DOCUMENTATION_POLICY_RU.md](DOCUMENTATION_POLICY_RU.md).
