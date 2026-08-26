# Native build в MyOS: ограниченный in-OS workflow

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="NATIVE_BUILD.md">🇺🇸 ENGLISH</a>
</p>

> **Статус:** реализовано и проверено в QEMU-validated integration line `main`. Встроенный `asm` поддерживает вывод текста, bounded forwarding program arguments, ввод одного байта, вывод времени RTC, именованные метки, одно ограниченное condition value и безусловные или условные переходы только вперёд. Это не general-purpose assembler, C compiler и не замена host [MyOS SDK](SDK_RU.md).

## Назначение

Команда user shell `build` запускает встроенный инструмент `asm`. Он читает `.mya` source file, формирует static `x86_64 ELF64 ET_EXEC` в памяти и сохраняет его как обычный VFS file. Команда `install` копирует ELF в `/apps/<name>/main.elf`, а `run <name>` запускает package как отдельную ring-3 task.

Намеренно малый язык подтверждает полный путь **source → ELF → package → execution** внутри MyOS, не добавляя general-purpose compiler, linker, relocation model или writable user-controlled program data.

## Быстрый workflow

Начните project с default fixed skeleton `hello`, fixed skeleton `args` либо editable zero-length source `empty`, затем соберите и запустите его через project shortcuts.

```text
newproj native-args args
# Либо создать exact starter и открыть в shell editor только его новый fixed source:
newproj native-author empty edit
# Либо создать default runnable starter и собрать только его новую fixed source/output pair:
newproj native-built build
# Либо создать, собрать и установить default starter только через его fixed project/package paths:
newproj native-installed install
buildproj native-args
runproj native-args hello MyOS
installproj native-args
run native-args hello MyOS

# Либо создать exact starter и открыть в GUI editor только его новый fixed source:
startgui project native-author new args edit
# Либо создать default starter и собрать только его новую fixed source/output pair напрямую:
startgui project native-built new build
# Затем собрать ту же fixed source/output pair через GUI entry:
startgui project native-args build
# Затем запустить только его fixed generated output с ordinary bounded native tail:
startgui project native-args run hello MyOS
# Либо установить только этот fixed output в matching package:
startgui project native-args install
# Либо удалить только этот fixed installed package, сохраняя source/build:
startgui project native-args uninstall
# Либо удалить только этот fixed generated output, сохраняя source/package:
startgui project native-args clean
# Либо удалить только clean project workspace, сохраняя package:
startgui project native-args remove
```

`newproj` по умолчанию записывает runnable program `Hello from MyOS project`; `newproj <name> args` записывает fixed bracketed starter `args`, а `newproj <name> empty` создаёт editable regular zero-length source без zero-length write request. Добавьте только exact final token `edit`, `build` либо `install`, чтобы создать выбранный starter и затем открыть в established shell editor лишь его новый fixed source, собрать только его новую fixed source/output pair через established assembler либо собрать и затем установить только его новую fixed project/package pair. `install` сначала требует, чтобы build создал regular new `main.elf`; иначе он сохраняет completed project и не запускает installer. Later editor-launch, build либо install failure сохраняет completed project. Starter напрямую выводит `[hello MyOS]` через `runproj native-args hello MyOS` и `run native-args hello MyOS`, а без parameters выводит `[]`. Замените любой source через `edit`, чтобы написать другую program; например добавьте `time` перед final `exit 37`, чтобы вывести корректную строку `HH:MM:SS` и вернуть status `37`. Для output ELF используйте persistent project paths: generated images немного больше 4 KiB, а temporary VFS намеренно мал.

## Команды shell

