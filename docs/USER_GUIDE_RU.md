# Руководство пользователя MyOS

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="USER_GUIDE.md">🇺🇸 ENGLISH</a>
</p>


Это руководство предназначено для человека, который хочет **собрать, запустить и попробовать MyOS**, не изучая устройство ядра. MyOS — экспериментальная учебно-практическая ОС для `x86_64`, написанная с нуля на freestanding C11 и x86_64 NASM. Используйте QEMU в первую очередь; запуск на физическом компьютере выполняйте только с отдельной тестовой флешкой.

> **Текущая линия разработки:** `feature/gui`, версия `0.13.1-gui-preview.1`. Стабильная консольная граница сохранена immutable тегом `v0.12.1-console`; GUI и MYPFS004 пока не переносятся в эту границу автоматически.

Для установки toolchain на Windows, WSL, macOS и другие host-платформы сначала откройте [руководство по платформам](PLATFORMS_RU.md). Ниже описано использование MyOS после подготовки build environment.

## 1. Что понадобится

Для сборки и запуска в QEMU нужны GNU-compatible build tools, NASM, utilities для image creation и QEMU. На Ubuntu/Debian рабочий набор устанавливается так:

```bash
sudo apt update
sudo apt install build-essential nasm xorriso mtools gdisk qemu-system-x86 ovmf
```

Во всех примерах предполагается, что терминал открыт в корне исходного дерева:

```bash
cd /home/ubuntu/myos
```

## 2. Сборка artifacts

Выполните:

```bash
make all img
```

| Файл | Когда использовать |
|---|---|
| `myos.iso` | Быстрый boot test как ISO/CD в QEMU. |
| `myos.img` | Рекомендуемый raw disk/USB image с GPT, BIOS boot partition, EFI partition и постоянным разделом данных MyOS. |

> `make img` намеренно пересоздаёт `myos.img`. Все данные из persistent MYPFS004 раздела предыдущего образа при этом удаляются. Если нужны тестовые файлы, скопируйте образ до повторной сборки.

## 3. Рекомендуемый запуск в QEMU

Для полного сценария, включая persistent files и приложения, подключайте raw image именно как IDE drive:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

После загрузки MyOS показывает diagnostics в блоках `BOOT ENVIRONMENT`, `KERNEL SERVICES`, `STORAGE AND RUNTIME` и `USER ENVIRONMENT`. Затем начинается трёхсекундный countdown, после которого автоматически запускается user shell. Перед переходом framebuffer очищается:

```text
[myos]$
```

Нажмите `K` во время countdown, если нужен diagnostic kernel shell. В этом режиме boot log остаётся на экране, а user shell запускается вручную:

```text
kernel>
init
```

