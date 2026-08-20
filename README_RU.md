<h1 align="center">MyOS</h1>

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="README.md">🇺🇸 ENGLISH</a>
</p>

**MyOS** — экспериментальная операционная система для **x86_64**, написанная с нуля на freestanding C11 и x86_64 NASM. Limine пока подготавливает окружение загрузки, а ядро, управление памятью, планировщик, ring-3 программы, shell, файловая система и драйверы реализуются в этом репозитории.

<h2 align="center">Статус</h2>

Этот checkout — **`main`**: поддерживаемая консольная ветка с одобренными console-only maintenance и documentation updates. Экспериментальная работа над GUI и native-development остаётся в отдельной ветке `gui/bringup` и не входит в эту console branch.

| Линия | Назначение |
|---|---|
| `console-stable` | Проверенный console baseline на `v0.12.1-console`. |
| `main` | Поддерживаемая консольная ветка и документационный baseline. |
| `gui/bringup` | Отдельная экспериментальная GUI и native-development line. |

<h2 align="center">Быстрый старт</h2>

```bash
git clone https://github.com/matveyb9/MyOS.git myos
cd myos
git switch main
make all img
```

Запустите persistent raw disk image в QEMU. Параметр `if=ide` обязателен для проверенного AHCI persistence path.

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

Через три секунды MyOS откроет user shell. Нажимайте `K`, только если нужен диагностический `kernel>` shell. В user shell начните с:

```text
help
ls /
run hello
```

> `make img` пересоздаёт `myos.img` и удаляет прежние persistent data MyOS. Перед экспериментами создайте отдельную копию образа. Для physical USB testing используйте `myos.img`, а не ISO, и следуйте предупреждениям из руководства пользователя.

<h2 align="center">Документация</h2>

| Документ | Откройте его, если нужно… |
|---|---|
| [Карта документации](docs/README_RU.md) | Быстро перейти к актуальным и историческим материалам. |
| [Руководство пользователя](docs/USER_GUIDE_RU.md) | Запустить QEMU, использовать shell, files, persistence и безопасно записать USB. |
| [Руководство по платформам](docs/PLATFORMS_RU.md) | Настроить Linux, Windows/WSL, macOS и host tools. |
| [Руководство разработчика](docs/DEVELOPER_GUIDE_RU.md) | Понять архитектуру, source layout, ABI, storage rules и validation. |
| [Руководство по релизам](docs/RELEASES_RU.md) | Разобраться в branches, tags, release notes и двуязычном формате commits. |
| [Политика документации](docs/DOCUMENTATION_POLICY_RU.md) | Выполнять same-commit updates, переводы и проверку ссылок. |

<h2 align="center">Проверка</h2>

```bash
make smoke          # BIOS и UEFI boot markers
make regression     # disposable-image regression доступен в этом source tree
make release-check  # clean rebuild, checks и SHA-256 evidence
```

`make release-check` выполняет только локальную проверку. Он не создаёт tag, GitHub Release или Pre-release.

---

MyOS — учебно-практический эксперимент, а не готовая desktop ОС. В scope этой консольной ветки не входят сеть, USB HID, SMP, Secure Boot, dynamic linking, полноценный native C compiler и physical-PC release validation. Лицензия проекта пока не выбрана.
