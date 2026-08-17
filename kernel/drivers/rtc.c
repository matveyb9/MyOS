#include <stdint.h>

#include <arch.h>
#include <rtc.h>

#define CMOS_INDEX_PORT UINT16_C(0x70)
#define CMOS_DATA_PORT UINT16_C(0x71)
#define CMOS_NMI_DISABLE UINT8_C(0x80)
#define CMOS_STATUS_A UINT8_C(0x0A)
#define CMOS_STATUS_B UINT8_C(0x0B)
#define CMOS_SECONDS UINT8_C(0x00)
#define CMOS_MINUTES UINT8_C(0x02)
#define CMOS_HOURS UINT8_C(0x04)
#define CMOS_DAY UINT8_C(0x07)
#define CMOS_MONTH UINT8_C(0x08)
#define CMOS_YEAR UINT8_C(0x09)
#define CMOS_UIP UINT8_C(0x80)
#define CMOS_BINARY_MODE UINT8_C(0x04)
#define CMOS_24_HOUR_MODE UINT8_C(0x02)

struct rtc_snapshot {
    uint8_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

static uint8_t cmos_read(uint8_t register_index) {
    arch_out8(CMOS_INDEX_PORT, (uint8_t)(CMOS_NMI_DISABLE | register_index));
    return arch_in8(CMOS_DATA_PORT);
}

static int update_in_progress(void) {
    return (cmos_read(CMOS_STATUS_A) & CMOS_UIP) != 0U;
}

static void read_snapshot(struct rtc_snapshot *snapshot) {
    snapshot->second = cmos_read(CMOS_SECONDS);
    snapshot->minute = cmos_read(CMOS_MINUTES);
    snapshot->hour = cmos_read(CMOS_HOURS);
    snapshot->day = cmos_read(CMOS_DAY);
    snapshot->month = cmos_read(CMOS_MONTH);
    snapshot->year = cmos_read(CMOS_YEAR);
}

static int snapshots_equal(const struct rtc_snapshot *left, const struct rtc_snapshot *right) {
    return left->year == right->year && left->month == right->month && left->day == right->day
           && left->hour == right->hour && left->minute == right->minute && left->second == right->second;
}

static uint8_t bcd_to_binary(uint8_t value) {
    return (uint8_t)((value & 0x0fU) + ((value >> 4U) * 10U));
}

int rtc_read_time(struct rtc_time *time) {
    struct rtc_snapshot first;
    struct rtc_snapshot second;
    uint8_t status_b;

    if (time == (struct rtc_time *)0) {
        return 0;
    }
    for (uint8_t attempt = 0U; attempt < 8U; attempt++) {
        uint32_t spins = 100000U;

        while (update_in_progress() != 0 && spins != 0U) {
            spins--;
        }
        if (spins == 0U) {
            return 0;
        }
        read_snapshot(&first);
        if (update_in_progress() != 0) {
            continue;
        }
        read_snapshot(&second);
        if (snapshots_equal(&first, &second) == 0) {
            continue;
        }
        status_b = cmos_read(CMOS_STATUS_B);
        if ((status_b & CMOS_BINARY_MODE) == 0U) {
            second.second = bcd_to_binary(second.second);
            second.minute = bcd_to_binary(second.minute);
            second.day = bcd_to_binary(second.day);
            second.month = bcd_to_binary(second.month);
            second.year = bcd_to_binary(second.year);
            second.hour = bcd_to_binary((uint8_t)(second.hour & 0x7fU))
                          | (uint8_t)(second.hour & 0x80U);
        }
        if ((status_b & CMOS_24_HOUR_MODE) == 0U) {
            const uint8_t pm = second.hour & 0x80U;

            second.hour &= 0x7fU;
            if (pm != 0U && second.hour < 12U) {
                second.hour = (uint8_t)(second.hour + 12U);
            }
            if (pm == 0U && second.hour == 12U) {
                second.hour = 0U;
            }
        }
        if (second.month == 0U || second.month > 12U || second.day == 0U || second.day > 31U
            || second.hour > 23U || second.minute > 59U || second.second > 59U) {
            return 0;
        }
        time->year = (uint16_t)(2000U + second.year);
        time->month = second.month;
        time->day = second.day;
        time->hour = second.hour;
        time->minute = second.minute;
        time->second = second.second;
        return 1;
    }
    return 0;
}
