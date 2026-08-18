# Документация MyOS Console 0.12.0-dev

Эта папка разделяет **актуальную документацию завершённой консольной версии** и ранние документы, сохранённые как история разработки. Не следует использовать ранние документы как инструкцию по сборке или описание текущих возможностей без сверки с актуальными руководствами.

## Актуальные материалы

| Документ | Для кого | Содержание |
|---|---|---|
| [USER_GUIDE_RU.md](USER_GUIDE_RU.md) | Обычные пользователи | Сборка, запуск в QEMU, первый сеанс shell, работа с файлами, USB-тест и безопасное выключение. |
| [DEVELOPER_GUIDE_RU.md](DEVELOPER_GUIDE_RU.md) | Разработчики | Архитектура console release, каталоги, исходный код, ABI, test workflow и технические ограничения. |
| [../README.md](../README.md) | Все | Краткое описание release, artifacts и Git references. |

## Исторические development notes

Файлы ниже отражают состояние ранних milestones `0.3.0-dev`–`0.7.0-dev`. Они сохранены для истории решений и для последующей учебной редакции, но не описывают полный console release `0.12.0-dev`.

| Файл | Историческая тема | Как использовать сейчас |
|---|---|---|
| `architecture.md` | Ранняя архитектура `0.7.0-dev` | Только как исторический снимок; актуальная архитектура — в developer guide. |
| `validation.md` | Ранняя проверка `0.3.0-dev` и `myos.hdd` | Устарело: современный raw image называется `myos.img` и имеет GPT data partition. |
| `interrupt-model.md`, `irq-validation.md` | Ранняя модель IRQ/PS/2 | Исторический контекст; современный kernel также использует scheduler, ring 3 и user shell. |
| `paging-model.md`, `paging-validation.md` | Ранний paging/heap milestone | Исторический контекст; актуальные memory boundaries перечислены в developer guide. |
| `memory-safety-model.md`, `memory-safety-validation.md` | Ранний memory-safety milestone | Исторический контекст; не заменяет текущие invariants. |
| `framebuffer-console-model.md`, `framebuffer-validation.md`, `framebuffer-visual-check.md` | Ранняя framebuffer text console | Исторический контекст; current console release сохраняет text console и COM1 mirror. |
| `research-apic-virtual-wire.md` | Исследование APIC virtual-wire | Справочный материал для разработчиков. |
| `architecture-decision-32bit.md` | Решение не поддерживать 32-bit | Актуальное архитектурное решение: MyOS остаётся x86_64-only. |

> Для публикации на GitHub в корне repository достаточно начать с `README.md`; ссылки на оба актуальных руководства находятся в нём в первой таблице.