| Команда | Назначение |
|---|---|
| `newproj <project-name> [hello\|args\|empty] [edit\|build\|install]` | Создаёт `/users/myos/projects/<project-name>/main.mya` из одного fixed starter: `hello` используется по умолчанию как runnable template, `args` записывает bracketed native-argument program, а `empty` создаёт editable regular zero-length source без zero-length write request. Единственный optional final token — exact `edit`, `build` либо `install`: только после successful creation он запускает established editor с этим новым fixed source, established assembler только для его новой fixed source/output pair либо собирает и затем запускает established installer только для новой fixed pair `main.elf → /apps/<name>/main.elf`. `install` требует regular new build output до installer invocation. Names — 1–31 ASCII letters, digits, `-` или `_`; existing project, unknown starter или trailing token отклоняется без создания либо overwrite. Later editor-launch, build либо install failure сохраняет completed project. |
| `editproj <project-name>` | Открывает fixed path `<project>/main.mya` через bounded program `edit`. Не создаёт file или directory. |
| `buildproj <project-name>` | Вызывает `build` через fixed project paths `<project>/main.mya` → `<project>/main.elf`. Не создаёт directory и сохраняет assembler replacement/error behavior. |
| `startgui project <project-name> new [hello\|args\|empty]` | Принимает только bounded absent project name и fixed default starter `hello`, `args` либо editable zero-length `empty`, создаёт только `<project>/main.mya`, отклоняет existing target и удаляет только own partial creation state после later ordinary failure. Он остаётся в GUI с narrow result status и не создаёт child. |
| `startgui project <project-name> new [hello\|args\|empty] edit` | Использует тот же bounded absent-name и exact-starter creation contract, затем только после успеха выбирает этот новый fixed source для existing GUI editor. Child не создаётся; ordinary editor-handoff failure сохраняет completed workspace вместо его удаления. |
| `startgui project <project-name> new [hello\|args\|empty] build` | Использует тот же bounded absent-name и exact-starter creation contract, затем только после успеха запускает established assembler лишь для новой fixed pair `<project>/main.mya → <project>/main.elf`. GUI завершается, пока child выполняется в console; assembler failure сохраняет completed workspace. |
| `startgui project <project-name> build` | Повторно проверяет existing bounded project directory и regular fixed `main.mya`, затем запускает established assembler только с fixed arguments `<project>/main.mya` и `<project>/main.elf`. GUI session завершается, пока child завершается в console, а `startgui` завершается с его assembler status; invalid project/source requests остаются в узком GUI status вместо открытия viewer. |
| `startgui project <project-name> run [arguments]` | Повторно проверяет только fixed regular `<project>/main.elf`, запускает только этот already allowlisted generated output и передаёт ordinary native argument tail не более 127 visible bytes. После successful spawn GUI завершается, ожидает и возвращает program status; invalid project/output requests остаются в узком GUI status. |
| `startgui project <project-name> install` | Повторно проверяет только fixed regular `<project>/main.elf`, запускает established installer только с этим source и fixed target `/apps/<project-name>/main.elf`, затем завершает GUI, ожидает и возвращает installer status. Established intentional package-replacement behavior сохраняется. |
| `startgui project <project-name> uninstall` | Повторно проверяет только fixed regular `/apps/<project-name>/main.elf` и удаляет лишь этот output. Он сохраняет project source/build и остаётся в GUI с narrow result status; child process не создаётся. |
| `startgui project <project-name> clean` | Повторно проверяет только fixed regular `<project>/main.elf` и удаляет лишь этот generated output. Он сохраняет project source/package и остаётся в GUI с narrow result status; child process не создаётся. |
| `startgui project <project-name> remove` | Немедленно повторно проверяет exact project directory, разрешает только regular `main.mya`, если он присутствует, и absent `main.elf`, удаляет source, затем empty directory и сохраняет `/apps/<project-name>/main.elf`. Он остаётся в GUI с narrow result status, не создаёт child и не заявляет crash-transactional deletion. |
| `runproj <project-name> [arguments]` | Повторно проверяет и запускает только regular generated `<project>/main.elf` через existing foreground loader без создания или замены package. Он передаёт existing native argument string не более 127 visible bytes; при missing build сообщает, что сначала нужен `buildproj`. |
| `installproj <project-name>` | Вызывает `install` из `<project>/main.elf` в `/apps/<project-name>/main.elf`; existing package target намеренно заменяется, как и при `install`. |
| `uninstallproj <project-name>` | Удаляет только existing regular `/apps/<project-name>/main.elf`; он сохраняет project source/build и сообщает absent package без mutation, поэтому `installproj` может его восстановить. |
| `projlist` | Просматривает максимум 128 entries `/users/myos/projects`, фильтрует valid project directories и выводит read-only fixed source/build/package status rows для каждого. |
| `projstatus <project-name>` | Читает fixed directory entries `main.mya`, `main.elf` и `/apps/<name>/main.elf`, выводит `READY <size> bytes`, `MISSING` или `NOT REGULAR`. Ничего не изменяет. |
| `cleanproj <project-name>` | Удаляет только existing regular `<project>/main.elf`. Сохраняет `main.mya` и `/apps/<name>/main.elf`; missing output сообщает без mutation, а `buildproj` может создать его снова. |
| `rmproj <project-name>` | Требует absent build, принимает только project directory с `main.mya` и/или `main.elf`, удаляет regular source, затем empty directory в этом порядке и сохраняет `/apps/<name>/main.elf`. |
| `build <source.mya> <output.elf>` | Public workflow wrapper; запускает `asm` в foreground. |
| `run asm <source.mya> <output.elf>` | Прямой вызов assembler для диагностики. |
| `help startgui` | Показывает bounded GUI entry points, включая exact project actions и `new [hello\|args\|empty] [edit\|build]`; ничего не создаёт. |
| `help asm` | Показывает краткий current syntax reference. |
| `install <source> /apps/<name>/main.elf` | Копирует ELF в executable package location. |
| `run <name>` | Разрешает и запускает `/apps/<name>/main.elf`. |

