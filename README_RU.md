<h1 align="center">MyOS</h1>

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="README.md">🇺🇸 ENGLISH</a>
</p>

**MyOS** — экспериментальная операционная система для **x86_64**, написанная с нуля на freestanding C11 и x86_64 NASM. Limine пока подготавливает окружение загрузки, а ядро, управление памятью, планировщик, ring-3 программы, shell, файловая система и драйверы реализуются в этом репозитории.

<h2 align="center">Статус</h2>

Стабильная консольная граница закреплена тегом [`v0.12.1-console`](docs/RELEASES_RU.md). Текущая работа над графическим интерфейсом и пользовательскими программами изолирована в ветке [`feature/gui`](https://github.com/matveyb9/MyOS/tree/feature/gui); это **экспериментальная** линия, которая не переносится в `main` автоматически. Ветка содержит BIOS/UEFI boot paths, persistent MYPFS004 storage, framebuffer GUI с bounded mouse-first desktop launcher, открываемым через `startgui` и обнаруживающим до четырёх installed tiles `/apps/<name>/main.elf`, compositor-owned clock widget `HH:MM:SS`, обновляемый раз в PIT second через clock-only partial repaint, и live bounded status footer `FOCUS`/`TASKS`/`RUN`, обновляемым вместе с GUI content, per-window controls подъёма по title bar и закрытия, общий bounded text editor, persistent ELF packages, MyOS SDK с bounded public VFS subset, direct shell `cp`, backed by live developer tool, direct bounded `wc` word counting, direct bounded `grep` text search, direct bounded `tree` hierarchy view, direct bounded `find` name search, direct bounded `head` text preview, direct bounded `sort` text ordering, direct bounded `tail` text preview, direct bounded `stat` metadata lookup и packaged safe-write example `sdk-write`, native read-only bounded `tree`, `find`, `head`, `tail`, `sort`, `stat` и 12 KiB `stackprobe` VFS/platform diagnostics, four-page guarded ring-3 user stack размером 16 KiB, встроенный assembler с bounded program-argument forwarding, single-byte input, RTC output `HH:MM:SS`, восемью private byte variables `store`/`load`, bounded bitwise `not`, modular byte `neg`, `inc` и `dec`, `and`, `or` и `xor`, bounded logical byte shifts `shl`/`shr` и circular byte rotates `rol`/`ror`, modular byte arithmetic `add`/`sub`/`mul`, safe unsigned `div`, bounded unsigned-remainder `mod` и bounded private-slot `cmp` и `swap`, normalized byte-mask `test`, метками, явными condition values, exact-byte comparison и forward-only безусловными или условными переходами, а также read-only System Inventory `/system/live/` с boot, compiled-in driver, device и process records, выводимыми командой `sysinfo`, и File Workspace v1: compact desktop tile `FILES`, который начинает в `/users/myos/`, просматривает полную logical VFS с полным current path в title окна, fixed-column type и byte-size metadata, создаёт новый empty file или directory из bounded filename prompt без `/` только в existing writable roots, открывает new file в GUI editor и обновляет browser после new directory, а также открывает bounded writable text files до 16 KiB в GUI editor через до шестидесяти четырёх неизменных VFS transfers по 256 bytes без ослабления read-only или boot boundaries.

| Линия | Назначение | Состояние |
|---|---|---|
| `console-stable` | Проверенный baseline консольной ОС. | Стабильная линия, тег `v0.12.1-console`. |
| `main` | Поддержка консольной ОС и документационный baseline. | Поддерживаемая консольная линия. |
| `feature/gui` | GUI и среда разработки пользовательских программ. | Экспериментальная development line. |

<h2 align="center">Быстрый старт</h2>

Склонируйте репозиторий и выберите нужную ветку. Команды ниже выбирают актуальную GUI development line.

```bash
git clone https://github.com/matveyb9/MyOS.git myos
cd myos
git switch feature/gui
make all img
```

Запустите persistent raw disk image в QEMU. Параметр `if=ide` обязателен для проверенного AHCI persistence path.

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

Через три секунды MyOS откроет user shell. Нажимайте `K` во время countdown, только если нужен диагностический `kernel>` shell. В user shell начните с:

```text
help
sysinfo
ls /
tree /system
find tree /system/core
run head /system/core/resources/motd.txt 2
run stat /system/core/resources/motd.txt
tail /system/core/resources/motd.txt 2
run sort /system/core/resources/motd.txt
run stackprobe
run hello
startgui
# Нажмите FILES для просмотра от /users/myos
```

> `make img` пересоздаёт `myos.img` и удаляет прежние persistent data MyOS. Перед экспериментами создайте отдельную копию образа. Для проверки на физической USB-флешке используйте `myos.img`, а не ISO, и следуйте предупреждениям из руководства пользователя.

<h2 align="center">Документация</h2>

| Документ | Откройте его, если нужно… |
|---|---|
| [Карта документации](docs/README_RU.md) | Быстро перейти к актуальным и историческим материалам. |
| [Руководство пользователя](docs/USER_GUIDE_RU.md) | Запустить QEMU, использовать shell, files, persistence и безопасно записать USB. |
| [Руководство по платформам](docs/PLATFORMS_RU.md) | Настроить Linux, Windows/WSL, macOS и host tools. |
| [Руководство разработчика](docs/DEVELOPER_GUIDE_RU.md) | Понять архитектуру, source layout, ABI, storage rules и validation. |
| [Руководство по релизам](docs/RELEASES_RU.md) | Разобраться в branches, tags, release notes и двуязычном формате commits. |
| [Дорожная карта](docs/ROADMAP_RU.md) | Узнать, что завершено, что в работе и что запланировано. |
| [Руководство по GUI](docs/GUI_BRINGUP_RU.md) | Использовать experimental framebuffer desktop и понять его границы. |
| [Руководство по native build](docs/NATIVE_BUILD_RU.md) | Написать `.mya`, собрать, установить и запустить программу внутри MyOS. |
| [Политика документации](docs/DOCUMENTATION_POLICY_RU.md) | Выполнять обязательные same-commit updates документации и переводов. |

<h2 align="center">Проверка</h2>

```bash
make smoke          # BIOS и UEFI boot markers
make regression     # disposable-image GUI и native workflow
make release-check  # clean rebuild, checks и SHA-256 evidence
```

`make regression` использует disposable copy `myos.img`: он покрывает QMP-injected PS/2 `Alt+Tab` focus, `Alt+F4` закрытие focused MONITOR, `Esc` viewer return, `Alt+F4` editor cancel-to-viewer и `Ctrl+Q` clean exit в BIOS/UEFI, затем mouse actions для launcher tiles `NOTES`, `FILES` (включая title текущего пути, parent navigation, byte-size metadata и bounded flow NEW FILE prompt/editor) и обнаруженных installed-app tiles, оконных controls закрытия SYSTEM/MONITOR, подъёма MONITOR по title bar, viewer close-to-home и editor cancel-to-viewer со screenshot transitions framebuffer, включая visible regions clock, focus indicator и task status на desktop и wait, доказывающий изменение clock glyph region без GUI content input. Click по app tile запускает verified persisted package, завершает GUI session и возвращает его output в shell. Также проверяются retained alias `startgui home` в BIOS, GUI editor load/save/reload 16 KiB через deterministic initramfs fixture и шестьдесят четыре VFS chunks с exact UEFI persisted readback, плюс GUI creation zero-byte file `/users/myos/guinew` и directory `/users/myos/guidir` с UEFI type/size persistence, read-only System Inventory directory tree и `sysinfo` output в обоих firmware paths, direct bounded `tree` с retained `run tree` compatibility, direct case-insensitive `find` с retained `run find` compatibility, direct two-line `head` preview с retained `run head` compatibility, direct two-line `tail` preview с retained `run tail` compatibility, direct `sort` ASCII ordering с retained `run sort` compatibility, direct `stat` type/size VFS output с retained `run stat` compatibility, а также checksum `1566720` diagnostic `stackprobe` с automatic buffer 12 KiB в BIOS и UEFI, 305-byte direct shell `cp` copy через VFS request boundary с retained `run cp` compatibility rejection, exact direct `wc` line/word/byte output для persisted file 259 bytes, чьё final word пересекает boundary 256 bytes, с retained `run wc` compatibility, direct `grep` output короткой matching line при пропуске matching line, пересекающей limit 127 bytes, с сохранением `run grep` compatibility, packaged example `sdk-write` create/write с exact payload readback и no-overwrite behavior и их UEFI persistence, native forward-only control flow, `store`/`load` variable persistence с rejected slot `8`, modular add/sub arithmetic `(250 + 8 - 2) mod 256` с rejected uninitialized `add`, persisted multiply/divide arithmetic `MULDIV` с rejected `div 0`, bounded `BITWISE` not/and/or persistence с rejected uninitialized `not` и out-of-range `and 256`, bounded `XOR` persistence для `170 xor 255 xor 85 = 0` с rejected uninitialized и out-of-range xor, bounded `SHIFT` persistence для `3 shl 5 shr 4 = 6` с rejected uninitialized, zero и out-of-range shifts, bounded `ROTATE` persistence для `129 rol 1 ror 2 = 192` с rejected uninitialized, zero и out-of-range rotates, bounded `MOD` persistence для `200 mod 57 = 29` с rejected uninitialized и zero-divisor mod, bounded `NEG` persistence для modular byte negation (`7` становится `249`) с rejected uninitialized `neg`, bounded `INC` persistence для byte increment с wrapping (`255` становится `0`) с rejected uninitialized `inc`, bounded `DEC` persistence для byte decrement с wrapping (`0` становится `255`) с rejected uninitialized `dec`, bounded `SWAP` persistence, обменивающий byte `12` с private slot byte `73`, с rejected uninitialized `swap`, bounded `TEST` persistence для nonzero и zero byte-mask outcomes с rejected uninitialized `test`, private-slot comparison persistence `EQ`/`NE` с rejected uninitialized или slot-`8` `cmp`, empty и forwarded native program arguments, input true/fallback branches и valid RTC output `HH:MM:SS`. `make release-check` выполняет только локальную проверку. Он не создаёт tag, GitHub Release или Pre-release.

---

MyOS — учебно-практический эксперимент, а не готовая desktop ОС. В текущую область не входят сеть, USB HID, SMP, Secure Boot, dynamic linking, полноценный native C compiler и physical-PC release validation. Лицензия проекта пока не выбрана.
