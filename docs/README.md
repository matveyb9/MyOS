# MyOS Documentation

> **Language:** [English](README.md) | [Русский](README_RU.md)

This folder contains the detailed documentation for the current branch. Start with the guide that matches your goal; you do not need to read every page.

## Current guides

| Guide | Use it for |
|---|---|
| [User Guide](USER_GUIDE.md) | Running MyOS in QEMU, shell commands, files, persistence and USB safety. |
| [Platform Guide](PLATFORMS.md) | Host setup on Linux, Windows/WSL and macOS. |
| [Developer Guide](DEVELOPER_GUIDE.md) | Source tree, architecture, ABI, storage invariants and validation. |
| [Release Guide](RELEASES.md) | Branch roles, tags, release notes and bilingual commit format. |
| [Roadmap](ROADMAP.md) | Completed milestones and planned work. |
| [Documentation Policy](DOCUMENTATION_POLICY.md) | Required same-commit updates, translations and link review. |

## Feature guides

| Guide | Use it for |
|---|---|
| [GUI Bring-up](GUI_BRINGUP.md) | The experimental framebuffer desktop in `gui/bringup`. |
| [Text Editor](TEXT_EDITOR.md) | Editing ordinary files and multi-line `.mya` source inside MyOS. |
| [Native Build](NATIVE_BUILD.md) | Writing, building, installing and running bounded `.mya` programs. |
| [MyOS SDK](SDK.md) | Building freestanding C11 programs on the host. |
| [Filesystem Specification](FILESYSTEM_SPEC.md) | Root layout, paths and runtime projection. |
| [MYPFS004 Storage](MYPFS004_STORAGE.md) | Persistent-file capacity, extents and migration. |
| [Release Stabilization](RELEASE_STABILIZATION.md) | Automated checks and the remaining physical-PC release gate. |

## Historical notes

The following records are retained for design history. They are **not** current build instructions or specifications: [architecture](architecture.md), [validation](validation.md), [interrupts](interrupt-model.md), [paging](paging-model.md), [memory safety](memory-safety-model.md), [framebuffer console](framebuffer-console-model.md), and [x86_64 decision](architecture-decision-32bit.md).

Return to the project overview: [root README](../README.md).