Все paths должны быть absolute. `newproj` — единственный shell project helper, создающий fixed directory и template `main.mya`; он принимает только default `hello`, starter `args` либо editable regular zero-length starter `empty` и только exact final `edit`, `build` либо `install` как optional handoff token. Он проверяет оба до любой VFS mutation, использует существующие VFS create/write operations без zero-length write для `empty`, отклоняет existing directory либо trailing token и удаляет только directory или file, которые сам успел создать, если его собственный последующий setup step завершился failure. После успеха `edit` запускается только для нового fixed source, `build` переиспользует established fixed pair `main.mya → main.elf`, а `install` сначала повторно проверяет regular new `main.elf`, затем переиспользует established fixed project/package pair. Later editor-launch, build либо install failure сохраняет completed project. `startgui project <name> new [hello|args|empty] edit` имеет тот же bounded creation behavior и только после успеха выбирает новый fixed source для established GUI editor; later ordinary handoff failure сохраняет completed project. `startgui project <name> new [hello|args|empty] build` имеет тот же bounded creation behavior и только после успеха запускает established assembler лишь для новой fixed source/output pair; GUI завершается, пока child выполняется в console, а assembler failure сохраняет completed project. Project name длиной 16–31 characters остаётся valid для shell `run`, но его installed package намеренно не получает launcher tile: GUI принимает максимум 15 printable name characters, поэтому framebuffer и user-space action mappings остаются identical. `editproj`, `buildproj`, `runproj`, `installproj`, `uninstallproj`, `projstatus`, `cleanproj` и `rmproj` принимают тот же restricted name и только формируют fixed existing project/package paths. `startgui project <name> build` принимает ту же exact pair name/suffix после direct-project revalidation, требует только regular fixed source и использует existing assembler path и два fixed source/output arguments вместо новой VFS primitive или arbitrary executable path. `startgui project <name> run [arguments]` принимает только тот же bounded name плюс exact run suffix, повторно проверяет fixed regular `main.elf`, передаёт не более existing native tail из 127 visible bytes и запускает только этот already allowlisted project output; он не добавляет VFS operation или arbitrary executable path. `startgui project <name> install` принимает только ту же exact bounded pair name/suffix, повторно проверяет fixed regular `main.elf` и запускает established installer только для этого output и fixed matching `/apps/<name>/main.elf`; он сохраняет package replacement и не добавляет VFS primitive или arbitrary target path. `startgui project <name> uninstall` принимает только ту же exact bounded pair name/suffix, повторно проверяет fixed regular installed `/apps/<name>/main.elf`, удаляет только этот path, сохраняет source/build и не добавляет VFS primitive либо arbitrary deletion target. `startgui project <name> clean` принимает только ту же exact bounded pair name/suffix, повторно проверяет fixed regular generated `<project>/main.elf`, удаляет только этот path, сохраняет source/package и не добавляет VFS primitive либо arbitrary deletion target. `startgui project <name> remove` принимает только ту же exact bounded pair name/suffix, немедленно повторно проверяет directory, разрешает только regular `main.mya`, если он присутствует, и absent `main.elf`, удаляет source, затем empty directory и сохраняет installed package; он не добавляет VFS primitive либо arbitrary deletion target. `projlist` read-only: он проверяет максимум 128 entries `/users/myos/projects`, игнорирует non-directory либо invalid-name entries и сообщает три fixed status paths каждого accepted project. `rmproj` требует revalidated exact project directory, отклоняет present или non-regular build и unexpected directory entries, затем удаляет только regular source (если есть), а после — empty project directory; он никогда не удаляет installed package и не заявляет crash-transactional deletion. `editproj`, `buildproj` и `installproj` делегируют established programs `edit`, `asm` и `install`; `uninstallproj` повторно проверяет только fixed regular installed `main.elf` перед одним remove request и сохраняет source/build; `runproj` повторно проверяет regular `main.elf`, затем без mutation делегирует existing foreground loader, передавая только ordinary native argument tail не более 127 visible bytes. Его kernel loader allowlist допускает только этот exact bounded path `/users/myos/projects/<name>/main.elf` и всё ещё отклоняет каждый иной executable path `/users/...`; `projstatus` читает не более 128 entries в каждом fixed parent directory, а `cleanproj` повторно проверяет regular `main.elf` перед одним remove request. Они не добавляют VFS operation, не меняют established package-replacement behavior и не заявляют crash-transactional persistence. Сам assembler не создаёт parent directories. Он разбирает весь source и формирует ELF до замены requested output file.

