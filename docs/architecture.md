# Architecture of MyOS 0.7.0-dev

> [🇷🇺 РУССКИЙ](architecture_RU.md) / **🇺🇸 ENGLISH**


> **Historical document.** It describes the milestone `0.7.0-dev`, not the current console release `0.12.0-dev`. For the up-to-date architecture use the [developer guide](DEVELOPER_GUIDE.md).

## Purpose

MyOS is an educational, hands-on in-house kernel for **x86_64**. The hybrid ISO image boots under both BIOS and UEFI in QEMU. The project first develops an observable ring-0 kernel, then will provide isolated processes, a filesystem and a graphical environment. Version 0.7.0-dev demonstrates interactive text not only via COM1 but also on the framebuffer for the first time.

> Limine prepares the boot environment and publishes boot responses, however the console, memory, IDT, drivers and future user subsystems belong to MyOS. [1]

## System layers

| Layer | Directory | Responsibility in 0.7.0-dev | Next boundary |
|---|---|---|---|
| Boot | `boot/` | Higher-half ELF, Limine requests, BIOS/UEFI menu. | `boot_info` and explicit ownership of boot regions. |
| Architecture | `kernel/arch/x86_64/` | GDT, IDT, CR2, Local APIC virtual-wire, ASM stubs. | Guard policy, APIC timer, IOAPIC and SMP. |
| IRQ and input | `kernel/irq/`, `kernel/drivers/` | PIC 8259A, PIT 100 Hz, PS/2 Set 1 and keyboard ring buffer. | USB HID, extended keyboard layout, device queues. |
| Physical memory | `kernel/mm/pmm.c` | Bitmap of usable frames and checked allocate/reserve/free. | Ownership tags, >4 GiB and NUMA. |
| Virtual memory | `kernel/mm/paging.c` | Own PML4, CR3 switch, 4 KiB mapping, APIC MMIO. | `unmap`, NX, supervisor/user mappings. |
| Heap | `kernel/mm/heap.c` | Free list, split/coalesce, `kmalloc` and `kfree`. | Locks, guard zones and page reclamation. |
| Serial console | `kernel/console/serial.c` | COM1 output/input, headless journal. | Panic-safe minimal fallback. |
| Framebuffer console | `kernel/console/framebuffer.c` | 8×8 raster, cell buffer, cursor, scroll and COM1 mirroring. | Unicode, optimized scroll, double buffering and UI primitives. |
| Shell | `kernel/console/shell.c` | A single input path for COM1 and PS/2; observability commands. | History, completion, user processes. |

## Output model

Serial remains the first point of the boot log. After accepting a 32-bit RGB Limine framebuffer, `framebuffer_console_init()` creates a character grid, clears the background and enables a second sink. Each subsequent `serial_write_char()` sends the byte to COM1, then to `framebuffer_console_putc()`. This deliberately keeps headless logging even if graphical mode is unavailable.

| Input/character | Serial | Framebuffer |
|---|---|---|
| Printable ASCII | COM1 transmit. | Glyph in the current cell. |
| `\n` | Adds `\r` before LF. | Line break and scroll when at the bottom boundary. |
| Backspace | Prints `\b`, space, `\b`. | Clears the previous cell. |
| ANSI clear | Forwarded to the terminal. | Escape sequence is not rendered; the shell performs an explicit clear. |
| Unsupported UTF-8 byte | Passed through as raw COM1 output. | Ignored until Unicode is supported. |

## Framebuffer implementation

MyOS accepts only Limine RGB framebuffers with `bpp == 32`, whose memory model and pitch are verified to be sufficient for the width. A pixel is addressed via `pixels[y * pixels_per_row + x]`, where `pixels_per_row = pitch / 4`. This accounts for possible padding between framebuffer rows. [2]

| Property | Current policy |
|---|---|
| Colors | Dark navy background, off-white text, cyan cursor/accent. |
| Font | Built-in 5×7 bitmap, placed inside an 8×8 cell. |
| Size | Up to 160 columns × 100 rows; QEMU 1280×800 uses a maximum of 160×100. |
| Scroll | Shift the cell buffer by one row and complete repaint. |
| Cursor | Accent underline in the current cell. |
| Clear | Full clear of cells and pixels; top accent line is preserved. |

## Memory and fault policy

PMM maintains usable and free bitmaps, the heap reuses blocks, and vector 14 preserves CR2 until output and performs a fail-stop. These guarantees remain necessary after adding the framebuffer: rendering does not require dynamic allocation and does not depend on a filesystem, so it is already available in the early bootstrap after paging.

## Visual verification

Both BIOS and UEFI QEMU were verified via QMP `screendump` after typing `fbdemo`/`fbinfo` with the virtual PS/2 keyboard. Screenshots show readable glyphs, the latest demo rows, the cursor, and a nonzero scroll counter. Serial logs independently reproduce the same shell commands and values.

## Next technical iteration

Next MyOS should explicitly reserve kernel and boot physical pages, add `unmap` and guard ranges for virtual memory. After strengthening address ownership the framebuffer-console can get a small line editor, history and a high-contrast status bar, without changing the base serial fallback.

## References

[1]: https://github.com/limine-bootloader/limine-protocol "Limine Boot Protocol"
[2]: https://wiki.osdev.org/Drawing_In_a_Linear_Framebuffer "OSDev Wiki — Drawing in a Linear Framebuffer"
