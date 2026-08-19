# Архитектурное решение: 32-битная поддержка MyOS

> **Язык:** [English](architecture-decision-32bit.md) | [Русский](architecture-decision-32bit_RU.md)


## Контекст

MyOS уже является x86_64-ядром: Limine передаёт управление в long mode, ядро использует System V x86_64 ABI, 64-битные GDT/IDT stubs, четырёхуровневые таблицы страниц, CR3 и higher-half virtual addresses. Полноценная i686 версия не является флагом компиляции: это отдельный архитектурный port с собственными boot/entry path, ABI, protected-mode paging/PAE, interrupt frame и тестовой матрицей.

## Внешние сигналы 2025–2026

| Наблюдение | Значение для MyOS |
|---|---|
| Современный массовый desktop software ориентирован на 64-bit CPUs и UEFI. | Базовой целевой платформой нового ПК-ядра должен оставаться x86_64. [1] |
| Debian продолжает выпускать i386 как **partial** port и описывает его как поддержку IA-32 processors. | 32-bit не исчез полностью: он оправдан для legacy hardware, совместимости и обучения, но не определяет основной путь MyOS. [2] |
| Современный Linux/x86 сохраняет документацию для 32-bit boot protocol. | Поддержка совместимости существует, однако наличие её в зрелом Linux не уменьшает стоимость отдельного port для маленького ядра. [3] |

## Решение

**Не добавлять 32-битную поддержку сейчас.** Основной ствол MyOS остаётся x86_64. Это совпадает с уже реализованными MMU, framebuffer, UEFI/BIOS образами и будущими планами user space и GUI.

| Вариант | Польза | Цена сейчас | Решение |
|---|---|---|---|
| x86_64-only mainline | Современные ПК, UEFI, больше памяти, один ABI и одна MMU-модель. | Не загрузится на i686-only hardware. | Выбран. |
| Полный i686 kernel port сейчас | Старые ПК, практическая работа с protected mode, наглядное сравнение архитектур. | Практически дублирует low-level дерево: boot, paging, GDT/IDT, syscall ABI, драйверные границы, CI/QEMU. Замедлит завершение первой пригодной версии. | Отложить. |
| Учебный i386 bootstrap later | Даёт понимание real/protected mode и совместим с обучающей целью. | Не должен сдерживать основной x86_64 roadmap. | Возможен после стабильного release как отдельный lab/branch. |
| 32-bit user compatibility на x86_64 | Полезно только при появлении user processes и конкретной цели запуска 32-bit программ. | Требует compat ABI/syscalls, ELF loader policy и испытаний. | Не рассматривать до ring 3 и ELF. |

## Триггеры для пересмотра

К решению следует вернуться только при появлении конкретной причины: целевое i686-only оборудование; требование запустить MyOS на старом ПК; учебный модуль по 32-bit protected mode; либо потребность запускать 32-bit пользовательские программы. До этого полезнее не создавать второй kernel port, а поддерживать в документации краткое сравнение x86 protected mode и x86_64 long mode.

## References

[1]: https://www.microsoft.com/en-us/windows/windows-11-specifications "Windows 11 Specifications"
[2]: https://www.debian.org/ports/ "Debian — Ports"
[3]: https://docs.kernel.org/arch/x86/index.html "Linux kernel documentation — x86-specific documentation"
