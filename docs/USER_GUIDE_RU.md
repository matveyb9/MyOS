# Руководство пользователя MyOS Console 0.12.0-dev

Это руководство предназначено для человека, который хочет **собрать, запустить и попробовать MyOS**, не изучая устройство ядра. MyOS — экспериментальная учебная ОС для x86_64. Используйте QEMU в первую очередь; запуск на физическом компьютере выполняйте только с отдельной тестовой флешкой.

> В этом release нет готового графического интерфейса. После загрузки доступна консольная оболочка с командами и небольшим набором программ.

## 1. Что понадобится

Для запуска в QEMU нужен компьютер с Linux и следующие программы: `gcc`, `make`, `nasm`, `ld`, `xorriso`, `mtools`, `sgdisk`, `qemu-system-x86_64`. Для UEFI-проверки дополнительно нужен пакет OVMF.

На Ubuntu/Debian набор обычно устанавливается так:

```bash
sudo apt update
sudo apt install build-essential nasm xorriso mtools gdisk qemu-system-x86 ovmf
```

Исходники должны находиться в каталоге проекта. Во всех следующих примерах предполагается:

```bash
cd /home/ubuntu/myos
```

## 2. Сборка

Выполните:

```bash
make all img
```

При первой сборке Make автоматически скачает пакет Limine и соберёт два файла в корне проекта.

| Файл | Когда использовать |
|---|---|
| `myos.iso` | Быстрый запуск как CD/ISO в QEMU. |
| `myos.img` | Рекомендуемый raw disk image для QEMU, USB-флешки и проверки persistent files. |

> `make img` пересоздаёт `myos.img`. Все файлы, которые были сохранены в `disk/` внутри старого образа, при этом удаляются.

## 3. Рекомендуемый запуск в QEMU

Для полной console версии с постоянными файлами используйте `myos.img`:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

Откроется окно QEMU. После загрузки появится kernel prompt:

```text
myos>
```

Введите:

```text
init
```

После этого откроется пользовательская оболочка:

```text
myos$
```

