# MyOS

**MyOS** — экспериментальная операционная система для **x86_64**, написанная с нуля на freestanding C11 и x86_64 NASM. Limine используется только как текущий bootloader; ядро, memory management, scheduler, ring-3 processes, shell, filesystem и drivers реализованы в этом repository.

> Исходный код и полная Git history опубликованы в [`matveyb9/MyOS`](https://github.com/matveyb9/MyOS). Последний стабильный console release зафиксирован тегом [`v0.12.1-console`](docs/RELEASES_RU.md); исходный completion point [`v0.12.0-console`](docs/RELEASES_RU.md) остаётся неизменяемым. Актуальная GUI, MYPFS004 и native-build development line находится в отдельной ветке `gui/bringup` и пока не является stable GUI release.

## Что готово

| Область | Текущее состояние `gui/bringup` |
|---|---|
| Загрузка | BIOS и UEFI/OVMF в QEMU; `myos.iso` и raw GPT `myos.img`. |
| Kernel | GDT, IDT, TSS, PMM, paging, heap, PIT, PS/2 keyboard/mouse, RTC, ACPI, PCI и AHCI. |
| Processes | Ring 3, ELF loader, scheduler, `wait`, `kill`, `sleep`, arguments и pipes. |
| Console | Kernel shell, user shell, improved onboarding, history, Tab completion, signed `calc`, `clear` и environment variables. |
| Files | Unified MYPFS004 root: read-only `/system/core`, persistent `/system/data`, `/apps`, `/users/myos`, RAM `/temp` и read-only runtime `/system/live`; regular files до 8 MiB. |
| GUI | Framebuffer desktop, `SYSTEM`/`NOTES`/`MONITOR` windows, cursor-aware note editor, PS/2 pointer, cursor-only movement refresh и return to shell. |
| Native development | SDK для host-built freestanding C11 apps, persistent `install`/`run`, and bounded in-OS `asm`/`build` workflow. |

## Быстрый старт

### 1. Получите исходники

```bash
git clone https://github.com/matveyb9/MyOS.git myos
cd myos
# Для актуальной GUI development line:
git switch gui/bringup
```

Если project был получен как ZIP, распакуйте его, откройте корневой каталог репозитория в терминале и помните: ZIP не содержит Git history, branches и tags.

### 2. Соберите artifacts

```bash
make all img
```

| Artifact | Для чего нужен |
|---|---|
| `myos.iso` | Быстрый BIOS/UEFI test как ISO/CD в QEMU. |
| `myos.img` | Рекомендуемый disk/USB image с persistent MYPFS004 storage и unified root `/`. |

### 3. Запустите в QEMU

Для полного console experience, включая persistent files, используйте raw image:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

После boot MyOS автоматически откроет user shell через три секунды. Нажмите `K` во время countdown только если нужен diagnostic kernel shell; затем попробуйте:

```text
help
uname
ls /
write /users/myos/files/note.txt Hello MyOS
cat /users/myos/files/note.txt
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
| [GUI bring-up manual](docs/GUI_BRINGUP_RU.md) | `gui/bringup` only | Experimental framebuffer desktop controls and validation. |
| [Filesystem specification](docs/FILESYSTEM_SPEC_RU.md) | Users and contributors | Unified root layout, path contract and runtime projection. |
| [MYPFS004 storage](docs/MYPFS004_STORAGE_RU.md) | Users and contributors | 8 MiB dynamic multi-extent persistent storage and migration contract. |
| [MyOS SDK](docs/SDK_RU.md) | User-program authors | External freestanding C11 build and persistent ELF workflow. |
| [Native build guide](docs/NATIVE_BUILD_RU.md) | User-program authors | In-MyOS restricted assembler, source syntax and project-to-package workflow. |

## Build and repository actions

| Action | Command |
|---|---|
| Build ISO | `make` or `make all` |
| Build disk/USB image | `make img` |
| BIOS ISO test | `make run` |
| BIOS graphical ISO test | `make run-graphic` |
| UEFI ISO test | `make run-uefi` |
| BIOS + UEFI raw-image boot smoke | `make smoke` |
| Inspect kernel ELF | `make inspect` |
| Clean generated files | `make clean` |
| Deep-clean Limine dependency too | `make distclean` |

`make img` intentionally recreates `myos.img`; any existing persistent MYPFS004 files and application packages inside the prior image are erased. `make smoke` boots that raw image headlessly through BIOS and UEFI, verifies firmware, persistent AHCI mount and automatic `[myos]$` entry, then stops both guests. It is a boot baseline rather than a substitute for interactive GUI, filesystem or native-program tests.

## Git model

| Reference | Purpose |
|---|---|
| `main` | Current console-maintenance branch and documentation baseline. |
| `console-stable` | Latest reviewed console release baseline (`v0.12.1-console`). |
| `v0.12.1-console` | Immutable annotated tag for the refreshed console UX release. |
| `v0.12.0-console` | Immutable original console completion tag, preserved for history. |
| `gui/bringup` | Актуальная GUI development branch: MYPFS004, SDK, native `asm`/`build` и GUI hardening. Она не является stable GUI release. |

## Current limits

MyOS is an educational experimental OS, not a production desktop OS. Current GUI development still does not provide networking, USB HID, SMP, Secure Boot, demand paging, dynamic linking, package management, a full native C compiler, physical-PC release validation or production security hardening.

## Documentation maintenance promise

Documentation is maintained continuously. Every change that affects build/run commands, supported hosts, public shell commands, user-visible behavior, file layout, ABI, storage format, Git workflow or safety warnings must update the appropriate README or file in `docs/` in the **same commit**. See [the detailed policy](docs/DOCUMENTATION_POLICY_RU.md).

## License

A project license has not yet been selected. Do not redistribute MyOS as a licensed release until a license file is added.
