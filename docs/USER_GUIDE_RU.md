# Руководство пользователя MyOS

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="USER_GUIDE.md">🇺🇸 ENGLISH</a>
</p>


Это руководство предназначено для человека, который хочет **собрать, запустить и попробовать MyOS**, не изучая устройство ядра. MyOS — экспериментальная учебно-практическая ОС для `x86_64`, написанная с нуля на freestanding C11 и x86_64 NASM. Используйте QEMU в первую очередь; запуск на физическом компьютере выполняйте только с отдельной тестовой флешкой.

> **Текущая линия разработки:** `main`, экспериментальная QEMU-validated integration line. `console-stable` остаётся стабильным console baseline на immutable теге `v0.12.1-console`. GUI и MYPFS004 интегрированы в `main`, но публичные artifacts остаются только Pre-releases, пока не будет принята policy physical-PC validation.

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
tree /system
find tree /system/core
head /system/core/resources/motd.txt 2
stat /system/core/resources/motd.txt
tail /system/core/resources/motd.txt 2
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

`tree [absolute-directory]` выводит type-aware recursive view logical VFS от `/`. Можно передать один absolute directory, чтобы начать в другом месте, например `tree /system` или `tree /users/myos`. `run tree` остаётся compatibility form. Directory rows используют `[D]`, regular files — `[F]` с размером, virtual records — `[V]`. Built-in utility никогда не меняет storage, не принимает relative path, следует только existing logical VFS enumeration и останавливается на **восьми directory levels**, **64 entries в directory** или **256 printed entries**. Эти limits не позволяют exploratory command потреблять unbounded user memory или output.

### Native find search

`find <name-fragment> [absolute-directory]` ищет names entries case-insensitively по logical VFS и выводит matching absolute paths с type markers `[D]`, `[F]` или `[V]`. `run find` остаётся compatibility form. Optional start directory обязана быть absolute; relative path, empty fragment или extra arguments отклоняются. Поэтому `find TrEe /system/core` находит `/system/core/apps/tree.elf`. Как и `tree`, `find` read-only и ограничена **восьмью directory levels**, **64 entries в directory** и **256 scanned entries**, поэтому recursive search не становится unbounded consumption user memory или output.

### Native head view

`head <absolute-file> [1..64 lines]` выводит начало одного readable VFS file. Без optional count он выводит первые **10 строк**; например, `head /system/core/resources/motd.txt 2` показывает первые две строки MOTD. `run head` остаётся compatibility form. Utility принимает только один absolute file path и optional decimal count от 1 до 64. Она читает через обычные VFS ABI chunks по 256 bytes и останавливается после **4 KiB** output, даже если запрошенная граница строк ещё не встретилась, поэтому malformed или необычно длинная строка не создаёт unbounded output. Storage никогда не меняется.

### Native stat lookup

`stat <absolute-path>` выводит logical VFS **type** и **size** одного existing file, directory или virtual record. Например, `stat /system/core/resources/motd.txt` выводит entry `regular` и её byte size, а `stat /system/live/boot/info` — entry `virtual`. `run stat` остаётся compatibility form. Tool разрешает final path component сканированием не более **128 entries** его parent через existing VFS list ABI, сравнивает ASCII names case-insensitively как filesystem и не выполняет writes. Для invalid или missing entry выводится `stat: path not found`.

### Native tail view

`tail <absolute-file> [1..64 lines]` выводит конец одного readable VFS file. Без optional count он выводит последние **10 строк**; например, `tail /system/core/resources/motd.txt 2` показывает последние две строки MOTD. `run tail` остаётся compatibility form. Utility принимает один absolute file path и optional decimal count от 1 до 64. Она stream-читает файл через обычные VFS ABI chunks по 256 bytes, но хранит только его последние **4 KiB**, затем выбирает requested trailing lines из этого bounded buffer. Когда более старый content отброшен, выводится `tail: retained last 4096 bytes`; поэтому строка больше retained window может быть partial. Storage никогда не меняется.

