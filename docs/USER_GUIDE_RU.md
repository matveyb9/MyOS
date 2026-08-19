# Руководство пользователя MyOS

> **Язык:** [English](USER_GUIDE.md) | [Русский](USER_GUIDE_RU.md)


Это руководство предназначено для человека, который хочет **собрать, запустить и попробовать MyOS**, не изучая устройство ядра. MyOS — экспериментальная учебно-практическая ОС для `x86_64`, написанная с нуля на freestanding C11 и x86_64 NASM. Используйте QEMU в первую очередь; запуск на физическом компьютере выполняйте только с отдельной тестовой флешкой.

> **Текущая линия разработки:** `gui/bringup`, версия `0.12.2-dev`. Стабильная консольная граница сохранена immutable тегом `v0.12.1-console`; GUI и MYPFS004 пока не переносятся в эту границу автоматически.

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
ps
meminfo
date
uptime
ls /
ls /system/live/processes
cat /system/core/resources/motd.txt
```

Эти команды проверяют user shell, scheduler, память, часы, initramfs, root hierarchy и read-only runtime projection.

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
| `ls` | `ls /users/myos` | Показать содержимое каталога. |
| `cat` | `cat /system/core/resources/motd.txt` | Показать file. |
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
| `startgui` | `startgui` | Запустить experimental framebuffer GUI. |
| `reboot` / `poweroff` | `reboot` | Перезагрузить или выключить виртуальную машину. |
| `clear` | `clear` | Очистить text console. |

Большинство встроенных programs запускаются через `run` или `spawn`. Примеры:

```text
run hello
run wc /system/core/resources/motd.txt
run grep MyOS /system/core/resources/motd.txt
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

SDK собирает собственные freestanding C11 programs на host computer. Подробный workflow, ABI и linker contract приведены в [SDK_RU.md](SDK_RU.md). Для первого in-OS workflow используйте restricted assembler из следующего раздела; более богатый native C frontend остаётся последующим milestone.

## 8. Native build прямо в MyOS

Native build workflow использует restricted assembler и command `build`. Source хранится в `/users/myos/projects/`, generated ELF остаётся рядом с source, а для запуска program устанавливается в global package `/apps/<name>/main.elf`. Для multi-line source используйте общий command `edit`; `write` остаётся удобным для short one-line files.

```text
mkdir /users/myos/projects/native
edit /users/myos/projects/native/forward.mya
# Наберите source lines, затем Ctrl-S:
write "Before jump\n"
jump done
write "Skipped\n"
label done:
exit 37

build /users/myos/projects/native/forward.mya /users/myos/projects/native/forward.elf
install /users/myos/projects/native/forward.elf /apps/native-forward/main.elf
run native-forward
```

Source language supports `set <0..255>`, `label name:`, `write "text"`, `jump name`, `jump_if_zero name`, `jump_if_nonzero name` and final `exit <0..255>`. Conditional jump требует более ранний `set`; every target must be a defined label located later in source, so loops and backward jumps are rejected. Escapes `\n`, `\r`, `\t`, `\\` and `\"` are available inside text. The generated program runs in ring 3 and returns its authored exit status; используйте `help asm` и `help edit` для краткой command help, [Текстовый редактор](TEXT_EDITOR_RU.md) для controls и [Native Build](NATIVE_BUILD_RU.md) для всех bounds и syntax rules.

> Project ELF files are intentionally not directly runnable. The loader accepts installed user applications only from `/apps/<name>/main.elf`, so `install` remains the explicit package boundary.

## 9. Experimental GUI

GUI доступен только в ветке `gui/bringup` и запускается из console, а не автоматически:

```text
startgui
```

Без аргумента viewer открывает `/system/core/resources/motd.txt`. Для personal note можно передать absolute path:

```text
startgui /users/myos/files/notes/note
```

`Q` или `Esc` вне editor закрывает GUI session и возвращает в тот же user shell. Обычное движение PS/2 mouse или keyboard fallback `W`/`A`/`S`/`D` теперь перерисовывает только 11×11 pointer region; full desktop refresh остаётся только для content, focus, window visibility и layout changes. Полное описание controls, notes editor и известных границ находится в [GUI_BRINGUP_RU.md](GUI_BRINGUP_RU.md).

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

MyOS не является заменой Linux, Windows или BSD. В `gui/bringup` пока нет сети, USB HID, SMP, Secure Boot, demand paging, package manager, user accounts/permissions, полноценного native C compiler или production security hardening. Restricted native assembler реализован, но GUI остаётся bounded framebuffer environment, а не general-purpose desktop.

Если сборка или запуск не работают, выполните `make clean`, затем `make all img`, `make smoke` и `make regression`. Smoke command headlessly проверяет BIOS и UEFI boot markers, persistent AHCI mount и automatic `[myos]$` entry. Regression command использует disposable image copy: он создаёт и сохраняет GUI note, собирает/устанавливает forward-jump native program в BIOS, проверяет rejected backward target, затем проверяет note и persisted program through UEFI. Обе команды не заменяют physical-PC test. После этого повторите QEMU command из раздела 3. Для host-platform setup используйте [PLATFORMS_RU.md](PLATFORMS_RU.md), для release gates — [RELEASE_STABILIZATION_RU.md](RELEASE_STABILIZATION_RU.md), а для технической диагностики — [DEVELOPER_GUIDE_RU.md](DEVELOPER_GUIDE_RU.md).
