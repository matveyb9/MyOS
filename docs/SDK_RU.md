# MyOS SDK: внешняя разработка пользовательских программ

> **🌐 LANGUAGE / ЯЗЫК:** **🇷🇺 РУССКИЙ** / [🇺🇸 ENGLISH](SDK.md)


## Назначение

**MyOS SDK** предоставляет минимальный, воспроизводимый путь для сборки собственных пользовательских программ MyOS на хост-компьютере. SDK создаёт статический исполняемый файл `x86_64 ELF64 ET_EXEC`, который загружается в ring 3 по адресу `0x400000` и не требует изменения или пересборки исходного кода ядра.

На данном milestone SDK преднамеренно остаётся компактным: он не содержит POSIX, динамический linker, стандартную C-библиотеку или native C compiler внутри MyOS. Restricted native assembler `asm`/`build` уже доступен как отдельный in-OS workflow, но SDK остаётся внешним путём для freestanding C11 с прямым использованием небольшой публичной syscall ABI.

| Компонент | Путь | Назначение |
|---|---|---|
| Public header | `sdk/include/myos.h` | Версия ABI, безопасные thin wrappers для доступных syscalls и контракт `myos_main`. |
| Startup object | `sdk/lib/crt0.c` | Реализует `_start`, вызывает программу и передаёт её return code в `MYOS_SYS_EXIT`. |
| Linker script | `sdk/myos-user.ld` | Формирует статический ELF64 с entry point `_start` и loadable segments для MyOS loader. |
| Build template | `sdk/Makefile` | Собирает заданный C-файл в готовый MyOS ELF. |
| Проверочный пример | `sdk/examples/hello.c` | Выводит сообщение и принятую строку аргументов. |
| Практический SDK tool | `sdk/examples/cp.c` | Копирует existing regular file в новый absolute target только через public SDK VFS wrappers. Образ stage-ит его как `/system/core/apps/cp.elf`. |

## Требования и сборка

Сборка выполняется на хосте с `gcc`, `ld` из GNU binutils и GNU Make. В корне репозитория выполните следующую команду:

```bash
make -C sdk APP=sdk/examples/hello.c OUT=sdk/build/sdk-hello.elf
file sdk/build/sdk-hello.elf
readelf -h -l sdk/build/sdk-hello.elf
```

Ожидаемый результат `file` — **ELF 64-bit executable, x86-64, statically linked**. Команда `readelf` должна показать тип `EXEC`, архитектуру `Advanced Micro Devices X86-64`, entry point в пользовательском диапазоне и loadable segments. Для быстрого запуска стандартного примера допустима сокращённая команда `make -C sdk`; её результатом будет `sdk/build/hello.elf`.

Шаблон передаёт freestanding C11-флаги, отключает stack protector, PIC/PIE, red zone и SIMD-регистры. Поэтому программа не должна зависеть от libc, файлов хоста или обычного `main()`.

## Контракт программы и public ABI

Вместо обычного `main(int argc, char **argv)` приложение определяет одну функцию:

```c
#include <myos.h>

int myos_main(uint64_t argc, const char *arguments) {
    myos_write_text("Hello from a MyOS program!\n");
    return 0;
}
```

Startup object получает ABI entry `_start(uint64_t argc, const char *arguments)`, вызывает `myos_main` и завершает task с её return code. Пока shell передаёт **ровно один** текстовый аргумент: `argc` всегда равен `1`, а `arguments` указывает на NUL-terminated строку после program path. Пустая строка означает отсутствие аргументов. Это текущий контракт, а не POSIX `argv[]`.

| Элемент | Текущее правило |
|---|---|
| ABI version | `MYOS_ABI_VERSION = 0x00010000`. |
| Точка входа программы | Обязательная функция `int myos_main(uint64_t argc, const char *arguments)`. |
| Завершение | Return code `myos_main` передаётся в `MYOS_SYS_EXIT`; явный выход возможен через `myos_exit(status)`. |
| Текстовый вывод | `myos_write()` выполняет bounded write; `myos_write_text()` выводит NUL-terminated ASCII text на standard output. |
| Дополнительные wrappers | `myos_getpid()` и `myos_ticks()` доступны как прямые read-only syscall wrappers. |
| VFS subset | `myos_vfs_read()`, `myos_vfs_create_file()`, `myos_vfs_write()` и `myos_vfs_remove()` используют public fixed-size request structures. Read и write ограничены 256 bytes на request. |
| Память и runtime | Нет libc allocation, constructors, dynamic linking или floating-point runtime. |
| Формат | Только little-endian `x86_64 ELF64 ET_EXEC`; ELF32, PIE и dynamic ELF не поддерживаются. |

> **Совместимость ABI.** Номер версии фиксирует первый публичный SDK contract. При несовместимом изменении syscall wrapper или entry convention SDK получит новый ABI version; старый public header не будет молча переопределяться.

## Проверка в MyOS

Стандартная сборка образа добавляет проверочный пример в read-only initramfs как `/system/core/examples/sdk/hello.elf`. Соберите raw disk image и подключите его к QEMU как IDE drive, чтобы persistent AHCI path использовался именно в поддерживаемой конфигурации:

```bash
make img
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c -serial stdio -display none \
  -no-reboot -no-shutdown
```

После автоматического входа в `[myos]$` установите staged ELF в executable namespace и запустите его:

```text
install /system/core/examples/sdk/hello.elf /apps/sdk-hello/main.elf
run sdk-hello external SDK validation
```

Ожидаемый вывод содержит `Hello from MyOS SDK!` и строку `Arguments: external SDK validation`. `install` создаёт persistent package directory `/apps/sdk-hello/` и копирует ELF как `main.elf`; `run` создаёт новый user task и передаёт оставшуюся часть command line как аргументы. После reboot достаточно выполнить `run sdk-hello persisted`: повторная установка не требуется.

Образ также stage-ит SDK-built practical copy tool как live app `cp`. Ему нужны два absolute paths; destination не должна существовать, а её parent directory уже должна существовать. Это conservative rule предотвращает accidental overwrite или source loss. Пример:

```text
write /users/myos/files/source.txt MyOS SDK copy
run cp /users/myos/files/source.txt /users/myos/files/target.txt
cat /users/myos/files/target.txt
```

Инструмент читает и записывает запросами по 256 bytes, поддерживает empty source files и files до existing 8 MiB regular-file ceiling и удаляет только свой newly-created partial target при неудаче copy.

| Ограничение | Текущее значение |
|---|---:|
| Persistent VFS objects | До 128 файлов и каталогов в MYPFS004. |
| Максимальный размер одного persistent regular file | 8 MiB. |
| Persistent executable target | `/apps/<name>/main.elf`; короткое имя `<name>` разрешается shell в этот target. |
| Длина absolute program path | До 111 visible ASCII bytes плюс NUL terminator. |
| Длина передаваемой строки arguments | До 127 visible bytes плюс NUL terminator. |
| Initramfs staging path примера | `/system/core/examples/sdk/hello.elf`. |
| Live SDK tool path | `/system/core/apps/cp.elf`, resolved как `run cp`. |
| `cp` destination rule | Absolute path, absent target и already-existing parent directory; existing targets никогда не перезаписываются. |

## Как заменить пример своей программой

Путь `sdk/examples/hello.c` является обычным исходным файлом SDK. Его можно изменить или указать другой файл с `APP`; например, `make -C sdk APP=apps/status.c OUT=sdk/build/status.elf`. Для воспроизводимой проверки reference image собирает sample в `/system/core/examples/sdk/hello.elf`. Замените sample source, затем запустите `make img`, чтобы обновлённый ELF оказался по этому пути, после чего используйте `install /system/core/examples/sdk/hello.elf /apps/<name>/main.elf` и `run <name>`.

MYPFS004 предоставляет настоящую файловую иерархию и dynamic multi-extent regular files: исходники и local build outputs предназначены для `/users/myos/projects/`, global installed apps — для `/apps/`, а personal data и configuration — для `/users/myos/data/` и `/users/myos/config/`. Первый in-OS assembly workflow реализован через `build`; его restricted syntax и package workflow приведены в [NATIVE_BUILD_RU.md](NATIVE_BUILD_RU.md).

## Проверка milestone

Проверка выполнялась на ветке `gui/bringup` в QEMU Q35 BIOS с raw `myos.img`, подключённым через `-drive if=ide,format=raw,file=myos.img`.

| Проверка | Результат |
|---|---|
| Host build | `make -C sdk APP=sdk/examples/hello.c OUT=sdk/build/sdk-hello.elf` завершилась без warnings и errors. |
| ELF inspection | Получен statically linked `ELF64 ET_EXEC` для x86-64 с entry `0x40005f` и loadable text/rodata segments. |
| Image build | `make img` добавляет SDK sample как `/system/core/examples/sdk/hello.elf`. |
| Install and run | `install /system/core/examples/sdk/hello.elf /apps/sdk-hello/main.elf`, затем `run sdk-hello external SDK validation` вывели приветствие и полную строку аргументов; status `0`. |
| Persistence | После fresh BIOS boot `run sdk-hello persisted` успешно запускает ранее установленный ELF из MYPFS004 application package. |
| UEFI execution | OVMF boot с тем же `myos.img` успешно запустил persisted app командой `run sdk-hello uefi`. |
| SDK VFS copy | SDK-built live `cp` скопировал editor-authored persistent file размером 305 bytes через 256-byte request boundary, отклонил вторую overwrite attempt, а exact target data сохранились после UEFI. |

## Не входит в данный этап

SDK не добавляет 32-bit compatibility, native C compiler, dynamic linking, process `argv[]` или package manager. VFS subset намеренно не включает directory creation, listing, rename, metadata, overwrite flags или arbitrary I/O buffering. Unified hierarchy и MYPFS004 large-file storage уже реализованы; symbolic links, GUI shortcuts и личная установка приложений остаются последующими расширениями. Текущая GUI branch и immutable tags остаются без изменений.
