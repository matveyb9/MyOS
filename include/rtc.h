#ifndef MYOS_RTC_H
#define MYOS_RTC_H

#include <stdint.h>

struct rtc_time {
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

int rtc_read_time(struct rtc_time *time);

#endif