Для serial output в терминале добавьте `-serial stdio`. Если окно QEMU не нужно, добавьте `-display none`:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c -serial stdio -display none
```

## 4. Быстрая проверка

После automatic startup или ручного `init` попробуйте:

```text
help
uname
sysinfo
ps
meminfo
date
uptime
ls /
ls /system/live
ls /system/live/processes
run tree /system
run find tree /system/core
run head /system/core/resources/motd.txt 2
stat /system/core/resources/motd.txt
run tail /system/core/resources/motd.txt 2
sort /system/core/resources/motd.txt
run stackprobe
cat /system/core/resources/motd.txt
```

Эти команды проверяют user shell, scheduler, память, часы, initramfs, root hierarchy и read-only System Inventory runtime projection.

## 5. Файлы и каталоги

MyOS предоставляет единый логический корень `/`. Путь сохраняет оригинальное написание имени, но ASCII lookup не различает регистр: `Notes`, `NOTES` и `notes` обозначают один объект в одном каталоге. Подробная спецификация дерева приведена в [FILESYSTEM_SPEC_RU.md](FILESYSTEM_SPEC_RU.md).

| Путь | Назначение | Сохраняется после reboot |
|---|---|---:|
| `/system/core/` | Read-only initramfs: встроенные программы, resources и SDK example. | Да, как часть boot image. |
| `/system/data/`, `/system/config/` | Общие изменяемые machine-wide data и configuration. | Да. |
| `/system/live/` | Read-only snapshot процессов и устройств текущей загрузки. | Нет. |
| `/apps/` | Глобальные persistent application packages. | Да. |
| `/users/myos/files/` | Личные обычные files, notes и imported legacy files. | Да. |
| `/users/myos/projects/` | Проекты, исходники и будущие build outputs. | Да. |
| `/users/myos/data/`, `/users/myos/config/` | Личные data и configuration. | Да. |
| `/temp/` | Временные RAM files. | Нет. |

### System Inventory

`sysinfo` выводит bounded read-only records из `/system/live/boot/info`, `/system/live/drivers/` и `/system/live/devices/`. Boot record определяет active Limine/firmware/initramfs environment; driver records показывают current static compiled-in driver model и реальные bounded status или counters; device records суммируют active storage, display, input и clock paths. Это generated diagnostic records, а не persistent files и не raw-device interface. `ls /system/live` также показывает независимое дерево snapshot процессов.

### Обычная работа с файлами

Создайте каталог и текстовый файл в личном profile:

```text
mkdir /users/myos/projects/demo
write /users/myos/projects/demo/readme.txt My first MyOS project
ls /users/myos/projects/demo
cat /users/myos/projects/demo/readme.txt
```

Для временного файла используйте `/temp/`:

```text
write /temp/session.txt temporary text
cat /temp/session.txt
rm /temp/session.txt
```

После закрытия QEMU снова загрузите **тот же** `myos.img` и прочитайте persistent file по тому же absolute path. Не запускайте перед этим `make img`, потому что команда создаёт новый пустой data partition.

### Native tree view

`run tree` выводит type-aware recursive view logical VFS от `/`. Можно передать один absolute directory, чтобы начать в другом месте, например `run tree /system` или `run tree /users/myos`. Directory rows используют `[D]`, regular files — `[F]` с размером, virtual records — `[V]`. Built-in utility никогда не меняет storage, не принимает relative path, следует только existing logical VFS enumeration и останавливается на **восьми directory levels**, **64 entries в directory** или **256 printed entries**. Эти limits не позволяют exploratory command потреблять unbounded user memory или output.

### Native find search

`run find <name-fragment> [absolute-directory]` ищет names entries case-insensitively по logical VFS и выводит matching absolute paths с type markers `[D]`, `[F]` или `[V]`. Optional start directory обязана быть absolute; relative path, empty fragment или extra arguments отклоняются. Поэтому `run find TrEe /system/core` находит `/system/core/apps/tree.elf`. Как и `tree`, `find` read-only и ограничена **восьмью directory levels**, **64 entries в directory** и **256 scanned entries**, поэтому recursive search не становится unbounded consumption user memory или output.

### Native head view

`run head <absolute-file> [1..64 lines]` выводит начало одного readable VFS file. Без optional count он выводит первые **10 строк**; например, `run head /system/core/resources/motd.txt 2` показывает первые две строки MOTD. Utility принимает только один absolute file path и optional decimal count от 1 до 64. Она читает через обычные VFS ABI chunks по 256 bytes и останавливается после **4 KiB** output, даже если запрошенная граница строк ещё не встретилась, поэтому malformed или необычно длинная строка не создаёт unbounded output. Storage никогда не меняется.

### Native stat lookup

`stat <absolute-path>` выводит logical VFS **type** и **size** одного existing file, directory или virtual record. Например, `stat /system/core/resources/motd.txt` выводит entry `regular` и её byte size, а `stat /system/live/boot/info` — entry `virtual`. `run stat` остаётся compatibility form. Tool разрешает final path component сканированием не более **128 entries** его parent через existing VFS list ABI, сравнивает ASCII names case-insensitively как filesystem и не выполняет writes. Для invalid или missing entry выводится `stat: path not found`.

### Native tail view

`run tail <absolute-file> [1..64 lines]` выводит конец одного readable VFS file. Без optional count он выводит последние **10 строк**; например, `run tail /system/core/resources/motd.txt 2` показывает последние две строки MOTD. Utility принимает один absolute file path и optional decimal count от 1 до 64. Она stream-читает файл через обычные VFS ABI chunks по 256 bytes, но хранит только его последние **4 KiB**, затем выбирает requested trailing lines из этого bounded buffer. Когда более старый content отброшен, выводится `tail: retained last 4096 bytes`; поэтому строка больше retained window может быть partial. Storage никогда не меняется.

### Native sort

`sort <absolute-file>` читает один text file и выводит его retained lines в **bytewise ASCII ascending order**. Например, `sort /system/core/resources/motd.txt` выводит строки `The…`, `Use…`, затем `Welcome…`. `run sort` остаётся compatibility form. Operation read-only и использует обычные VFS read chunks по 256 bytes. Для bounded работы сохраняется не более **64 lines**, каждая до **127 bytes**; CR bytes игнорируются, а line или entry за этими limits пропускается с `sort: line or entry limit reached`. Duplicate lines сохраняют original relative order.

### Native stack probe

`run stackprobe` — read-only diagnostic utility user-program platform. Она заполняет 12 KiB automatic buffer и выводит `stackprobe: 12288 bytes checksum 1566720`. Этот точный результат подтверждает, что current program использует все четыре mapped ring-3 stack pages по 4 KiB; непосредственно ниже них остаётся guard page, перехватывающая downward stack overflow.

### File Workspace v1

Запустите `startgui` и нажмите **FILES**. Browser начинается в `/users/myos/` и показывает полный current logical path в title окна. Его controls `[..]`, `[PREV]`, entry rows и `[NEXT]` дают mouse-first traversal всех paths, которые открыты через logical VFS, включая `/`, `/system/core/`, `/system/live/`, `/apps/`, `/users/` и `/temp/`. Directory rows открывают каталог; regular и virtual files открываются безопасно. Каждая listed row показывает type marker (`D` directory, `F` regular file, `L` symbolic link или `V` virtual record), fixed visible-name column из 12 characters и current VFS byte size entry с suffix `B`. Это read-only metadata logical VFS, а не raw disk details. Raw Limine/EFI boot files и `kernel.elf` остаются boot artifacts вне этого runtime tree.

Regular file доступен для GUI edit только при writable VFS path: `/users/myos/`, `/temp/`, `/system/data/` или `/system/config/`. `Ctrl-S` сохраняет; `Esc`, `Alt+F4` или `X` окна NOTES отбрасывают несохранённый draft. `/system/core/`, `/system/live/` и `/apps/` остаются readable, но никогда не переходят в GUI editor mode. GUI document capacity — **16 KiB (16 384 bytes)**. GUI loading и saving используют не более шестидесяти четырёх bounded VFS transfers по 256 bytes. Отдельная console-команда `edit <path>` сохраняет limit 4 KiB; для больших MYPFS004 files используйте bounded program или SDK I/O.

> File Workspace v1 намеренно не предоставляет graphical create, rename, delete, copy/move, package installation или raw-device operations. Для этого используйте shell `touch`, `mkdir`, `rm`, `cp` и `install`.

### Практические limits MYPFS004

| Граница | Значение |
|---|---:|
| Persistent object records | До 128 файлов и каталогов суммарно. |
| Regular file | До 8 MiB. |
| Extents regular file | До 6 non-contiguous extents. |
| Interactive `write` command | До 256 ASCII bytes за одну command line. |
| Path | До 111 visible ASCII bytes плюс NUL. |
| Name | До 63 visible ASCII bytes. |
| Path depth | До 8 components ниже `/`. |

MYPFS004 выделяет storage лениво и растит file по мере записи. Большие программы и tools должны читать и писать файл offset-based chunks, а не ожидать, что весь файл будет одновременно открыт в непрерывном kernel buffer.

## 6. Наиболее полезные команды shell

| Команда | Пример | Назначение |
|---|---|---|
| `help` | `help` | Краткая карта возможностей shell. |
| `sysinfo` | `sysinfo` | Вывести read-only inventory boot, drivers и devices. |
| `ls` | `ls /users/myos` | Показать содержимое каталога. |
| `cat` | `cat /system/core/resources/motd.txt` | Показать file. |
| `cp` | `cp /users/myos/files/a.txt /users/myos/files/b.txt` | Скопировать file через bounded native copy tool. Target должна быть новым absolute path, а её parent уже должна существовать; overwrite не выполняется. `run cp` остаётся compatibility form. |
| `wc` | `wc /users/myos/files/a.txt` | Stream-читать один absolute readable file chunks VFS по 256 bytes и вывести newline-terminated lines, space/tab/CR/LF-delimited words и bytes. `run wc` остаётся compatibility form. |
| `grep` | `grep MyOS /system/core/resources/motd.txt` | Вывести newline-terminated lines не длиннее 127 bytes, содержащие один text fragment без spaces, при чтении одного absolute file VFS chunks по 256 bytes. Более длинные lines пропускаются. `run grep` остаётся compatibility form. |
| `run tree` | `run tree /system` | Рекурсивно показать VFS entries без mutation; принимает ноль или один absolute directory и ограничена 8 levels, 64 entries на directory и 256 printed entries. |
| `run find` | `run find tree /system/core` | Case-insensitively искать names entries без mutation; принимает fragment и optional absolute directory, с limits 8 levels, 64 entries на directory и 256 scanned entries. |
| `run stackprobe` | `run stackprobe` | Запустить diagnostic с automatic buffer 12 KiB; ожидаемый checksum `1566720` подтверждает все четыре mapped ring-3 stack pages. |
| `run head` | `run head /system/core/resources/motd.txt 2` | Вывести первые 10 строк по умолчанию или 1–64 запрошенные строки одного absolute readable file; VFS I/O использует chunks 256 bytes, а output ограничен 4 KiB. |
| `stat` | `stat /system/core/resources/motd.txt` | Вывести type и byte size одного absolute logical-VFS entry через bounded scan не более 128 entries его parent; storage не меняется. `run stat` остаётся compatible. |
| `run tail` | `run tail /system/core/resources/motd.txt 2` | Вывести последние 10 строк по умолчанию или 1–64 запрошенные trailing lines одного absolute readable file; stream-читает VFS chunks 256 bytes, сохраняя только последние 4 KiB. |
| `sort` | `sort /system/core/resources/motd.txt` | Отсортировать до 64 retained text lines в bytewise ASCII ascending order; каждая line ограничена 127 bytes, storage не меняется. `run sort` остаётся compatible. |
| `touch` | `touch /users/myos/files/note.txt` | Создать пустой persistent file. |
| `mkdir` | `mkdir /users/myos/projects/demo` | Создать каталог. |
| `write` | `write /users/myos/files/note.txt Hello` | Перезаписать file одной строкой. |
| `rm` | `rm /users/myos/files/note.txt` | Удалить file или пустой каталог. |
| `edit` | `edit /users/myos/files/note.txt` | Открыть bounded multi-line text editor; `Ctrl-S` сохраняет и завершает, `Ctrl-Q` или `Esc` отменяет edits. |
| `ps` | `ps` | Показать процессы. |
| `calc` | `calc -5 + 2` | Выполнить signed 64-bit арифметику. |
| `run` | `run hello` | Запустить foreground user program. |
| `spawn` | `spawn sleeper 3` | Запустить program в фоне. |
| `wait` / `kill` | `wait 4`, `kill 4` | Ждать или остановить дочерний процесс. |
| `set` / `get` / `env` | `set NAME MyOS` | Работать с environment variables. |
| `startgui` | `startgui` | Запустить experimental framebuffer GUI. Нажмите `FILES`, чтобы просматривать logical VFS от `/users/myos/`; row `[NEW FILE]` принимает имя до 63 printable ASCII bytes без `/` только в `/users/myos`, `/temp`, `/system/data` или `/system/config`, создаёт новый empty file и открывает его в GUI editor. Existing writable text files до 16 KiB также открываются там. |
| `reboot` / `poweroff` | `reboot` | Перезагрузить или выключить виртуальную машину. |
| `clear` | `clear` | Очистить text console. |

Большинство встроенных programs запускаются через `run` или `spawn`. Примеры:

```text
run hello
wc /system/core/resources/motd.txt
grep MyOS /system/core/resources/motd.txt
cp /system/core/resources/motd.txt /users/myos/files/motd-copy.txt
run argshow one two three
calc 12 / 3
```

`calc` принимает два signed 64-bit целых числа и оператор `+`, `-`, `*` или `/`. Деление является целочисленным; деление на ноль и overflow безопасно отклоняются.

## 7. Свои user programs и MyOS SDK

Встроенный reference ELF находится в `/system/core/examples/sdk/hello.elf`. Он копируется в global application package и затем запускается коротким именем:

```text
install /system/core/examples/sdk/hello.elf /apps/sdk-hello/main.elf
run sdk-hello external SDK validation
```

После reboot повторная установка не нужна:

```text
run sdk-hello persisted
```

SDK собирает собственные freestanding C11 programs на host computer. Его public header содержит bounded VFS read/create/write/remove wrappers, которые демонстрируют live utility `cp` и packaged reference example `sdk-write` в образе. Direct shell `cp` вызывает ту же native utility; `run cp` остаётся compatibility form. `cp` требует два absolute paths, а `sdk-write` принимает один новый absolute target и записывает fixed payload; ни один не перезаписывает existing target, а оба удаляют только partial target, созданный ими при failure. Чтобы попробовать writer, выполните `install /system/core/examples/sdk/write.elf /apps/sdk-write/main.elf`, затем `run sdk-write /users/myos/files/sdk-write-example.txt` и `cat` этого пути. Подробный workflow, ABI и linker contract приведены в [SDK_RU.md](SDK_RU.md). Для первого in-OS workflow используйте restricted assembler из следующего раздела; более богатый native C frontend остаётся последующим milestone.

## 8. Native build прямо в MyOS

Native build workflow использует restricted assembler и command `build`. Source хранится в `/users/myos/projects/`, generated ELF остаётся рядом с source, а для запуска program устанавливается в global package `/apps/<name>/main.elf`. Для multi-line source используйте общий command `edit`; `write` остаётся удобным для short one-line files.

```text
mkdir /users/myos/projects/native
edit /users/myos/projects/native/args.mya
# Наберите эти source lines, затем Ctrl-S:
write "["
args
write "]\n"
time
exit 37