## Source language `.mya`

Statements разделяются `;` или концом строки. `#` начинает line comment. Program содержит один final `exit`; после него не допускается label или statement.

| Statement | Значение |
|---|---|
| `label name:` | Определяет уникальную case-sensitive position перед следующим instruction. `name` начинается с ASCII letter или `_`, затем содержит letters, digits или `_`. |
| `write "text"` | Отправляет text в standard output через `MYOS_SYS_WRITE`. Допускаются `\n`, `\r`, `\t`, `\\` и `\"`; empty strings отклоняются. |
| `input` | Блокируется до получения одного байта через `MYOS_SYS_READ`, отбрасывает `CR` и `LF` и сохраняет полученный unsigned byte (`0..255`) как текущее condition value. Поэтому после terminal-команды он пропускает line delimiter и принимает следующий значимый байт. |
| `time` | Читает RTC через `MYOS_SYS_RTC_TIME` и выводит одну строку из девяти байт `HH:MM:SS\n`. Source-level arguments и time arithmetic отсутствуют. |
| `args` | Выводит exact NUL-terminated argument string, переданную после `run <name>`, не более 127 visible bytes. При отсутствии parameters ничего не выводит, не добавляет newline и не изменяет текущее condition value. |
| `set <0..255>` | Сохраняет одно явное unsigned condition value для последующих arithmetic или conditional jumps. |
| `not` | Bitwise-комплементирует все bits initialized current unsigned byte. |
| `neg` | Выполняет two’s-complement отрицание initialized current unsigned byte modulo 256. |
| `inc` | Увеличивает initialized current unsigned byte на единицу modulo 256. |
| `dec` | Уменьшает initialized current unsigned byte на единицу modulo 256. |
| `clz` | Считает leading zero bits initialized byte; `0` даёт `8`, а nonzero byte — `0..7`. |
| `parity` | Нормализует initialized current byte в `1` при even parity или `0` при odd parity. |
| `test <0..255>` | Вычисляет byte-wise intersection initialized current condition и одного unsigned byte, затем нормализует condition в `1` при nonzero или `0` при zero. |
| `and <0..255>` | Выполняет bitwise AND initialized current unsigned byte с одним unsigned byte. |
| `or <0..255>` | Выполняет bitwise OR initialized current unsigned byte с одним unsigned byte. |
| `xor <0..255>` | Выполняет bitwise exclusive OR initialized current unsigned byte с одним unsigned byte. |
| `shl <1..7>` | Логически сдвигает initialized current unsigned byte влево на 1–7 positions, отбрасывая shifted-out bits. |
| `shr <1..7>` | Логически сдвигает initialized current unsigned byte вправо на 1–7 positions, отбрасывая shifted-out bits. |
| `rol <1..7>` | Циклически поворачивает initialized current unsigned byte влево на 1–7 positions, возвращая shifted-out bits в правый край. |
| `ror <1..7>` | Циклически поворачивает initialized current unsigned byte вправо на 1–7 positions, возвращая shifted-out bits в левый край. |
| `add <0..255>` | Прибавляет один unsigned byte к initialized current condition с wrapping modulo 256. |
| `sub <0..255>` | Вычитает один unsigned byte из initialized current condition с wrapping modulo 256. |
| `mul <0..255>` | Умножает initialized current condition на один unsigned byte, сохраняя low byte modulo 256. |
| `div <1..255>` | Делит initialized current condition на один nonzero unsigned byte и сохраняет unsigned integer quotient. |
| `mod <1..255>` | Делит initialized current condition на один nonzero unsigned byte и сохраняет unsigned remainder. |
| `store <0..7>` | Копирует current unsigned condition byte в один из восьми private program-variable slots. Текущее condition value не меняется. |
| `load <0..7>` | Восстанавливает private program-variable byte как current condition value для последующих conditional jumps. |
| `cmp <0..7>` | Сравнивает initialized current condition с private slot и заменяет его на `0` при равенстве или `1` при различии. |
| `swap <0..7>` | Обменивает initialized current condition byte с одним private slot. |
| `jump name` | Безусловно переходит на строго более позднюю label. |
| `jump_if_zero name` | Переходит на строго более позднюю label, только если текущее condition value равно zero. |
| `jump_if_nonzero name` | Переходит на строго более позднюю label, только если текущее condition value не равно zero. |
| `jump_if <0..255> name` | Переходит на строго более позднюю label, только если текущее condition value точно совпадает с выбранным unsigned byte. |
| `exit <0..255>` | Вызывает `MYOS_SYS_EXIT` с выбранным status; обязательный final executable statement. |

