# Interrupt model of MyOS 0.4.0-dev

> **Language:** [English](interrupt-model.md) | [Русский](interrupt-model_RU.md)

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. See the [user guide](USER_GUIDE.md), the [developer guide](DEVELOPER_GUIDE.md) and the [documentation index](README.md).


## Goal of this stage

MyOS already handles CPU exceptions on vectors `0x00–0x1F`. This stage adds a controlled path for external IRQs: first the legacy PIC 8259A as a compatible starting platform for QEMU and most BIOS/UEFI configurations, then the PIT channel 0 as a periodic tick source and the PS/2 keyboard on IRQ1. On modern PCs the APIC/IOAPIC replace the PIC, so the PIC is considered a transitional learning controller; an APIC layer will appear after the kernel stabilizes. [1]

| Компонент | Решение | Обоснование |
|---|---|---|
| Диапазон IRQ | `0x20–0x2F`: IRQ0–IRQ15. | Vectors above the exceptions `0x00–0x1F`; the standard PIC remap uses master `0x20`, slave `0x28`. [1] |
| PIC 8259A | После ремаппинга все линии маскируются; разрешаются только IRQ0 и IRQ1. | Unexpected devices cannot reach an unready driver. |
| Подтверждение IRQ | EOI выдаётся после обработки: master для IRQ0–7, slave затем master для IRQ8–15. | PIC holds IRQs in service until EOI. [1] |
| PIT | Channel 0, режим rate generator, 100 Гц. | Channel 0 is connected to IRQ0; 100 Hz is sufficient for diagnostics and the future scheduler. [2] |
| Клавиатура | IRQ1, port `0x60`, scan code set 1 в QEMU/legacy-режиме; US QWERTY только для первого этапа. | IRQ1 is traditionally associated with the keyboard; a fuller PS/2 driver will later manage the controller and layouts. [1] [3] |
| Ввод shell | Единый путь получает символы либо из COM1, либо из кольцевого keyboard-буфера; ожидание — `hlt` при включённых IRQ. | The command line becomes independent of the serial port. |

## Initialization order

1. MyOS leaves the GDT and the IDT of exceptions active, then adds 16 interrupt gates for IRQs.
2. The PIC is remapped and all IRQs are masked; the PIT and the keyboard handler are initialized before unmasking.
3. Only IRQ0 and IRQ1 are enabled. Then the IF flag is set with the `sti` instruction.
4. Each interrupt stub saves registers, calls `irq_dispatch(irq)`, sends EOI to the PIC and returns with `iretq`.
5. The shell issues `hlt` only when both input sources are empty; an external IRQ is guaranteed to wake the processor.

> IRQ handlers do not perform formatted output to COM1 and do not invoke the shell. They only increment counters or place a character into the ring buffer. This reduces handling time and prevents mixing interrupt context with interactive code.

## Testing strategy

Verification of the PIT is not human-dependent: the shell command `ticks` will show a growing count of handled IRQ0. For the keyboard, QEMU will be run with the QMP/HMP monitor and virtual PS/2 keyboard events will be sent via `sendkey`. This test will prove the hardware IRQ1 path, not serial input. BIOS and UEFI are tested separately with the same kernel.

## Limitations of this milestone

MyOS 0.4.0-dev will not support USB HID, layouts other than US QWERTY, keys with multi-byte scan-code sequences, a PS/2 command queue, SMP, Local APIC or IOAPIC. These limitations are not bugs in the current prototype: they are deferred to subsequent compatible subsystems.

## References

[1]: https://wiki.osdev.org/8259_PIC "OSDev Wiki — 8259 PIC"
[2]: https://wiki.osdev.org/Programmable_Interval_Timer "OSDev Wiki — Programmable Interval Timer"
[3]: https://wiki.osdev.org/PS/2_Keyboard "OSDev Wiki — PS/2 Keyboard"