build /users/myos/projects/native/args.mya /users/myos/projects/native/args.elf
install /users/myos/projects/native/args.elf /apps/native-args/main.elf
run native-args hello MyOS
```

Программа выводит `[hello MyOS]`, затем выводит текущее время RTC в формате `HH:MM:SS` и возвращает status `37`. При `run native-args` без parameters она выводит `[]`. Source language supports `args`, `input`, `time`, `set <0..255>`, `add <0..255>`, `sub <0..255>`, `mul <0..255>`, `div <1..255>`, `store <0..7>`, `load <0..7>`, `cmp <0..7>`, `label name:`, `write "text"`, `jump name`, `jump_if_zero name`, `jump_if_nonzero name`, `jump_if <0..255> name` and final `exit <0..255>`. `store` сохраняет current condition byte в одном из восьми private slots; `load` восстанавливает его как condition и допустим перед arithmetic или conditional jump. `add`, `sub` и `mul` требуют initialized condition от `input`, `set` или `load`, затем обновляют этот byte modulo 256. `div` имеет то же prerequisite, получает nonzero divisor из `1..255` и заменяет byte его unsigned integer quotient. `cmp <slot>` также требует initialized condition, сравнивает его с выбранным private slot и заменяет на `0` при equality или `1` при inequality. Slots private для running program, zero-initialized и не имеют имён или direct addressing. Every target must be a defined label located later in source, so loops and backward jumps are rejected. Escapes `\n`, `\r`, `\t`, `\\` and `\"` are available inside text. Например, `set 250; add 8; store 3; set 0; load 3; sub 2; jump_if_zero matched` показывает, что `(250 + 8 - 2) mod 256` равно zero. Sequence `set 200; mul 2; add 57; div 3` вычисляет `67`: multiplication сохраняет low byte, а division является unsigned integer division. Sequence `set 73; store 5; set 73; cmp 5; jump_if_zero equal` выбирает `equal`, а active value `72` заставляет `jump_if_nonzero` выбрать inequality path. Slot number вне `0..7`, add/sub/mul operand вне `0..255`, `div 0`, uninitialized arithmetic, а также uninitialized или out-of-range `cmp` отклоняются. The generated program runs in ring 3 and returns its authored exit status; используйте `help asm` и `help edit` для краткой command help, [Текстовый редактор](TEXT_EDITOR_RU.md) для controls и [Native Build](NATIVE_BUILD_RU.md) для всех bounds и syntax rules.

