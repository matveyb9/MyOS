# MyOS

> **Language:** [English](README.md) | [Русский](README_RU.md)

**MyOS** is an experimental operating system written from scratch for **x86_64** in freestanding C11 and x86_64 NASM. Limine currently supplies the boot environment, while the kernel, memory management, scheduler, ring-3 programs, shell, filesystem and drivers belong to this repository.

## Status

The stable console baseline is [`v0.12.1-console`](docs/RELEASES.md). Current graphical and user-program work is isolated in [`gui/bringup`](https://github.com/matveyb9/MyOS/tree/gui/bringup); it is **experimental** and is not merged into `main` automatically. This branch includes BIOS/UEFI boot paths, MYPFS004 persistent storage, framebuffer GUI, persistent ELF packages, the MyOS SDK, and a bounded in-OS assembler with labels and forward-only jumps.

| Line | Purpose | State |
|---|---|---|
| `console-stable` | Reviewed console baseline. | Stable, tagged `v0.12.1-console`. |
| `main` | Console maintenance and documentation baseline. | Supported console line. |
| `gui/bringup` | GUI and native-development work. | Experimental development line. |

## Quick start

Clone the repository and choose the branch you want to run. The commands below select the current GUI development line.

```bash
git clone https://github.com/matveyb9/MyOS.git myos
cd myos
git switch gui/bringup
make all img
```

Run the persistent raw disk image in QEMU. The `if=ide` attachment is required for the tested AHCI persistence path.

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

MyOS opens the user shell after a three-second countdown. Press `K` during the countdown only when you need the diagnostic `kernel>` shell. In the user shell, start with:

```text
help
ls /
run hello
startgui
```

> `make img` recreates `myos.img` and erases its previous persistent MyOS data. Use a disposable copy before experiments. For a physical USB test, use `myos.img`, not the ISO, and follow the safety guidance in the User Guide.

## Documentation

| Guide | Start here when you need… |
|---|---|
| [Documentation map](docs/README.md) | A short route to every current and historical document. |
| [User Guide](docs/USER_GUIDE.md) | QEMU, shell commands, files, persistence and USB safety. |
| [Platform Guide](docs/PLATFORMS.md) | Linux, Windows/WSL, macOS and host-tool setup. |
| [Developer Guide](docs/DEVELOPER_GUIDE.md) | Architecture, source layout, ABI, storage rules and validation. |
| [Release Guide](docs/RELEASES.md) | Branches, tags, release notes and bilingual commit convention. |
| [Roadmap](docs/ROADMAP.md) | Completed work, current priorities and future directions. |
| [GUI Guide](docs/GUI_BRINGUP.md) | Controls and limits of the experimental framebuffer desktop. |
| [Native Build Guide](docs/NATIVE_BUILD.md) | In-OS `.mya` source, build, install and run workflow. |
| [Documentation Policy](docs/DOCUMENTATION_POLICY.md) | Required same-commit documentation and translation updates. |

## Verification

```bash
make smoke          # BIOS and UEFI boot markers
make regression     # disposable-image GUI and native workflow
make release-check  # clean rebuild, checks and SHA-256 evidence
```

`make release-check` is local verification only. It does not create a tag, GitHub Release or Pre-release.

---

MyOS is an educational experimental project, not a production desktop operating system. Networking, USB HID, SMP, Secure Boot, dynamic linking, a full native C compiler and physical-PC release validation remain outside the current scope. A project license has not yet been selected.