### Native sort

`sort <absolute-file>` читает один text file и выводит его retained lines в **bytewise ASCII ascending order**. Например, `sort /system/core/resources/motd.txt` выводит строки `The…`, `Use…`, затем `Welcome…`. `run sort` остаётся compatibility form. Operation read-only и использует обычные VFS read chunks по 256 bytes. Для bounded работы сохраняется не более **64 lines**, каждая до **127 bytes**; CR bytes игнорируются, а line или entry за этими limits пропускается с `sort: line or entry limit reached`. Duplicate lines сохраняют original relative order.

### Native stack probe

`run stackprobe` — read-only diagnostic utility user-program platform. Она заполняет 12 KiB automatic buffer и выводит `stackprobe: 12288 bytes checksum 1566720`. Этот точный результат подтверждает, что current program использует все четыре mapped ring-3 stack pages по 4 KiB; непосредственно ниже них остаётся guard page, перехватывающая downward stack overflow.

### File Workspace v1

Запустите `startgui` и нажмите **FILES**. Browser начинается в `/users/myos/` и показывает полный current logical path в title окна. Его controls `[..]`, `[PREV]`, entry rows и `[NEXT]` дают mouse-first traversal всех paths, которые открыты через logical VFS, включая `/`, `/system/core/`, `/system/live/`, `/apps/`, `/users/` и `/temp/`. Directory rows открывают каталог; regular и virtual files открываются безопасно. Каждая listed row показывает type marker (`D` directory, `F` regular file, `L` symbolic link или `V` virtual record), fixed visible-name column из 12 characters и current VFS byte size entry с suffix `B`. Это read-only metadata logical VFS, а не raw disk details. Raw Limine/EFI boot files и `kernel.elf` остаются boot artifacts вне этого runtime tree.

Regular file доступен для GUI edit только при writable VFS path: `/users/myos/`, `/temp/`, `/system/data/` или `/system/config/`. `Ctrl-S` сохраняет; `Esc`, `Alt+F4` или `X` окна NOTES отбрасывают несохранённый draft. `/system/core/`, `/system/live/` и `/apps/` остаются readable, но никогда не переходят в GUI editor mode. GUI document capacity — **16 KiB (16 384 bytes)**. GUI loading и saving используют не более шестидесяти четырёх bounded VFS transfers по 256 bytes. Отдельная console-команда `edit <path>` сохраняет limit 4 KiB; для больших MYPFS004 files используйте bounded program или SDK I/O.

`[SEARCH]` доступен в каждом browsable root, включая read-only roots. Он принимает непустой printable ASCII name fragment без `/` длиной до 63 bytes и сканирует не более 128 entries current directory с case-insensitive matching. Первые четыре matching logical-VFS entries показываются как mouse-openable rows; выбранная entry revalidated перед open, storage не меняется, а `[RETURN]` или `Esc` возвращает в browser.

В каждом established writable root `[NEW FILE]`, `[NEW FOLDER]`, `[DELETE]`, `[COPY]`, `[RENAME]` и `[MOVE]` принимают bounded input. File action создаёт один new empty regular file и открывает GUI editor; folder action создаёт один new empty directory. `[DELETE]` сначала показывает complete named target и удаляет этот file или empty directory только после второго подтверждения **Enter**; `Esc` отменяет действие до mutation. `[COPY]` запрашивает source regular-file name и absent target name в **same current directory**, затем stream-копирует не более **64 KiB** через existing VFS requests по 256 bytes; action никогда не перезаписывает target и удаляет собственный partial target при failure. `[RENAME]` запрашивает имя existing mutable file или directory, затем новое имя в **same current directory**. Операция меняет только VFS metadata: content, extents и дочерние записи каталога остаются связаны с тем же object.

