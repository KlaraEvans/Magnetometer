#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ds3231.hpp"


#define DS3231_ADDR  0x68
#define I2C_SDA_PIN  14
#define I2C_SCL_PIN  15
#define I2C_PORT     i2c1

namespace rtc {

static uint8_t bcd_to_dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t dec_to_bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

void ds3231_init() {
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

void ds3231_get_time(DateTime *dt) {
    uint8_t reg = 0x00;
    uint8_t buf[7];

    int ret = i2c_write_blocking_until(I2C_PORT, DS3231_ADDR, &reg, 1, true,
                                       make_timeout_time_ms(10));
    if (ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) return;

    ret = i2c_read_blocking_until(I2C_PORT, DS3231_ADDR, buf, 7, false,
                                  make_timeout_time_ms(10));
    if (ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) return;

    dt->seconds = bcd_to_dec(buf[0] & 0x7F);
    dt->minutes = bcd_to_dec(buf[1] & 0x7F);
    dt->hours   = bcd_to_dec(buf[2] & 0x3F);
    dt->day     = bcd_to_dec(buf[3] & 0x07);
    dt->date    = bcd_to_dec(buf[4] & 0x3F);
    dt->month   = bcd_to_dec(buf[5] & 0x1F);
    dt->year    = bcd_to_dec(buf[6]);
}

void ds3231_set_time(const DateTime *dt) {
    uint8_t buf[8] = {
        0x00,
        dec_to_bcd(dt->seconds),
        dec_to_bcd(dt->minutes),
        dec_to_bcd(dt->hours),
        dec_to_bcd(dt->day),
        dec_to_bcd(dt->date),
        dec_to_bcd(dt->month),
        dec_to_bcd(dt->year)
    };
    i2c_write_blocking_until(I2C_PORT, DS3231_ADDR, buf, 8, false,
                             make_timeout_time_ms(10));
}

float ds3231_get_temp() {
    uint8_t reg = 0x11;
    uint8_t buf[2];

    int ret = i2c_write_blocking_until(I2C_PORT, DS3231_ADDR, &reg, 1, true,
                                       make_timeout_time_ms(10));
    if (ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) return -273.0f;

    ret = i2c_read_blocking_until(I2C_PORT, DS3231_ADDR, buf, 2, false,
                                  make_timeout_time_ms(10));
    if (ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) return -273.0f;

    int8_t  integer  = (int8_t)buf[0];
    uint8_t fraction = (buf[1] >> 6) & 0x03;

    return (float)integer + fraction * 0.25f;
}
bool ds3231_scan(){
    for (int addr = 0; addr < 128; addr++) {
        uint8_t buf;
        int ret = i2c_read_blocking_until(i2c1, addr, &buf, 1, false, make_timeout_time_ms(5));
        if (ret >= 0) return true;
    }
    return false;
}
} // namespace rtc
