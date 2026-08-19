# Validation of MyOS 0.3.0-dev

> [🇷🇺 РУССКИЙ](validation_RU.md) / **🇺🇸 ENGLISH**

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), [developer guide](DEVELOPER_GUIDE.md) and [documentation index](README.md).


## Scope of validation

Validation was performed for two artifacts: `myos.iso` — a hybrid optical ISO image, and `myos.hdd` — a raw 64 MiB HDD/USB image with a FAT EFI System Partition. Both artifacts are built from the same `build/kernel.elf`; the Limine bootloader supports x86_64 and can operate with both BIOS and UEFI. [1]

| Artifact | BIOS QEMU | UEFI QEMU / OVMF | Verified scenario | Result |
|---|---:|---:|---|---|
| `myos.iso` | Passed | Passed | Kernel start, COM1 shell, `help`, `echo`, `meminfo`, `pmm`, `alloc`, `halt`. | Success. |
| `myos.iso` | Passed | Passed | `crash` triggers a divide-error and prints vector `0x0`, code `0x0`, RIP. | Success. |
| `myos.hdd` | Passed | Passed | Kernel start, PMM, serial-shell. | Success. |
| `myos.hdd` | Not performed on a physical PC | Not performed on a physical PC | Physical write intentionally not performed. | Awaiting a separate test USB. |

Tests used QEMU Q35 with 256 MiB RAM. For UEFI, OVMF code/vars images from the Ubuntu package were used. Shutdown after `halt` or `crash` is considered expected: the CPU disables interrupts and executes `hlt`, and QEMU exits due to the test time limit.

## How to reproduce the QEMU test

```bash
cd /home/ubuntu/myos
make clean && make
make run
make run-uefi
```

In either of the first two runs, wait for `myos>` and run:

```text
help
pmm
alloc
crash
```

After `crash` a diagnostic block `*** KERNEL EXCEPTION ***` should be printed, and the kernel will intentionally stop. In a separate run use `halt` for a normal shutdown.

## Prepared USB/HDD image

Creating the image does not write to real devices:

```bash
make hdd
```

The command creates `myos.hdd`, builds a GPT with an EFI System Partition and places `EFI/BOOT/BOOTX64.EFI`, `boot/kernel.elf`, `boot/limine.conf` and the Limine BIOS second-stage file into it. Before physical boot, verify the contents only in QEMU, then select an **empty dedicated USB drive**.

> Do not write the image to your system disk, a backup disk, or your only working USB. The overwrite operation destroys the previous partition table and data on the selected device.

When a separate USB device is chosen, the command must be executed manually and only after carefully verifying its identifier:

```bash
# Example: replace /dev/sdX only after verifying with lsblk yourself.
sudo dd if=myos.hdd of=/dev/sdX bs=4M conv=fsync status=progress
sync
```

MyOS 0.3.0-dev does not support Secure Boot, USB keyboard, networking, SATA/NVMe drivers or filesystem writing. A physical test at this stage is intended solely to verify that UEFI/BIOS hands control to the kernel and that the serial log is available; if there is no COM1 console, further work is required to add framebuffer text rendering and PS/2 keyboard support.

## References

[1]: https://github.com/limine-bootloader/limine "Limine — official bootloader repository"
