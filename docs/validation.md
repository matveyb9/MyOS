# Validation of MyOS 0.3.0-dev

<p align="center">
  <a href="validation_RU.md">🇷🇺 РУССКИЙ</a> / <strong>🇺🇸 ENGLISH</strong>
</p>

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), the [developer guide](DEVELOPER_GUIDE.md) and the [documentation index](README.md).


## Scope of validation

Validation was performed for two artifacts: `myos.iso` — a hybrid optical ISO image, and `myos.hdd` — a raw 64 MiB HDD/USB image with a FAT EFI System Partition. Both artifacts are built from the same `build/kernel.elf`; the Limine bootloader supports x86_64 and can operate with both BIOS and UEFI. [1]

| Артефакт | BIOS QEMU | UEFI QEMU / OVMF | Проверенный сценарий | Итог |
|---|---:|---:|---|---|
| `myos.iso` | Passed | Passed | Kernel start, COM1 shell, `help`, `echo`, `meminfo`, `pmm`, `alloc`, `halt`. | Success. |
| `myos.iso` | Passed | Passed | `crash` triggers a divide-error and prints vector `0x0`, code `0x0`, RIP. | Success. |
| `myos.hdd` | Passed | Passed | Kernel start, PMM, serial shell. | Success. |
| `myos.hdd` | Not performed on physical PC | Not performed on physical PC | Physical writing was intentionally not performed. | Awaiting a separate test USB. |

Tests used QEMU Q35 with 256 MiB RAM. For UEFI, OVMF code/vars images from the Ubuntu package were used. Shutdown after `halt` or `crash` is considered expected: the processor disables interrupts and executes `hlt`, and QEMU exits due to the test time limit.

## How to reproduce the QEMU validation

```bash
cd /home/ubuntu/myos
make clean && make
make run
make run-uefi
```

In either of the first two runs wait for `myos>` and run:

```text
help
pmm
alloc
crash
```

After `crash` a diagnostic block `*** KERNEL EXCEPTION ***` should be printed and the kernel will intentionally stop. In a separate run use `halt` for a normal shutdown.

## Prepared USB/HDD image

Creating the image does not write to real devices:

```bash
make hdd
```

The command creates `myos.hdd`, builds a GPT with an EFI System Partition and places `EFI/BOOT/BOOTX64.EFI`, `boot/kernel.elf`, `boot/limine.conf` and the Limine BIOS second-stage file into it. Before a physical run, inspect the contents only in QEMU, then choose **an empty dedicated USB device**.

> Do not write the image to your system disk, a backup disk, or your only working USB. The write operation will overwrite the previous partition table and data on the selected device.

When a separate USB drive is chosen, the command must be run manually and only after carefully verifying its identifier:

```bash
# Example: replace /dev/sdX only after verifying with lsblk.
sudo dd if=myos.hdd of=/dev/sdX bs=4M conv=fsync status=progress
sync
```

MyOS 0.3.0-dev does not support Secure Boot, USB keyboard, networking, SATA/NVMe drivers, or writing to the filesystem. Physical testing at this stage is intended solely to verify that UEFI/BIOS hands control to the kernel and the serial log is available; if there is no COM1 console, a further stage of text framebuffer rendering and PS/2 keyboard is required.

## References

[1]: https://github.com/limine-bootloader/limine "Limine — official bootloader repository"