> Project ELF files are intentionally not directly runnable. The loader accepts installed user applications only from `/apps/<name>/main.elf`, so `install` remains the explicit package boundary.

## 9. Experimental GUI

GUI доступен только в ветке `feature/gui` и запускается из console, а не автоматически:

```text
startgui
# Compatibility alias: startgui home
```

Без аргумента `startgui` открывает **MYOS DESKTOP** — bounded mouse-first launcher; `startgui home` остаётся alias. Click по `SYSTEM` открывает system message, по `NOTES` — notes, по `EDIT NOTE` — editor default personal note. До четырёх installed packages с `/apps/<name>/main.elf` также появляются под fixed tiles как `OPEN APP`; click по одному запускает программу, закрывает GUI и возвращает её normal output в console. Click по `FILES` запускает путь `/users/myos/`; title его окна показывает полный current logical-VFS path и обновляется после parent или child navigation. Click по top-bar `X` выполняет выход. Launcher и window actions выполняются только мышью. Сохранившиеся GUI-level keyboard shortcuts: `Alt+Tab` переводит focus на следующее видимое окно, `Alt+F4` закрывает focused window, `Esc` возвращает или отменяет, а `Ctrl+Q` выходит. Для personal note можно передать absolute path:

```text
startgui /users/myos/files/notes/note
```

