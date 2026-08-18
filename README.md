# MyOS

**MyOS** — экспериментальная операционная система для **x86_64**, написанная с нуля на freestanding C11 и x86_64 NASM. Limine используется только как текущий bootloader; ядро, memory management, scheduler, ring-3 processes, shell, filesystem и drivers реализованы в этом repository.

> Console OS завершена и зафиксирована тегом [`v0.12.0-console`](docs/RELEASES_RU.md). GUI-разработка хранится отдельно в ветке `gui/bringup`; она не является частью console release.

## Что готово

| Область | Возможности console release |
|---|---|
| Загрузка | BIOS и UEFI/OVMF в QEMU; `myos.iso` и raw GPT `myos.img`. |
| Kernel | GDT, IDT, TSS, PMM, paging, heap, PIT, PS/2 keyboard, RTC, ACPI, PCI и AHCI. |
| Processes | Ring 3, ELF loader, scheduler, `wait`, `kill`, `sleep`, arguments и pipes. |
| Console | Kernel shell, user shell, history, Tab completion и environment variables. |
| Files | Initramfs, временные `tmp/` files и persistent `disk/` files в isolated AHCI data partition. |
| Utilities | `calc`, `wc`, `grep`, `edit`, `hello`, `sleeper`, `argshow` и другие. |

## Быстрый старт

### 1. Получите исходники

```bash
git clone <URL-вашего-репозитория> myos
cd myos
```

Если project был получен как ZIP, распакуйте его и откройте каталог `myos-complete-project` в терминале.

### 2. Соберите artifacts

```bash
make all img
```

| Artifact | Для чего нужен |
|---|---|
| `myos.iso` | Быстрый BIOS/UEFI test как ISO/CD в QEMU. |
| `myos.img` | Рекомендуемый disk/USB image с persistent `disk/` storage. |

### 3. Запустите в QEMU

Для полного console experience, включая persistent files, используйте raw image:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

После boot введите `init`, чтобы открыть user shell, затем попробуйте:

```text
help
uname
touch disk/note
write disk/note Hello MyOS
cat disk/note
```

## Платформы разработки и запуска

| Host platform | Рекомендуемый путь | Статус |
|---|---|---|
| Linux | Native build + QEMU | Основной и проверенный путь. |
| Windows 10/11 | WSL 2 + Ubuntu + QEMU | Рекомендуемый Windows path. |
| Windows 10/11 | Native MSYS2 + QEMU | Возможен, но менее проверен, чем WSL. |
| macOS | Homebrew toolchain + QEMU | Experimental host path. |
| Other Unix-like hosts | Подобрать equivalent tools | Community/experimental path. |

Подробные команды для Linux, Windows, WSL, MSYS2 и macOS находятся в [руководстве по платформам](docs/PLATFORMS_RU.md).

> Для записи на реальную USB-флешку используйте `myos.img`, а не ISO. Запись через `dd` или аналог полностью стирает выбранный device. Полную безопасную процедуру смотрите в [руководстве пользователя](docs/USER_GUIDE_RU.md).

## Документация

| Документ | Для кого | Содержание |
|---|---|---|
| [Руководство пользователя](docs/USER_GUIDE_RU.md) | Обычные пользователи | QEMU, shell, files, utilities, persistence и USB testing. |
| [Руководство по платформам](docs/PLATFORMS_RU.md) | Linux, Windows, macOS users | Installation prerequisites and launch paths. |
| [Руководство разработчика](docs/DEVELOPER_GUIDE_RU.md) | Contributors | Architecture, source tree, ABI, storage invariants and validation. |
| [Правила документации](docs/DOCUMENTATION_POLICY_RU.md) | Maintainers | What must be updated with every project change. |
| [Releases and branches](docs/RELEASES_RU.md) | All readers | Meaning of `main`, `console-stable`, tags and GUI branch. |
| [Documentation index](docs/README.md) | All readers | Current manuals versus historical development notes. |

## Build and repository actions

| Action | Command |
|---|---|
| Build ISO | `make` or `make all` |
| Build disk/USB image | `make img` |
| BIOS ISO test | `make run` |
| BIOS graphical ISO test | `make run-graphic` |
| UEFI ISO test | `make run-uefi` |
| Inspect kernel ELF | `make inspect` |
| Clean generated files | `make clean` |
| Deep-clean Limine dependency too | `make distclean` |

`make img` intentionally recreates `myos.img`; any existing persistent `disk/` files inside the prior image are erased.

## Git model

| Reference | Purpose |
|---|---|
| `main` | Current console-maintenance branch and documentation baseline. |
| `console-stable` | Strict original console snapshot. |
| `v0.12.0-console` | Immutable annotated tag marking completed console OS. |
| `gui/bringup` | Separate GUI experiment branch. |

## Current limits

MyOS is an educational experimental OS, not a production desktop OS. The console release does not yet provide networking, USB HID, SMP, Secure Boot, demand paging, a general-purpose filesystem, package management or production security hardening.

## Documentation maintenance promise

Documentation is maintained continuously. Every change that affects build/run commands, supported hosts, public shell commands, user-visible behavior, file layout, ABI, storage format, Git workflow or safety warnings must update the appropriate README or file in `docs/` in the **same commit**. See [the detailed policy](docs/DOCUMENTATION_POLICY_RU.md).

## License

A project license has not yet been selected. Do not redistribute MyOS as a licensed release until a license file is added.