`[MOVE]` намеренно уже, чем общий file-manager move. Сначала он принимает одно имя существующего **regular file** — до 63 printable ASCII bytes без `/`, затем абсолютный existing writable destination directory. Destination сохраняет basename source, полный target path должен укладываться в fixed VFS path limit, а existing target отклоняется без overwrite. Action меняет только VFS metadata: он не копирует file content, не перемещает directories и не эмулирует move через copy и delete. Persistent file может перемещаться только внутри одного persistent move anchor: hierarchy `/users/...` образует один anchor, а `/system/data/...` и `/system/config/...` являются отдельными anchor. Temporary file может перемещаться только внутри hierarchy `/temp/...`. Read-only paths, malformed paths, missing source или destination directory, а также все cross-anchor или persistent-to-temporary moves отклоняются без mutation. Каждое successful action обновляет browser на месте. Package installation и raw-device operations остаются shell-only; используйте `rm`, `cp` и `install` для этих workflows.

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
| `tree` | `tree /system` | Рекурсивно показать VFS entries без mutation; принимает ноль или один absolute directory и ограничена 8 levels, 64 entries на directory и 256 printed entries. `run tree` остаётся compatible. |
| `find` | `find tree /system/core` | Case-insensitively искать names entries без mutation; принимает fragment и optional absolute directory, с limits 8 levels, 64 entries на directory и 256 scanned entries. `run find` остаётся compatible. |
| `run stackprobe` | `run stackprobe` | Запустить diagnostic с automatic buffer 12 KiB; ожидаемый checksum `1566720` подтверждает все четыре mapped ring-3 stack pages. |
| `head` | `head /system/core/resources/motd.txt 2` | Вывести первые 10 строк по умолчанию или 1–64 запрошенные строки одного absolute readable file; VFS I/O использует chunks 256 bytes, а output ограничен 4 KiB. `run head` остаётся compatible. |
| `stat` | `stat /system/core/resources/motd.txt` | Вывести type и byte size одного absolute logical-VFS entry через bounded scan не более 128 entries его parent; storage не меняется. `run stat` остаётся compatible. |
| `tail` | `tail /system/core/resources/motd.txt 2` | Вывести последние 10 строк по умолчанию или 1–64 запрошенные trailing lines одного absolute readable file; stream-читает VFS chunks 256 bytes, сохраняя только последние 4 KiB. `run tail` остаётся compatible. |
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
| `startgui` | `startgui` | Запустить experimental framebuffer GUI. Его footer показывает bounded active surface как `FOCUS HOME`, `FOCUS SYSTEM`, `FOCUS NOTES` или `FOCUS MONITOR`. Нажмите `FILES`, чтобы просматривать logical VFS от `/users/myos/`; rows `[NEW FILE]`, `[NEW FOLDER]`, `[DELETE]`, `[COPY]`, `[RENAME]` и `[MOVE]` доступны только в `/users/myos`, `/temp`, `/system/data` или `/system/config`. Первый создаёт new empty file и открывает его в GUI editor; второй создаёт directory и обновляет browser; `[DELETE]` показывает named target и требует второго `Enter`, прежде чем удалить file или empty directory; `[COPY]` stream-копирует existing regular source не более 64 KiB в absent same-directory target; `[RENAME]` меняет имя existing mutable file или directory внутри того же directory без копирования content; `[MOVE]` перемещает один regular file только обновлением metadata в existing writable absolute destination directory, сохраняя basename, отклоняя overwrite и оставаясь внутри одного persistent move anchor либо hierarchy `/temp`. Existing writable text files до 16 KiB также открываются там. |
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

