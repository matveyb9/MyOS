<h1 align="center">MyOS</h1>

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="README.md">🇺🇸 ENGLISH</a>
</p>

**MyOS** — экспериментальная операционная система для **x86_64**, написанная с нуля на freestanding C11 и x86_64 NASM. Limine пока подготавливает окружение загрузки, а ядро, управление памятью, планировщик, ring-3 программы, shell, файловая система и драйверы реализуются в этом репозитории.

<h2 align="center">Статус</h2>

Стабильная консольная граница закреплена тегом [`v0.12.1-console`](docs/RELEASES_RU.md). Текущая работа над графическим интерфейсом и пользовательскими программами изолирована в ветке [`feature/gui`](https://github.com/matveyb9/MyOS/tree/feature/gui); это **экспериментальная** линия, которая не переносится в `main` автоматически. Ветка содержит BIOS/UEFI boot paths, persistent MYPFS004 storage, framebuffer GUI с bounded mouse-first desktop launcher, открываемым через `startgui` и обнаруживающим до четырёх installed tiles `/apps/<name>/main.elf`, compositor-owned clock widget `HH:MM:SS`, обновляемый раз в PIT second через clock-only partial repaint, и live bounded status footer `TASKS`/`RUN`, обновляемым вместе с GUI content, per-window controls подъёма по title bar и закрытия, общий bounded text editor, persistent ELF packages, MyOS SDK с bounded public VFS subset, direct shell `cp`, backed by live developer tool, direct bounded `wc` word counting и packaged safe-write example `sdk-write`, native read-only bounded `tree`, `find`, `head`, `tail`, `sort`, `stat` и 12 KiB `stackprobe` VFS/platform diagnostics, four-page guarded ring-3 user stack размером 16 KiB, встроенный assembler с bounded program-argument forwarding, single-byte input, RTC output `HH:MM:SS`, восемью private byte variables `store`/`load`, modular byte arithmetic `add`/`sub`/`mul`, safe unsigned `div` и bounded private-slot `cmp`, метками, явными condition values, exact-byte comparison и forward-only безусловными или условными переходами, а также read-only System Inventory `/system/live/` с boot, compiled-in driver, device и process records, выводимыми командой `sysinfo`, и File Workspace v1: compact desktop tile `FILES`, который начинает в `/users/myos/`, просматривает полную logical VFS с полным current path в title окна, fixed-column type и byte-size metadata и открывает bounded writable text files до 16 KiB в GUI editor через до шестидесяти четырёх неизменных VFS transfers по 256 bytes без ослабления read-only или boot boundaries.

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
run tree /system
run find tree /system/core
run head /system/core/resources/motd.txt 2
run stat /system/core/resources/motd.txt
run tail /system/core/resources/motd.txt 2
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

`make regression` использует disposable copy `myos.img`: он покрывает QMP-injected PS/2 `Alt+Tab` focus, `Alt+F4` закрытие focused MONITOR, `Esc` viewer return, `Alt+F4` editor cancel-to-viewer и `Ctrl+Q` clean exit в BIOS/UEFI, затем mouse actions для launcher tiles `NOTES`, `FILES` (включая title текущего пути, parent navigation и byte-size metadata) и обнаруженных installed-app tiles, оконных controls закрытия SYSTEM/MONITOR, подъёма MONITOR по title bar, viewer close-to-home и editor cancel-to-viewer со screenshot transitions framebuffer, включая visible regions clock и task status на desktop и wait, доказывающий изменение clock glyph region без GUI content input. Click по app tile запускает verified persisted package, завершает GUI session и возвращает его output в shell. Также проверяются retained alias `startgui home` в BIOS, GUI editor load/save/reload 16 KiB через deterministic initramfs fixture и шестьдесят четыре VFS chunks с exact UEFI persisted readback, read-only System Inventory directory tree и `sysinfo` output в обоих firmware paths, native `tree`, case-insensitive `find`, bounded two-line `head`, two-line `tail`, ASCII `sort` и `stat` type/size VFS output, а также checksum `1566720` diagnostic `stackprobe` с automatic buffer 12 KiB в BIOS и UEFI, 305-byte direct shell `cp` copy через VFS request boundary с retained `run cp` compatibility rejection, exact direct `wc` line/word/byte output для persisted file 259 bytes, чьё final word пересекает boundary 256 bytes, с retained `run wc` compatibility, packaged example `sdk-write` create/write с exact payload readback и no-overwrite behavior и их UEFI persistence, native forward-only control flow, `store`/`load` variable persistence с rejected slot `8`, modular add/sub arithmetic `(250 + 8 - 2) mod 256` с rejected uninitialized `add`, multiply/divide persistence `MULDIV` с rejected `div 0`, private-slot comparison persistence `EQ`/`NE` с rejected uninitialized или slot-`8` `cmp`, empty и forwarded native program arguments, input true/fallback branches и valid RTC output `HH:MM:SS`. `make release-check` выполняет только локальную проверку. Он не создаёт tag, GitHub Release или Pre-release.

---

MyOS — учебно-практический эксперимент, а не готовая desktop ОС. В текущую область не входят сеть, USB HID, SMP, Secure Boot, dynamic linking, полноценный native C compiler и physical-PC release validation. Лицензия проекта пока не выбрана.
