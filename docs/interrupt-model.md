# MyOS 0.4.0-dev Interrupt Model

> **Language:** [English](interrupt-model.md) | [Русский](interrupt-model_RU.md)

> **Historical document.** This file describes an early development milestone and is not a specification of the current console release `0.12.0-dev`. Refer to the [user guide](USER_GUIDE.md), [developer guide](DEVELOPER_GUIDE.md) and [documentation index](README.md).


## Goal of the milestone

MyOS already handles CPU exceptions on vectors `0x00–0x1F`. This milestone adds a managed path for external IRQs: first the legacy PIC 8259A as a compatible starting platform for QEMU and most BIOS/UEFI configurations, then PIT channel 0 as a source of periodic ticks and the PS/2 keyboard on IRQ1. On modern PCs the APIC/IOAPIC replace the PIC, so the PIC is considered a transitional learning controller; an APIC layer will be added once the kernel stabilizes. [1]

| Component | Decision | Rationale |
|---|---|---|
| IRQ range | `0x20–0x2F`: IRQ0–IRQ15. | Vectors above exceptions `0x00–0x1F`; the standard PIC remap uses master `0x20`, slave `0x28`. [1] |
| PIC 8259A | After remapping all lines are masked; only IRQ0 and IRQ1 are enabled. | Unexpected devices cannot reach an unready driver. |
| IRQ acknowledgment | EOI is issued after handling: master for IRQ0–7, slave then master for IRQ8–15. | The PIC holds an IRQ in service until EOI. [1] |
| PIT | Channel 0, rate-generator mode, 100 Hz. | Channel 0 is wired to IRQ0; 100 Hz is sufficient for diagnostics and the future scheduler. [2] |
| Keyboard | IRQ1, port `0x60`, scan code set 1 in QEMU/legacy mode; US QWERTY only for the first stage. | IRQ1 is traditionally associated with the keyboard; a fuller PS/2 driver will later manage the controller and layouts. [1] [3] |
| Shell input | A single path receives characters either from COM1 or from the keyboard ring buffer; waiting uses `hlt` with IRQs enabled. | The command line becomes independent of the serial port. |

## Initialization sequence

1. MyOS leaves the GDT and exception IDT active, then adds 16 interrupt gates for the IRQs.
2. The PIC is remapped, all IRQs are masked; the PIT and keyboard handler are initialized before unmasking.
3. Only IRQ0 and IRQ1 are enabled. Then the IF flag is set with the `sti` instruction.
4. Each interrupt stub saves registers, calls `irq_dispatch(irq)`, acknowledges the PIC and returns with `iretq`.
5. The shell calls `hlt` only when both input sources are empty; an external IRQ is guaranteed to wake the CPU.

> IRQ handlers do not perform formatted output to COM1 and do not call the shell. They only increment counters or place a character into the ring buffer. This reduces handler latency and avoids mixing interrupt context with interactive code.

## Test strategy

PIT verification is non-interactive: the shell command `ticks` will show an increasing count of handled IRQ0s. For the keyboard QEMU will be run with the QMP/HMP monitor and virtual PS/2 keyboard events will be sent with `sendkey`. This test will prove the hardware IRQ1 path specifically, not the serial input. BIOS and UEFI are tested separately with the same kernel.

## Limitations of this milestone

MyOS 0.4.0-dev will not support USB HID, layouts other than US QWERTY, keys with multi-byte scan-code sequences, the PS/2 command queue, SMP, Local APIC or IOAPIC. These limitations are not defects of the current prototype: they are deferred to subsequent compatible subsystems.

## References

[1]: https://wiki.osdev.org/8259_PIC "OSDev Wiki — 8259 PIC"
[2]: https://wiki.osdev.org/Programmable_Interval_Timer "OSDev Wiki — Programmable Interval Timer"
[3]: https://wiki.osdev.org/PS/2_Keyboard "OSDev Wiki — PS/2 Keyboard"
