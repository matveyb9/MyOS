# IRQ and PS/2 validation in MyOS 0.4.0-dev

> **🌐 LANGUAGE / ЯЗЫК:** [🇷🇺 РУССКИЙ](irq-validation_RU.md) / **🇺🇸 ENGLISH**

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), [developer guide](DEVELOPER_GUIDE.md) and [documentation index](README.md).


## What was tested

MyOS 0.4.0-dev uses the legacy PIC as the IRQ source controller, but delivers its signal via the Local APIC LINT0 in **ExtINT virtual-wire** mode. This is required for the normal APIC configuration in QEMU: the PIC/PIT operates with the APIC disabled, but without the Local APIC the route does not reach the CPU. Local APIC MMIO is mapped only in a single uncached page `0xFFFFFFFFC0000000`; this early mapper is not a full virtual memory system.

| Check | BIOS QEMU Q35 | UEFI QEMU Q35 + OVMF | Result |
|---|---:|---:|---|
| Boot ISO | Passed | Passed | Kernel reports `Local APIC virtual wire: enabled`. |
| PIT IRQ0 | Passed | Passed | `ticks` increases at roughly 100 Hz. |
| PIC mask | Passed | Passed | Mask `0xFFFC`: only IRQ0 and IRQ1 enabled. |
| PS/2 scanning | Passed | Passed | Driver received ACK for `0xF4` and unmasked IRQ1. |
| QMP `sendkey` | Passed | Passed | `help`, `keyboard`, `halt` were typed only via the virtual PS/2 keyboard. |
| Keyboard IRQ counter | Passed | Passed | IRQ1 counter increased; no ring buffer overflows. |

> For the test, keys were injected using QEMU HMP `sendkey` via the QMP socket. This verifies the path "virtual PS/2 keyboard → PIC IRQ1 → Local APIC → IDT → IRQ dispatcher → keyboard ring buffer → shell", not COM1-serial input.

## Observed results

In the BIOS scenario the PIT counter rose approximately from `0xA4` to `0x16C` over a two-second interval. In the UEFI scenario it rose from `0xD01` to `0x118D` between two `ticks` queries. The `keyboard` command in UEFI showed `IRQ1` count `0x2A` and `dropped characters: 0x0` after entering the commands `ticks`, `keyboard` and `halt`.

| Subsystem | Implemented behavior | Explicit limitation |
|---|---|---|
| PIC 8259A | Remap `0x20–0x2F`, line masking, master/slave EOI. | No correct handling for spurious IRQ7/IRQ15. |
| Local APIC | LINT0 ExtINT, TPR 0, software-enable via SVR, LAPIC EOI. | No Local APIC timer, IPI, SMP or IOAPIC. |
| PIT | Channel 0, rate generator, about 100 Hz. | PIT is legacy and not a long-term time source. |
| PS/2 | Enable-scanning `0xF4`, ACK/Resend, Set 1 US QWERTY, Shift, Backspace, Enter. | No USB HID, Caps Lock, extended keys, command queue or keyboard layouts. |
| Shell | Polls serial and keyboard buffer; `hlt` with no ready input. | A screen text console will appear only in version 0.6.0-dev. |

## Reproducing the test

Standard quick checks are preserved:

```bash
cd /home/ubuntu/myos
make
make run
make run-uefi
```

For automated hardware input, QEMU must be started with a QMP socket and a serial log. Then QMP obtains `qmp_capabilities`, after which commands of the form `human-monitor-command` with `sendkey h`, `sendkey e`, `sendkey l`, `sendkey p`, `sendkey ret` are issued. After that the serial log should contain the executed command and the increase in the `IRQ1` keyboard count.

## Next milestone

The next priority will be to extend the current point mapper to a controlled 4-level paging: a single `boot_info` structure, reservation of pages for the kernel/modules/MMIO, a kernel heap and safe mapping APIs. After that a text terminal on the framebuffer will appear — a key step to run a command line on a normal PC without COM1.

## References

[1]: https://wiki.osdev.org/8259_PIC "OSDev Wiki — PIC remap, masks and EOI"
[2]: https://wiki.osdev.org/APIC "OSDev Wiki — Local APIC"
[3]: https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-376.html "Intel SDM Vol. 3A — ExtINT on LINT0"
[4]: https://wiki.osdev.org/Programmable_Interval_Timer "OSDev Wiki — PIT"
[5]: https://wiki.osdev.org/PS/2_Keyboard "OSDev Wiki — PS/2 keyboard"