Для запуска с serial output в терминале добавьте `-serial stdio`. Если нужен только терминал без QEMU window, добавьте также `-display none`:

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c -serial stdio -display none
```

## 4. Быстрая проверка системы

После `init` попробуйте эти команды по очереди:

```text
help
uname
ps
meminfo
date
uptime
ls
cat motd.txt
```

Это подтвердит, что user shell, scheduler, память, часы, initramfs и файловая система запустились.

## 5. Работа с файлами

В MyOS есть два простых типа файлов.

| Путь | Смысл | Что происходит после restart |
|---|---|---|
| `tmp/<имя>` | Временный файл в памяти | Исчезает. |
| `disk/<имя>` | Постоянный файл на data partition `myos.img` | Сохраняется, если не пересоздавать image. |

Пример временного файла:

```text
touch tmp/test
write tmp/test Hello
cat tmp/test
rm tmp/test
```

Пример постоянного файла:

```text
touch disk/note
write disk/note My first persistent file
cat disk/note
ls
```

Закройте QEMU, затем снова загрузите **тот же** `myos.img` и выполните:

```text
init
cat disk/note
```

Текст должен сохраниться. Persistent filesystem намеренно небольшой: до 8 файлов, каждый до 512 bytes; одна команда `write` передаёт до 128 bytes текста.

## 6. Самые полезные команды shell

| Команда | Пример | Назначение |
|---|---|---|
| `help` | `help` | Список доступных команд. |
| `ls` | `ls` | Список файлов initramfs, `tmp/` и `disk/`. |
| `cat` | `cat motd.txt` | Показать файл. |
| `touch` | `touch disk/note` | Создать пустой файл. |
| `write` | `write disk/note Hello` | Перезаписать файл одной строкой. |
| `rm` | `rm disk/note` | Удалить файл. |
| `edit` | `run edit disk/note` | Открыть простой однострочный editor. |
| `ps` | `ps` | Показать процессы. |
| `sleep` | `sleep 2` | Подождать указанное число секунд. |
| `run` | `run calc 7 * 6` | Запустить программу и дождаться её завершения. |
| `spawn` | `spawn sleeper 3` | Запустить программу в фоне. |
| `wait` | `wait 4` | Дождаться процесса по PID. |
| `kill` | `kill 4` | Остановить дочерний процесс. |
| `pipe` | `pipe hello` | Передать текст через встроенный pipe workflow. |
| `set` / `get` / `env` | `set NAME Ada` | Работать с environment variables shell. |
| `reboot` | `reboot` | Перезапустить виртуальную машину. |
| `poweroff` | `poweroff` | Запросить корректное выключение через ACPI. |

### Программы из initramfs

Программы запускаются через `run` или `spawn`.

```text
run hello
run calc 12 / 3
run wc motd.txt
run grep MyOS motd.txt
run argshow one two three
run edit disk/note
```

| Программа | Назначение |
|---|---|
| `hello` | Минимальная ring-3 demo program. |
| `sleeper` | Спит заданное время; удобна для `ps`, `wait` и `kill`. |
| `orphaner` | Демонстрирует orphan handling. |
| `safety` | Проверяет user/kernel safety boundary. |
| `argshow` | Показывает полученные arguments. |
| `calc` | Выполняет простую арифметику. |
| `pipewrite`, `piperead` | Служебные programs для bounded pipes. |
| `wc` | Считает строки, слова и bytes файла. |
| `grep` | Ищет строку в файле. |
| `edit` | Меняет одну строку в `tmp/` или `disk/` файле. |

## 7. Удобства ввода

В user shell работают следующие упрощения.

| Ввод | Результат |
|---|---|
| Up / Down | Переход по ограниченной истории команд. |
| Tab | Завершение уникальной команды или уникального пути файла. |
| `$NAME` | Подстановка ранее установленной переменной environment. |

Пример:

```text
set NAME MyOS
write tmp/greeting Hello $NAME
cat tmp/greeting
```

## 8. Запуск ISO и UEFI

ISO подходит для простого boot test, но не содержит отдельного data partition для persistent files:

```bash
qemu-system-x86_64 -machine q35 -m 256M -cdrom myos.iso -boot d
```

Для UEFI с raw image:

```bash
cp /usr/share/OVMF/OVMF_VARS_4M.fd /tmp/myos-vars.fd
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=pflash,format=raw,readonly=on,file=/usr/share/OVMF/OVMF_CODE_4M.fd \
  -drive if=pflash,format=raw,file=/tmp/myos-vars.fd \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

## 9. Запись на USB-флешку

Для физического ПК используйте **`myos.img`**, а не ISO. Этот image содержит GPT, BIOS boot partition, EFI partition и отдельный data partition для `disk/`.

1. Подключите отдельную флешку без важных данных.
2. Найдите её имя:

   ```bash
   lsblk
   ```

3. Убедитесь, что выбрали диск целиком, например `/dev/sdb`, а не раздел `/dev/sdb1`.
4. Запишите образ:

   ```bash
   sudo dd if=myos.img of=/dev/sdX bs=4M conv=fsync status=progress
   sync
   ```

> Команда `dd` полностью удаляет содержимое выбранного диска. Неверный `/dev/sdX` может уничтожить данные на системном или внешнем диске. Не выполняйте команду, если не уверены в имени устройства.

## 10. Ограничения release

MyOS Console 0.12.0-dev — не замена Linux, Windows или BSD. В текущем состоянии нет сети, USB HID keyboard/mouse, SMP, Secure Boot, полноценной многооконной GUI среды, general-purpose filesystem, package manager или compatibility layer для Unix programs. Используйте ОС как учебный и экспериментальный проект.

Если сборка или запуск не работают, сначала выполните `make clean`, затем `make all img` и повторите QEMU command из раздела 3. Для технической диагностики и разработки обратитесь к [руководству разработчика](DEVELOPER_GUIDE_RU.md).
