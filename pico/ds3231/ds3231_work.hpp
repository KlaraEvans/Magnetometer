#pragma once
#include "pico/stdlib.h"
#include "hardware/i2c.h"

namespace rtc {

struct DateTime {
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t date;
    uint8_t month;
    uint8_t year;
};

void ds3231_init();
void ds3231_set_time(const DateTime *dt);
void ds3231_get_time(DateTime *dt);
bool ds3231_scan();
float ds3231_get_temp();

} // namespace rtc
