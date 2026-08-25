#include <stdint.h>

#include <arch.h>
#include <framebuffer.h>
#include <pit.h>

#define PIT_CHANNEL0_DATA 0x40U
#define PIT_COMMAND 0x43U
#define PIT_INPUT_HZ 1193182U
#define PIT_COMMAND_CHANNEL0_LOHI_MODE2 0x34U

static volatile uint64_t ticks;
static uint32_t configured_frequency_hz;

void pit_init(uint32_t frequency_hz) {
    uint32_t divisor;

    if (frequency_hz == 0U) {
        frequency_hz = PIT_FREQUENCY_HZ;
    }
    divisor = PIT_INPUT_HZ / frequency_hz;
    if (divisor == 0U) {
        divisor = 1U;
    }
    if (divisor > UINT16_MAX) {
        divisor = UINT16_MAX;
    }

    ticks = 0U;
    configured_frequency_hz = PIT_INPUT_HZ / divisor;

    arch_out8(PIT_COMMAND, PIT_COMMAND_CHANNEL0_LOHI_MODE2);
    arch_out8(PIT_CHANNEL0_DATA, (uint8_t)(divisor & 0xffU));
    arch_out8(PIT_CHANNEL0_DATA, (uint8_t)((divisor >> 8U) & 0xffU));
}

void pit_on_irq(uint8_t irq) {
    (void)irq;
    ticks++;
    if (configured_frequency_hz != 0U && ticks % configured_frequency_hz == 0U) {
        framebuffer_gui_on_timer_tick();
    }
}

uint64_t pit_ticks(void) {
    return ticks;
}

uint32_t pit_frequency_hz(void) {
    return configured_frequency_hz;
}
