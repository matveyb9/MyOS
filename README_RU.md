# MyOS

> **Язык:** [English](README.md) | [Русский](README_RU.md)

**MyOS** — экспериментальная операционная система для **x86_64**, написанная с нуля на freestanding C11 и x86_64 NASM. Limine пока подготавливает окружение загрузки, а ядро, управление памятью, планировщик, ring-3 программы, shell, файловая система и драйверы реализуются в этом репозитории.

## Статус

Стабильная консольная граница закреплена тегом [`v0.12.1-console`](docs/RELEASES_RU.md). Текущая работа над графическим интерфейсом и пользовательскими программами изолирована в ветке [`gui/bringup`](https://github.com/matveyb9/MyOS/tree/gui/bringup); это **экспериментальная** линия, которая не переносится в `main` автоматически. Ветка содержит BIOS/UEFI boot paths, persistent MYPFS004 storage, framebuffer GUI с bounded mouse-first desktop launcher, открываемым через `startgui`, per-window controls подъёма по title bar и закрытия, общий bounded text editor, persistent ELF packages, MyOS SDK с bounded public VFS subset и live `cp` developer tool, а также встроенный assembler с bounded program-argument forwarding, single-byte input, RTC output `HH:MM:SS`, метками, явными condition values, exact-byte comparison и forward-only безусловными или условными переходами.

| Линия | Назначение | Состояние |
|---|---|---|
| `console-stable` | Проверенный baseline консольной ОС. | Стабильная линия, тег `v0.12.1-console`. |
| `main` | Поддержка консольной ОС и документационный baseline. | Поддерживаемая консольная линия. |
| `gui/bringup` | GUI и среда разработки пользовательских программ. | Экспериментальная development line. |

## Быстрый старт

Склонируйте репозиторий и выберите нужную ветку. Команды ниже выбирают актуальную GUI development line.

```bash
git clone https://github.com/matveyb9/MyOS.git myos
cd myos
git switch gui/bringup
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
ls /
run hello
startgui
```

> `make img` пересоздаёт `myos.img` и удаляет прежние persistent data MyOS. Перед экспериментами создайте отдельную копию образа. Для проверки на физической USB-флешке используйте `myos.img`, а не ISO, и следуйте предупреждениям из руководства пользователя.

## Документация

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

## Проверка

```bash
make smoke          # BIOS и UEFI boot markers
make regression     # disposable-image GUI и native workflow
make release-check  # clean rebuild, checks и SHA-256 evidence
```

`make regression` использует disposable copy `myos.img`: он покрывает QMP-injected PS/2 `Alt+Tab` focus, `Ctrl+B` desktop return и `Alt+F4` clean exit в BIOS/UEFI, затем mouse actions для centered launcher tile `NOTES`, оконных controls закрытия SYSTEM/MONITOR, подъёма MONITOR по title bar, viewer close-to-home и editor cancel-to-viewer со screenshot transitions framebuffer. Также проверяются retained alias `startgui home` в BIOS, GUI/editor workflow, 305-byte SDK `cp` copy через VFS request boundary, его no-overwrite rule и UEFI persistence, native forward-only control flow, empty и forwarded native program arguments, input true/fallback branches и valid RTC output `HH:MM:SS`. `make release-check` выполняет только локальную проверку. Он не создаёт tag, GitHub Release или Pre-release.

---

MyOS — учебно-практический эксперимент, а не готовая desktop ОС. В текущую область не входят сеть, USB HID, SMP, Secure Boot, dynamic linking, полноценный native C compiler и physical-PC release validation. Лицензия проекта пока не выбрана.
