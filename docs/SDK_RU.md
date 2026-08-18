# MyOS SDK: внешняя разработка пользовательских программ

## Назначение

**MyOS SDK** предоставляет минимальный, воспроизводимый путь для сборки собственных пользовательских программ MyOS на хост-компьютере. SDK создаёт статический исполняемый файл `x86_64 ELF64 ET_EXEC`, который загружается в ring 3 по адресу `0x400000` и не требует изменения или пересборки исходного кода ядра.

На данном milestone SDK преднамеренно остаётся компактным: он не содержит POSIX, динамический linker, стандартную C-библиотеку или native compiler внутри MyOS. Это внешний workflow для freestanding C11 с прямым использованием небольшой публичной syscall ABI.

| Компонент | Путь | Назначение |
|---|---|---|
| Public header | `sdk/include/myos.h` | Версия ABI, безопасные thin wrappers для доступных syscalls и контракт `myos_main`. |
| Startup object | `sdk/lib/crt0.c` | Реализует `_start`, вызывает программу и передаёт её return code в `MYOS_SYS_EXIT`. |
| Linker script | `sdk/myos-user.ld` | Формирует статический ELF64 с entry point `_start` и loadable segments для MyOS loader. |
| Build template | `sdk/Makefile` | Собирает заданный C-файл в готовый MyOS ELF. |
| Проверочный пример | `sdk/examples/hello.c` | Выводит сообщение и принятую строку аргументов. |

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
| Память и runtime | Нет libc allocation, constructors, dynamic linking или floating-point runtime. |
| Формат | Только little-endian `x86_64 ELF64 ET_EXEC`; ELF32, PIE и dynamic ELF не поддерживаются. |

> **Совместимость ABI.** Номер версии фиксирует первый публичный SDK contract. При несовместимом изменении syscall wrapper или entry convention SDK получит новый ABI version; старый public header не будет молча переопределяться.

## Проверка в MyOS

Стандартная сборка образа добавляет проверочный пример в initramfs под именем `sdk/hello`. Соберите raw disk image и подключите его к QEMU как IDE drive, чтобы persistent AHCI path использовался именно в поддерживаемой конфигурации:

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
install sdk/hello disk/bin/sdk-hello
run disk/bin/sdk-hello external SDK validation
```

Ожидаемый вывод содержит `Hello from MyOS SDK!` и строку `Arguments: external SDK validation`. `install` копирует файл в persistent storage, а `run` создаёт новый user task и передаёт оставшуюся часть command line как аргументы. После reboot достаточно выполнить `run disk/bin/sdk-hello persisted`: повторная установка не требуется.

| Ограничение | Текущее значение |
|---|---:|
| Persistent executable records | До 8. |
| Максимальный размер одного persistent файла | 32 KiB. |
| Persistent executable target | Только `disk/bin/<name>`. |
| Длина program path | До 63 visible bytes плюс NUL terminator. |
| Длина передаваемой строки arguments | До 127 visible bytes плюс NUL terminator. |
| Initramfs staging name примера | `sdk/hello`. |

## Как заменить пример своей программой

Путь `sdk/examples/hello.c` является обычным исходным файлом SDK. Его можно изменить или указать другой файл с `APP`; например, `make -C sdk APP=apps/status.c OUT=sdk/build/status.elf`. Для проверки custom ELF текущему этапу нужен путь staging в VFS: стандартный image build содержит `sdk/hello` именно как воспроизводимый reference path. Замените sample source, затем запустите `make img`, чтобы обновлённый ELF оказался под `sdk/hello` в initramfs, после чего используйте `install sdk/hello disk/bin/<name>` и `run disk/bin/<name>`.

Это временный workflow намеренно не является общей передачей произвольных файлов с хоста и не является файловой иерархией. Следующий этап — developer filesystem workflow — будет спроектирован отдельно до реализации: пользователь и проект должны сначала согласовать имена и структуру будущих каталогов. Нативная сборка исходников прямо в MyOS также остаётся следующим самостоятельным milestone.

## Проверка milestone

Проверка выполнялась на ветке `gui/bringup` в QEMU Q35 BIOS с raw `myos.img`, подключённым через `-drive if=ide,format=raw,file=myos.img`.

| Проверка | Результат |
|---|---|
| Host build | `make -C sdk APP=sdk/examples/hello.c OUT=sdk/build/sdk-hello.elf` завершилась без warnings и errors. |
| ELF inspection | Получен statically linked `ELF64 ET_EXEC` для x86-64 с entry `0x40005f` и loadable text/rodata segments. |
| Image build | `make img` добавила SDK sample как initramfs file `sdk/hello`. |
| Install and run | `install sdk/hello disk/bin/sdk-hello`, затем `run disk/bin/sdk-hello external SDK validation` вывели приветствие и полную строку аргументов; status `0`. |
| Persistence | После fresh BIOS boot `run disk/bin/sdk-hello persisted` успешно запустила ранее установленный ELF; AHCI probe отметил prior persistent pattern как `present`. |

## Не входит в данный этап

SDK не добавляет 32-bit compatibility, native C compiler, редактор исходников, dynamic linking, process `argv[]`, package manager или general hierarchical filesystem. Эти возможности будут рассматриваться только в последующих согласованных milestones; текущая GUI branch и immutable tags остаются без изменений.
