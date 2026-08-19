# Research Notes: APIC virtual-wire

> [🇷🇺 РУССКИЙ](research-apic-virtual-wire_RU.md) / **🇺🇸 ENGLISH**


Verification date: 17 August 2026.

| Observation | Application in MyOS |
|---|---|
| In QEMU with `-cpu qemu64,-apic` the legacy PIC/PIT issues IRQ0, while with APIC enabled normally the IRQ0 counter remained zero. | The legacy PIC by itself is insufficient on a modern APIC configuration. |
| The Local APIC has LVT registers, including LINT0, and its base is usually at physical `0xFEE00000`; the base must be taken from the `IA32_APIC_BASE` MSR. | MyOS will introduce a minimal Local APIC layer and obtain the base via the MSR. |
| In ExtINT mode LINT0 accepts the legacy PIC signal; according to the Intel SDM the trigger mode for ExtINT is always level-sensitive. | The LVT LINT0 will be configured with delivery mode `ExtINT` and the mask bit cleared. |
| The Local APIC must be logically enabled in the Spurious Interrupt Vector Register (bit 8); the chosen spurious vector must not overlap with exceptions. | MyOS uses spurious vector `0xFF`, and the IDT will be extended with a common safe handler. |
| The PIC remains the initial compatible controller, but APIC/IOAPIC is the modern architecture. | Current transitional mode: PIC remap + Local APIC virtual-wire; IOAPIC will replace the PIC in the next larger phase. |

## Sources

[1] https://wiki.osdev.org/APIC — overview of the Local APIC, CPUID APIC bit, SVR and MMIO base.

[2] https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-376.html — Intel SDM Vol. 3A, rules for LINT0/LINT1 and ExtINT.

[3] https://wiki.osdev.org/8259_PIC — PIC remapping, IRQ masks and EOI.