Native build workflow использует restricted assembler и command `build`. `newproj <name> [hello|args]` безопасно создаёт `/users/myos/projects/<name>/main.mya` из default runnable template `hello` или fixed starter `args`; name — 1–31 ASCII letters, digits, `-` или `_`, а existing project либо unknown template отклоняется без creation или overwrite. Затем `editproj <name>`, `buildproj <name>`, `runproj <name> [arguments]`, `installproj <name>` и `uninstallproj <name>` разрешают только эти fixed project paths и `/apps/<name>/main.elf`, сохраняя editor, assembler, existing foreground loader и package installer behavior. `runproj <name> [arguments]` повторно проверяет и запускает только generated regular project `main.elf` без создания или замены package; он передаёт existing native argument tail не более 127 visible bytes, а при отсутствии build просит сначала выполнить `buildproj`. Read-only `projlist` просматривает максимум 128 valid project directories и показывает fixed status каждого source, build и package. Read-only `projstatus <name>` показывает, находится ли каждый fixed source, build и package file в состоянии `READY`, `MISSING` или `NOT REGULAR`, а для ready выводит byte size. `uninstallproj <name>` удаляет только installed regular package `main.elf`, сохраняя project source/build, поэтому `installproj` может восстановить package; absent package сообщается без mutation. `cleanproj <name>` удаляет только generated project `main.elf`, сохраняя source и installed package, поэтому последующий `buildproj` может создать output снова. `rmproj <name>` — final project-workspace cleanup: после `cleanproj` он принимает только known project entries, удаляет regular `main.mya`, затем его now-empty project directory и сохраняет installed package runnable. Project name длиной 16–31 characters по-прежнему запускается из shell, но его package намеренно не получает GUI launcher tile, потому что launcher names ограничены 15 printable characters. Для multi-line source используйте общий command `edit`; `write` остаётся удобным для short one-line files.

```text
newproj native-args args
buildproj native-args
runproj native-args hello MyOS
installproj native-args
run native-args hello MyOS
projlist
uninstallproj native-args
projstatus native-args
projlist
installproj native-args
cleanproj native-args
buildproj native-args
runproj native-args
cleanproj native-args
rmproj native-args
projlist
run native-args hello MyOS
```

Fixed starter `args` выводит `[hello MyOS]` при forwarded arguments и `[]` без parameters. Чтобы заменить его authored one-line source, используйте `write /users/myos/projects/native-args/main.mya write "[";args;write "]\n";time;exit 37`, затем rebuild; эта program также выводит текущее время RTC в формате `HH:MM:SS` и возвращает status `37`. Source language supports `args`, `input`, `time`, `set <0..255>`, `not`, `neg`, `inc`, `dec`, `test <0..255>`, `and <0..255>`, `or <0..255>`, `xor <0..255>`, `shl <1..7>`, `shr <1..7>`, `rol <1..7>`, `ror <1..7>`, `add <0..255>`, `sub <0..255>`, `mul <0..255>`, `div <1..255>`, `mod <1..255>`, `store <0..7>`, `load <0..7>`, `cmp <0..7>`, `swap <0..7>`, `label name:`, `write "text"`, `jump name`, `jump_if_zero name`, `jump_if_nonzero name`, `jump_if <0..255> name` and final `exit <0..255>`. `store` сохраняет current condition byte в одном из восьми private slots; `load` восстанавливает его как condition и допустим перед arithmetic или conditional jump. `not`, `neg`, `inc`, `dec`, `test`, `and`, `or`, `xor`, `shl`, `shr`, `rol`, `ror`, `add`, `sub` и `mul` требуют initialized condition от `input`, `set` или `load`. Bitwise operations обновляют этот byte; operand-free `neg` заменяет его two’s-complement отрицанием байта modulo 256 (`set 5; neg` даёт `251`, а zero остаётся zero); operand-free `inc` увеличивает byte modulo 256 (`set 255; inc` даёт `0`); operand-free `dec` уменьшает byte modulo 256 (`set 0; dec` даёт `255`); `test <mask>` нормализует byte-wise intersection в `1` при nonzero или `0` при zero; `shl` и `shr` выполняют logical byte shifts ровно на 1–7 positions, отбрасывая shifted-out bits; `rol` и `ror` циклически поворачивают byte в том же диапазоне; `add`, `sub` и `mul` обновляют его modulo 256. `div` имеет то же prerequisite, получает nonzero divisor из `1..255` и заменяет byte его unsigned integer quotient; `mod` использует тот же divisor range и заменяет byte unsigned remainder. `cmp <slot>` также требует initialized condition, сравнивает его с выбранным private slot и заменяет на `0` при equality или `1` при inequality. `swap <slot>` имеет тот же prerequisite и обменивает accumulator byte с этим private slot. Slots private для running program, zero-initialized и не имеют имён или direct addressing. Every target must be a defined label located later in source, so loops and backward jumps are rejected. Escapes `\n`, `\r`, `\t`, `\\` and `\"` are available inside text. Например, `set 250; add 8; store 3; set 0; load 3; sub 2; jump_if_zero matched` показывает, что `(250 + 8 - 2) mod 256` равно zero. Sequence `set 200; mul 2; add 57; div 3` вычисляет `67`: multiplication сохраняет low byte, а division является unsigned integer division. Sequence `set 73; store 5; set 73; cmp 5; jump_if_zero equal` выбирает `equal`, а active value `72` заставляет `jump_if_nonzero` выбрать inequality path. Slot number вне `0..7`, operands add/sub/mul/and/or/xor вне `0..255`, counts `shl`/`shr`/`rol`/`ror` вне `1..7`, `div 0`, `mod 0`, uninitialized byte operations including `neg`, `inc`, `dec` или `test`, а также uninitialized или out-of-range `cmp` или `swap` отклоняются. The generated program runs in ring 3 and returns its authored exit status; используйте `help asm` и `help edit` для краткой command help, [Текстовый редактор](TEXT_EDITOR_RU.md) для controls и [Native Build](NATIVE_BUILD_RU.md) для всех bounds и syntax rules.

