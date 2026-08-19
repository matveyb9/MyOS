# MyOS

> **Language:** [English](README.md) | [Русский](README_RU.md)

**MyOS** is an experimental operating system written from scratch for **x86_64** in freestanding C11 and x86_64 NASM. Limine currently supplies the boot environment; the kernel, memory management, scheduler, ring-3 programs, shell, filesystem and drivers belong to this repository.

## Status

This checkout is **`console-stable`**: the reviewed and tagged console baseline at v0.12.1-console. The experimental GUI and native-development work remains isolated in `gui/bringup` and is not part of this console branch.

| Line | Purpose |
|---|---|
| `console-stable` | Reviewed console baseline at `v0.12.1-console`. |
| `main` | Maintained console branch and documentation baseline. |
| `gui/bringup` | Separate experimental GUI and native-development line. |

## Quick start

```bash
git clone https://github.com/matveyb9/MyOS.git myos
cd myos
git switch console-stable
make all img
```

Run the persistent raw disk image in QEMU. The `if=ide` attachment is required for the tested AHCI persistence path.

```bash
qemu-system-x86_64 \
  -machine q35 -m 256M \
  -drive if=ide,format=raw,file=myos.img \
  -boot c
```

MyOS opens the user shell after a three-second countdown. Press `K` only when you need the diagnostic `kernel>` shell. In the user shell, begin with:

```text
help
ls /
run hello
```

> `make img` recreates `myos.img` and erases its previous persistent MyOS data. Use a disposable copy before experiments. For physical USB testing, use `myos.img`, not the ISO, and follow the User Guide safety instructions.

## Documentation

| Guide | Start here when you need… |
|---|---|
| [Documentation map](docs/README.md) | A short route to all current and historical documents. |
| [User Guide](docs/USER_GUIDE.md) | QEMU, shell commands, files, persistence and USB safety. |
| [Platform Guide](docs/PLATFORMS.md) | Linux, Windows/WSL, macOS and host-tool setup. |
| [Developer Guide](docs/DEVELOPER_GUIDE.md) | Architecture, source layout, ABI, storage rules and validation. |
| [Release Guide](docs/RELEASES.md) | Branches, tags, release notes and bilingual commit convention. |
| [Documentation Policy](docs/DOCUMENTATION_POLICY.md) | Required same-commit updates, translations and link review. |

## Verification

```bash
make smoke          # BIOS and UEFI boot markers
make regression     # disposable-image regression available in this source tree
make release-check  # clean rebuild, checks and SHA-256 evidence
```

`make release-check` is local verification only. It does not create a tag, GitHub Release or Pre-release.

---

MyOS is an educational experimental project, not a production desktop operating system. Networking, USB HID, SMP, Secure Boot, dynamic linking, a full native C compiler and physical-PC release validation remain outside this console branch's scope. A project license has not yet been selected.