`not`, `neg`, `inc`, `dec`, `clz`, `parity`, `test`, `and`, `or`, `xor`, `shl`, `shr`, `rol`, `ror`, `add`, `sub`, `mul`, `div`, `mod`, `cmp` и `swap` требуют более ранний `input`, `set` или `load`. `not`, `neg`, `inc`, `dec`, `and`, `or`, `xor`, `shl`, `shr`, `rol`, `ror`, `add`, `sub` и `mul` сохраняют это initialized single condition value как byte; `clz` считает leading zero bits и даёт `8` для zero; `parity` нормализует even parity в `1` или odd parity в `0`; `test` вместо этого нормализует byte-wise intersection в `0` или `1`. Arithmetic wrap modulo 256, bitwise operations применяются к его восьми bits, logical shifts принимают только 1–7 positions и отбрасывают shifted-out bits, а circular rotates принимают тот же диапазон и возвращают эти bits на противоположный край. `div` заменяет его unsigned integer quotient, а `mod` — unsigned remainder; обе операции отклоняют zero source operand; `cmp` заменяет его на `0` при равенстве или `1` при различии с выбранным private slot. Conditional jump также требует это initialized value. `store` копирует его без изменения, а `write`, `time` и `label` его не очищают. Target должен существовать и быть расположен позже в source. Это сохраняет source-level paths конечными: loops, backward/current targets и indirect jumps отклоняются. `input` может ждать human или serial input, но не добавляет source-language loop.

```text
# Exact match: только заглавная A выбирает первый путь.
input
jump_if 65 matched
write "other\n"
jump done
label matched:
write "A\n"
label done:
time
exit 0
```

## Generated code и bounds

Generated entry prologue сохраняет loader-provided argument pointer в fixed private data segment. `args` заново загружает этот pointer, сканирует не более 127 bytes до NUL terminator и выполняет один write syscall, только когда supplied string не пуста. `set` генерирует `mov ebx, imm32`. `input` помещает один байт в data offset `8` через `MYOS_SYS_READ`, перезагружает scratch pointer после syscall boundary, отфильтровывает `CR` и `LF`, затем помещает принятый байт в `EBX`. `time` читает fixed layout `myos_rtc_time` по offset `8` той же private 32-byte area и форматирует часы, минуты и секунды в two decimal digits перед одним write syscall. `store` генерирует один absolute byte store из `BL` в offset `24..31`; `load` генерирует один zero-extending absolute byte load в `EBX` из выбранного slot. `not` генерирует `not bl`; `neg` генерирует `neg bl`; `inc` генерирует `inc bl`; `dec` генерирует `dec bl`; `clz` генерирует bounded zero branch и `bsr ecx, ebx; mov eax, 7; sub eax, ecx; movzx ebx, al`; `parity` генерирует `test bl, bl; setp bl; movzx ebx, bl`; `test` генерирует `test bl, imm8; setne bl; movzx ebx, bl`; `and`, `or` и `xor` генерируют соответственно `and bl, imm8`, `or bl, imm8` и `xor bl, imm8`; `shl` и `shr` генерируют `shl bl, imm8` и `shr bl, imm8`; `rol` и `ror` генерируют `rol bl, imm8` и `ror bl, imm8`, все с validated count 1–7. `add` и `sub` генерируют соответственно `add bl, imm8` и `sub bl, imm8`. `mul` генерирует `mov eax, ebx; imul eax, eax, imm32; movzx ebx, al`; `div` генерирует `mov eax, ebx; xor edx, edx; mov ecx, imm32; div ecx; movzx ebx, al`; `mod` использует ту же safe unsigned divide sequence и генерирует `movzx ebx, dl` для remainder; `cmp` генерирует `cmp bl, byte [absolute slot]; setne bl; movzx ebx, bl`; `swap` генерирует `xchg bl, byte [absolute slot]`. Required nonzero divisor и byte accumulator гарантируют, что division не вызывает trap и не выдаёт quotient выше 255, а каждая arithmetic или comparison operation восстанавливает zero-extended byte в `EBX` для последующих branches.