> `runproj <name> [arguments]` is the bounded development exception: it accepts only the fixed generated regular `/users/myos/projects/<name>/main.elf` after revalidation, forwards at most 127 visible native-argument bytes and does not create or replace a package. `install` remains the explicit package boundary for persistent applications and GUI launcher discovery.

## 9. Experimental GUI

GUI доступен в QEMU-validated ветке `main` и запускается из console, а не автоматически:

```text
startgui
# Compatibility alias: startgui home
```

Без аргумента `startgui` открывает **MYOS DESKTOP** — bounded mouse-first launcher; `startgui home` остаётся alias. Click по `SYSTEM` открывает system message, по `NOTES` — notes, по `EDIT NOTE` — editor default personal note. До четырёх installed packages с `/apps/<name>/main.elf` также появляются под fixed tiles как `OPEN APP`; click по одному запускает программу, закрывает GUI и возвращает её normal output в console. Click по `FILES` запускает путь `/users/myos/`; `startgui projects` сразу открывает тот же File Workspace в `/users/myos/projects`, а `startgui project <name>` открывает один existing project directory после revalidation его bounded name из 1–31 characters и directory type. Exact suffix `edit` открывает только revalidated regular writable `main.mya` этого project в existing GUI editor. Exact read-only suffix `status` выводит fixed source, build и installed-package rows `READY <size> bytes`, `MISSING` или `NOT REGULAR` без mutation. Exact suffix `build` повторно проверяет только fixed regular source и запускает established assembler для `main.mya → main.elf` этого project, закрывая GUI, пока result остаётся в console, и затем завершаясь с assembler status. Exact suffix `run [arguments]` повторно проверяет только fixed regular `main.elf` этого project, запускает только этот bounded generated output, передаёт не более existing native tail из 127 visible bytes и затем закрывает GUI, возвращая child status. Exact suffix `install` повторно проверяет только тот же fixed regular output и запускает established installer только для `main.elf → /apps/<name>/main.elf` этого project, закрывая GUI и возвращая installer status; replacement existing package остаётся intentional. Invalid либо absent project request сообщает `UNABLE TO OPEN PROJECT`; missing или non-regular direct source сообщает `UNABLE TO OPEN PROJECT SOURCE`, а missing или non-regular direct output — `UNABLE TO OPEN PROJECT OUTPUT`, без открытия viewer. Title его окна показывает полный current logical-VFS path и обновляется после parent или child navigation. Click по top-bar `X` выполняет выход. Launcher и window actions выполняются только мышью. Сохранившиеся GUI-level keyboard shortcuts: `Alt+Tab` переводит focus на следующее видимое окно, `Alt+F4` закрывает focused window, `Esc` возвращает или отменяет, а `Ctrl+Q` выходит. Для personal note можно передать absolute path:

