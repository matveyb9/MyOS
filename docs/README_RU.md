# Документация MyOS

> **Язык:** [English](README.md) | [Русский](README_RU.md)

В этой папке находится подробная документация для текущей ветки. Откройте guide, который соответствует вашей задаче: читать все страницы подряд не требуется.

## Актуальные руководства

| Документ | Откройте его, если нужно… |
|---|---|
| [Руководство пользователя](USER_GUIDE_RU.md) | Запустить MyOS в QEMU, использовать shell, files, persistence и безопасно записать USB. |
| [Руководство по платформам](PLATFORMS_RU.md) | Настроить host environment на Linux, Windows/WSL или macOS. |
| [Руководство разработчика](DEVELOPER_GUIDE_RU.md) | Понять source tree, архитектуру, ABI, storage invariants и validation. |
| [Руководство по релизам](RELEASES_RU.md) | Разобраться в branches, tags, release notes и двуязычном формате commits. |
| [Дорожная карта](ROADMAP_RU.md) | Узнать о завершённых milestones и запланированной работе. |
| [Политика документации](DOCUMENTATION_POLICY_RU.md) | Выполнять same-commit updates, переводы и проверку ссылок. |

## Руководства по возможностям

| Документ | Откройте его, если нужно… |
|---|---|
| [GUI bring-up](GUI_BRINGUP_RU.md) | Использовать экспериментальный framebuffer desktop в `gui/bringup`. |
| [Native build](NATIVE_BUILD_RU.md) | Написать, собрать, установить и запустить ограниченную `.mya` программу. |
| [MyOS SDK](SDK_RU.md) | Собрать freestanding C11 program на host-компьютере. |
| [Спецификация файловой системы](FILESYSTEM_SPEC_RU.md) | Понять root layout, paths и runtime projection. |
| [MYPFS004 storage](MYPFS004_STORAGE_RU.md) | Узнать limits persistent files, extents и migration. |
| [Release stabilization](RELEASE_STABILIZATION_RU.md) | Выполнить automated checks и подготовить physical-PC release gate. |

## Исторические заметки

Следующие records сохранены для истории проектирования. Это **не** актуальные инструкции по сборке или спецификации: [architecture](architecture_RU.md), [validation](validation_RU.md), [interrupts](interrupt-model_RU.md), [paging](paging-model_RU.md), [memory safety](memory-safety-model_RU.md), [framebuffer console](framebuffer-console-model_RU.md) и [решение x86_64](architecture-decision-32bit_RU.md).

Вернуться к обзору проекта: [корневой README](../README_RU.md).