Каждый zero/non-zero branch генерирует `test ebx, ebx`, сразу за которым следует fixed-size `JZ rel32` или `JNZ rel32`. `jump_if` генерирует `cmp ebx, imm32`, за которым следует `JZ rel32`. Поэтому generated branches не зависят от flags, оставленных syscall или другой instruction. `jump` остаётся `E9 rel32`, а assembler разрешает все labels до формирования ELF.

| Граница | Текущее правило |
|---|---|
| Source file | Не более 2 047 bytes, чтение bounded VFS requests по 256 bytes. |
| Text literals | Не более 2 048 bytes суммарно. |
| Executable statements | Не более 64 суммарных `write`, `args`, `input`, `time`, `set`, `not`, `neg`, `inc`, `dec`, `clz`, `parity`, `test`, `and`, `or`, `xor`, `shl`, `shr`, `rol`, `ror`, `add`, `sub`, `mul`, `div`, `mod`, `store`, `load`, `cmp`, `swap`, jump и `exit` instructions. |
| Arguments | Existing loader ABI string из `run <name> [arguments]`, не более 127 visible bytes; `args` только читает и выводит её. |
| Labels | Не более 16 unique labels; identifiers содержат 1–31 ASCII characters. |
| Control flow | Forward-only `jump`, `jump_if_zero`, `jump_if_nonzero` и `jump_if`; source-level loops и indirect targets отсутствуют. |
| Condition | Одно value `0..255` в generated `EBX`, initialized `input`, `set` или `load`; `not`, `neg`, `inc`, `dec`, `and`, `or` и `xor` обновляют его восемь bits; `clz` считает leading zero bits и даёт `8` для zero; `parity` нормализует even parity в `1` или odd parity в `0`; `test` нормализует byte-wise intersection в `0` или `1`, `shl` и `shr` логически сдвигают его byte на 1–7 positions, а `rol` и `ror` циклически поворачивают его в том же диапазоне, `add`, `sub` и `mul` обновляют low byte modulo 256, `div` использует unsigned quotient, а `mod` — unsigned remainder, обе с required divisor `1..255`, а `cmp <0..7>` записывает `0` для equality или `1` для inequality с private slot. General user-addressable mutable memory отсутствует. |
| Generated ELF | Не более 8 192 bytes, запись bounded VFS writes по 256 bytes. |
| ELF layout | Один RX `PT_LOAD` по `0x400000` и fixed RW `PT_LOAD` размером 32 bytes по `0x401000`: bytes `0..7` сохраняют entry argument pointer, bytes `8..23` — private input/time scratch data, а bytes `24..31` — восемь zero-initialized slots `store`/`load`. Relocation, libc и dynamic linker отсутствуют. |

Эти границы сохраняют parsing, target resolution, code generation и storage статичными и проверяемыми. Invalid syntax, duplicate labels, store/load/cmp slot вне `0..7`, operand and/or/xor/test/add/sub/mul вне `0..255`, count shl/shr/rol/ror вне `1..7`, div/mod divisor вне `1..255`, bitwise/shift/arithmetic/cmp или conditional jump без `input`, `set` или `load`, missing/non-forward targets, malformed paths или output-write failure оставляют shell usable и возвращают non-zero status.