```text
startgui /users/myos/files/notes/note
# Project root:
startgui projects
# Exact existing project workspace:
startgui project native-args
# Exact project source editor:
startgui project native-args edit
# Read-only project lifecycle view:
startgui project native-args status
# Build fixed project output and return to console:
startgui project native-args build
# Run only fixed generated project output with optional bounded native tail:
startgui project native-args run hello MyOS
# Install only fixed generated output into matching package:
startgui project native-args install
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

MyOS не является заменой Linux, Windows или BSD. В QEMU-validated ветке `main` пока нет сети, USB HID, SMP, Secure Boot, demand paging, package manager, user accounts/permissions, полноценного native C compiler или production security hardening. Restricted native assembler реализован, но GUI остаётся bounded framebuffer environment, а не general-purpose desktop. Physical-PC validation пока недоступна, поэтому публикуемые artifacts остаются Pre-releases.

Если сборка или запуск не работают, выполните `make clean`, затем `make all img`, `make smoke` и `make regression`. Smoke command headlessly проверяет BIOS и UEFI boot markers, persistent AHCI mount и automatic `[myos]$` entry. Regression command использует disposable image copy: он создаёт и сохраняет default GUI note через mouse tile `EDIT NOTE`, вводит QMP PS/2 `Alt+Tab` для focus MONITOR, `Alt+F4` для закрытия focused MONITOR, `Esc` для viewer return, `Alt+F4` для editor cancel-to-viewer и `Ctrl+Q` для clean exit, затем проверяет centered launcher tiles NOTES и FILES, включая visible transitions title текущего пути File Workspace при parent и `/system` navigation, оконные controls закрытия SYSTEM/MONITOR, подъём MONITOR по title bar, viewer close-to-home и editor cancel-to-viewer через PPM framebuffer transitions. Он также сохраняет BIOS проверку alias `startgui home`, использует direct shell `cp` для копирования editor-authored file размером 305 bytes через 256-byte request boundary, сохраняет `run cp` compatibility rejection, проверяет exact target data и отклоняет overwrite, затем проверяет direct `wc` на persisted file 259 bytes, чьё final word пересекает boundary chunk 256 bytes, и сохраняет `run wc` compatibility check, собирает и устанавливает native packages в BIOS, проверяет legacy forward-only branches, empty и forwarded native arguments, exact-match и fallback paths инструкции `input`, корректный вывод RTC `HH:MM:SS`, modular add/sub arithmetic `(250 + 8 - 2) mod 256` с rejected uninitialized `add`, persisted multiply/divide arithmetic `MULDIV` с rejected `div 0`, persisted private-slot comparison `EQ`/`NE` с rejected uninitialized или slot-`8` `cmp` и rejected invalid control flow, затем проверяет persisted files, `cp` target и installed input/time/argument/arithmetic packages через UEFI. Обе команды не заменяют physical-PC test. После этого повторите QEMU command из раздела 3. Для host-platform setup используйте [PLATFORMS_RU.md](PLATFORMS_RU.md), для release gates — [RELEASE_STABILIZATION_RU.md](RELEASE_STABILIZATION_RU.md), а для технической диагностики — [DEVELOPER_GUIDE_RU.md](DEVELOPER_GUIDE_RU.md).
