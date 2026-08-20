# Исследовательские заметки: APIC virtual-wire

<p align="center">
  <strong>🇷🇺 РУССКИЙ</strong> / <a href="research-apic-virtual-wire.md">🇺🇸 ENGLISH</a>
</p>


Дата проверки: 17 августа 2026.

| Наблюдение | Применение в MyOS |
|---|---|
| В QEMU с `-cpu qemu64,-apic` legacy PIC/PIT выдаёт IRQ0, а с обычным включённым APIC счётчик IRQ0 оставался нулевым. | Legacy PIC сам по себе недостаточен на современной APIC-конфигурации. |
| Local APIC имеет LVT-регистры, включая LINT0, а его base обычно находится около физического `0xFEE00000`; base должен браться из `IA32_APIC_BASE` MSR. | MyOS введёт минимальный Local APIC слой и получит base через MSR. |
| В режиме ExtINT LINT0 принимает legacy PIC-сигнал; согласно Intel SDM trigger mode для ExtINT всегда level-sensitive. | LVT LINT0 будет настроен delivery mode `ExtINT` и снята mask-bit. |
| Local APIC должен быть логически включён в Spurious Interrupt Vector Register (bit 8); допустимый spurious vector не должен пересекаться с исключениями. | MyOS использует spurious vector `0xFF`, а IDT расширится общим безопасным обработчиком. |
| PIC остаётся начальным совместимым контроллером, но APIC/IOAPIC является современной архитектурой. | Текущий переходный режим: PIC remap + Local APIC virtual-wire; IOAPIC заменит PIC на следующем более крупном этапе. |

## Источники

[1] https://wiki.osdev.org/APIC — обзор Local APIC, CPUID APIC-бит, SVR и MMIO-base.

[2] https://xem.github.io/minix86/manual/intel-x86-and-64-manual-vol3/o_fe12b1e2a880e0ce-376.html — Intel SDM Vol. 3A, правила LINT0/LINT1 и ExtINT.

[3] https://wiki.osdev.org/8259_PIC — ремаппинг PIC, IRQ-маски и EOI.