```text
asm: syntax error; set/load/input must precede not/neg/inc/dec/clz/parity/test/add/sub/mul/div/mod/and/or/xor/shl/shr/rol/ror/cmp/swap and conditional jumps, add/sub/mul/and/or/xor/test are byte values 0..255, shl/shr/rol/ror are 1..7, div/mod are 1..255, store/load/cmp/swap slots are 0..7, labels need ':' and jumps must target a later label
```

## Завершённая проверка

| Проверка | Результат |
|---|---|
| Strict build | `make all img` завершается с `-Werror`. |
| BIOS zero/non-zero branches | Существующие программы с `set` сохраняют поведение zero-true, zero-false и nonzero-true. |
| BIOS native input | Одна установленная программа на отдельных запусках принимает `A` и `B`, выбирает соответствующие exact-match и fallback paths и завершается со status `46`. |
| BIOS native arguments | Один installed package выводит и `[]` без parameters, и `[alpha beta]` для forwarded arguments, сохраняет valid time output и завершается со status `47`. |
| BIOS RTC output | Та же программа выводит строку, соответствующую допустимым диапазонам `HH:MM:SS`. |
| BIOS native variables | Собранная программа сохраняет `73` в slot `2`, перезаписывает active condition, загружает slot `2`, выбирает exact-match branch `VAR` и завершается со status `48`. Slot `8` отклоняется. |
| BIOS byte arithmetic | Вторая программа вычисляет `(250 + 8 - 2) mod 256`, сохраняет и повторно загружает intermediate byte, выбирает zero branch, выводит `ARITH` и завершается со status `49`; uninitialized `add` отклоняется. |
| BIOS byte leading-zero count | Программа вычисляет `clz 32 = 2` и `clz 0 = 8`, выбирает оба expected branches, выводит `CLZ` и завершается со status `63`; uninitialized `clz` отклоняется. |
| BIOS byte parity predicate | Программа нормализует byte `3` в even parity, а byte `1` в odd parity, выбирает оба expected paths, выводит `PARITY` и завершается со status `62`; uninitialized `parity` отклоняется. |
| BIOS byte test predicate | Программа нормализует `160 test 128` в nonzero, а `160 test 15` в zero, выбирает оба expected paths, выводит `TEST` и завершается со status `61`; uninitialized `test` отклоняется. |
| BIOS private-slot swap | Программа сохраняет `73` в slot `4`, устанавливает `12`, обменивает его со slot `4`, выбирает branch `73`, выводит `SWAP` и завершается со status `60`; uninitialized `swap` отклоняется. |
| BIOS byte decrement | Программа вычисляет `dec 0 = 255` modulo 256, сохраняет/загружает byte, выбирает exact branch, выводит `DEC` и завершается со status `59`; uninitialized `dec` отклоняется. |
| BIOS byte increment | Программа вычисляет `inc 255 = 0` modulo 256, сохраняет/загружает byte, выбирает zero branch, выводит `INC` и завершается со status `58`; uninitialized `inc` отклоняется. |
| BIOS byte negation | Программа вычисляет `neg 7 = 249` modulo 256, сохраняет/загружает byte, выбирает exact branch, выводит `NEG` и завершается со status `57`; uninitialized `neg` отклоняется. |
| BIOS bitwise byte operations | Программа инициализирует byte `240`, применяет operand-free `not`, затем `and 63` и `or 128`, получает byte `143`, сохраняет/загружает его, выбирает exact branch, выводит `BITWISE` и завершается со status `52`; uninitialized `not` и `and 256` отклоняются. |
| BIOS exclusive-or byte operation | Программа вычисляет `170 xor 255 xor 85 = 0`, сохраняет/загружает byte, выбирает zero branch, выводит `XOR` и завершается со status `53`; uninitialized `xor` и `xor 256` отклоняются. |
| BIOS logical byte shifts | Программа вычисляет `3 shl 5 shr 4 = 6`, сохраняет/загружает byte, выбирает exact branch, выводит `SHIFT` и завершается со status `54`; uninitialized shifts, `shl 0` и `shr 8` отклоняются. |
| BIOS byte rotates | Программа вычисляет `129 rol 1 ror 2 = 192`, сохраняет/загружает byte, выбирает exact branch, выводит `ROTATE` и завершается со status `55`; uninitialized rotates, `rol 0` и `ror 8` отклоняются. |
| BIOS multiply/divide | Третья программа вычисляет `((200 * 2 mod 256) + 57) / 3 = 67`, сохраняет и повторно загружает quotient, выбирает zero branch после вычитания `67`, выводит `MULDIV` и завершается со status `50`; `div 0` отклоняется. |
| BIOS byte remainder | Программа вычисляет `200 mod 57 = 29`, сохраняет/загружает byte, выбирает exact branch, выводит `MOD` и завершается со status `56`; uninitialized `mod` и `mod 0` отклоняются. |
| BIOS private comparison | Четвёртая программа сохраняет `73` в slot `5`, проверяет equality с `cmp 5` через `jump_if_zero`, затем проверяет inequality после `set 72` через `jump_if_nonzero`; она выводит `EQ` и `NE`, завершается со status `51` и отклоняет uninitialized comparison или slot `8`. |
| Rejection cases | Missing condition, ordinary backward target и exact-conditional backward target возвращают documented syntax diagnostic и status `2`. |
| UEFI persistence | Установленные input/time, argument и variable packages снова запускаются после UEFI/OVMF boot; `A` выбирает ожидаемый path, `[ovmf args]` выводится из forwarded arguments, package `store`/`load` выбирает `VAR`, persisted add/sub arithmetic package выбирает `ARITH`, persisted mul/div package выбирает `MULDIV`, persisted bitwise package выбирает `BITWISE`, persisted XOR package выбирает `XOR`, persisted shift package выбирает `SHIFT`, persisted rotate package выбирает `ROTATE`, persisted remainder package выбирает `MOD`, persisted negation package выбирает `NEG`, persisted increment package выбирает `INC`, persisted decrement package выбирает `DEC`, persisted swap package выбирает `SWAP`, persisted leading-zero package выбирает `CLZ`, persisted parity package выбирает `PARITY`, persisted test package выбирает `TEST`, persisted comparison package выбирает и `EQ`, и `NE`, а каждая time line остаётся корректной. |
| Project starter/direct-run lifecycle | BIOS доказывает exact default-template bytes, duplicate и unknown-template rejection без project creation, затем direct GUI `startgui project <name> build` собирает только fixed source/output pair до подтверждения `build: READY` через `projstatus`; direct GUI `startgui project <name> run [arguments]` запускает только этот generated output, передаёт bounded native tail и отклоняет missing output, пока GUI остаётся active; direct GUI `startgui project <name> install` устанавливает только этот generated output в matching package и подтверждает `package: READY`, одновременно отклоняя missing output в GUI; direct GUI `startgui project <name> uninstall` отклоняет missing package, удаляет только fixed package, сохраняя source/build ready, после чего выполняется bounded direct reinstall; direct GUI `startgui project <name> clean` отклоняет missing generated output, удаляет только этот output, сохраняя source/package ready, и возвращает expected missing-build state; direct GUI `startgui project <name> remove` отклоняет unclean workspace, удаляет только clean source workspace и сохраняет его package. `cleanproj` остаётся covered для shell lifecycle behavior; `buildproj` → `runproj` запускает uninstalled generated template, а starter `args` напрямую передаёт normal native argument string до `installproj`, создающего package. `projlist` сообщает оба starter projects с их fixed three-way status rows, а после `uninstallproj` source/build остаются ready при missing package до восстановления через `installproj`; после `cleanproj` package всё ещё запускается при missing build, затем rebuild снова запускается напрямую перед final clean; `rmproj` удаляет выбранный clean project из `projlist`, пока его installed package остаётся runnable. UEFI повторяет direct GUI build/status/run/install/uninstall/reinstall/clean sequence, подтверждает direct-clean source/package preservation и затем persisted direct-removed project source/build state и preserved package. |
| Automated regression | `make regression` выполняет disposable-image BIOS GUI/editor/native workflow, затем проверяет persistent files и installed native packages при UEFI/OVMF. |

## Связь с SDK и следующий шаг

Host [MyOS SDK](SDK_RU.md) остаётся поддерживаемым путём для более крупных freestanding C11 programs. Native build намеренно меньше: он подтверждает контролируемый in-OS authoring path, а не повторяет host toolchain. Реализованный [Текстовый редактор](TEXT_EDITOR_RU.md) — normal in-OS путь для создания multi-line `.mya` source перед `build`; он также редактирует ordinary text files. Будущая работа над native toolchain останется ограниченной и должна сохранять package separation, loader policy и гарантию forward-only source control flow.