Чтобы добавить desktop app tile, сначала используйте existing package boundary:

```text
install /system/core/apps/hello.elf /apps/hello/main.elf
startgui
# Click HELLO → OPEN APP
```

`Alt+F4` закрывает focused window с тем же state-specific поведением, что и его `X`: скрывает SYSTEM или MONITOR, возвращает viewer NOTES к MYOS DESKTOP либо отменяет draft editor к viewer. В viewer или editor mode активное окно NOTES выводится на передний план. Click по title bar открытого окна поднимает его. Desktop top-bar `X` и `Ctrl+Q` завершают всю GUI session. `Esc` возвращает viewer home и отменяет draft editor, а `Ctrl+S` сохраняет. Обычное движение PS/2 mouse перерисовывает только 11×11 pointer region; full desktop refresh остаётся только для content, focus, window visibility и layout changes. Полное описание controls, notes editor и известных границ находится в [GUI_BRINGUP_RU.md](GUI_BRINGUP_RU.md).

## 10. UEFI и ISO

ISO подходит для простого boot test, но не предназначен для persistent data workflow:

```bash
qemu-system-x86_64 -machine q35 -m 256M -cdrom myos.iso -boot d
```

Для UEFI с raw image на Linux используйте OVMF:

```bash
cp /usr/share/OVMF/OVMF_VARS_4M.fd /tmp/myos-vars.fd
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/tmp/myos-vars.fd \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

## 11. Запись на USB-флешку

Для физического компьютера используйте **`myos.img`**, а не ISO. Образ содержит GPT, BIOS boot partition, EFI partition и MYPFS004 data partition.

1. Подключите отдельную флешку без важных данных.
2. Найдите её имя, например через `lsblk`.
3. Убедитесь, что выбран диск целиком, например `/dev/sdb`, а не раздел `/dev/sdb1`.
4. Запишите образ:

   ```bash
   sudo dd if=myos.img of=/dev/sdX bs=4M conv=fsync status=progress
   sync
   ```

> `dd` полностью удаляет содержимое выбранного устройства. Неверный `/dev/sdX` может уничтожить данные на системном или внешнем диске. Не выполняйте эту команду, если не уверены в имени носителя.

## 12. Ограничения текущей линии

MyOS не является заменой Linux, Windows или BSD. В `feature/gui` пока нет сети, USB HID, SMP, Secure Boot, demand paging, package manager, user accounts/permissions, полноценного native C compiler или production security hardening. Restricted native assembler реализован, но GUI остаётся bounded framebuffer environment, а не general-purpose desktop.

Если сборка или запуск не работают, выполните `make clean`, затем `make all img`, `make smoke` и `make regression`. Smoke command headlessly проверяет BIOS и UEFI boot markers, persistent AHCI mount и automatic `[myos]$` entry. Regression command использует disposable image copy: он создаёт и сохраняет default GUI note через mouse tile `EDIT NOTE`, вводит QMP PS/2 `Alt+Tab` для focus MONITOR, `Alt+F4` для закрытия focused MONITOR, `Esc` для viewer return, `Alt+F4` для editor cancel-to-viewer и `Ctrl+Q` для clean exit, затем проверяет centered launcher tiles NOTES и FILES, включая visible transitions title текущего пути File Workspace при parent и `/system` navigation, оконные controls закрытия SYSTEM/MONITOR, подъём MONITOR по title bar, viewer close-to-home и editor cancel-to-viewer через PPM framebuffer transitions. Он также сохраняет BIOS проверку alias `startgui home`, использует direct shell `cp` для копирования editor-authored file размером 305 bytes через 256-byte request boundary, сохраняет `run cp` compatibility rejection, проверяет exact target data и отклоняет overwrite, затем проверяет direct `wc` на persisted file 259 bytes, чьё final word пересекает boundary chunk 256 bytes, и сохраняет `run wc` compatibility check, собирает и устанавливает native packages в BIOS, проверяет legacy forward-only branches, empty и forwarded native arguments, exact-match и fallback paths инструкции `input`, корректный вывод RTC `HH:MM:SS`, modular add/sub arithmetic `(250 + 8 - 2) mod 256` с rejected uninitialized `add`, persisted multiply/divide arithmetic `MULDIV` с rejected `div 0`, persisted private-slot comparison `EQ`/`NE` с rejected uninitialized или slot-`8` `cmp` и rejected invalid control flow, затем проверяет persisted files, `cp` target и installed input/time/argument/arithmetic packages через UEFI. Обе команды не заменяют physical-PC test. После этого повторите QEMU command из раздела 3. Для host-platform setup используйте [PLATFORMS_RU.md](PLATFORMS_RU.md), для release gates — [RELEASE_STABILIZATION_RU.md](RELEASE_STABILIZATION_RU.md), а для технической диагностики — [DEVELOPER_GUIDE_RU.md](DEVELOPER_GUIDE_RU.md).
