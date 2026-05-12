#include <stdio.h>
#include "pico/stdlib.h"
#include "ds3231/ds3231.hpp"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include "st7735/ST7735_TFT.hpp" 
ST7735_TFT myTFT;
#define SCLK_TFT 2 
#define SDA_TFT  3
#define RS_TFT   4 
#define RST_TFT  6
#define CS_TFT   5
#define ST7735_TEAL 0x03ef
#define ST7735_GREENYELLOW 0xb7e0

int main() {
    stdio_init_all();
    rtc::ds3231_init();
    
    myTFT.TFTInitSPIType(8000, spi0);
    myTFT.TFTSetupGPIO(RST_TFT, RS_TFT, CS_TFT, SCLK_TFT, SDA_TFT);
    myTFT.TFTInitScreenSize(0, 0, 128, 160);
    myTFT.TFTInitPCBType(TFT_ST7735S_Black);
    myTFT.TFTsetRotation(TFT_Degrees_90);
    myTFT.TFTfillScreen(ST7735_BLACK);
     myTFT.TFTdrawText(10, 55,"Time not set", ST7735_WHITE, ST7735_BLACK, 2);
     sleep_ms(5000);
    printf("Enter date and time: DD MM YY HH MM SS\n");
    printf("Example: 16 04 26 12 57 20\n> ");
    fflush(stdout);

    char buf[32];
    int i = 0;
    int date, month, year, hours, minutes, seconds;
    bool time_set = false;
    while (true) {
        int c = getchar_timeout_us(100000);

        if (c != PICO_ERROR_TIMEOUT) {
            printf("%c", c);
            fflush(stdout);

            if (c == '\n' || c == '\r') {
                buf[i] = '\0';
                if (i > 0) {
                    int parsed = sscanf(buf, "%d %d %d %d %d %d",
                                        &date, &month, &year,
                                        &hours, &minutes, &seconds);
                    if (parsed == 6) {
                       rtc::DateTime dt = {
                            .seconds = (uint8_t)seconds,
                            .minutes = (uint8_t)minutes,
                            .hours   = (uint8_t)hours,
                            .day     = 1,
                            .date    = (uint8_t)date,
                            .month   = (uint8_t)month,
                            .year    = (uint8_t)year
                        };
                       rtc::ds3231_set_time(&dt);
                        printf("\nTime set: 20%02d-%02d-%02d %02d:%02d:%02d\n",
                            year, month, date, hours, minutes, seconds);
                        time_set = true;
                    } else {
                        printf("\nError: expected 6 values, got %d. Try again:\n> ", parsed);
                    }
                    i = 0;
                }
            } else if (i < 31) {
                buf[i++] = (char)c;
            }
        } else {
            //timeout: buffer not empty
            if (i > 0) {
                buf[i] = '\0';
                int parsed = sscanf(buf, "%d %d %d %d %d %d",
                                    &date, &month, &year,
                                    &hours, &minutes, &seconds);
                if (parsed == 6) {
                    rtc::DateTime dt = {
                        .seconds = (uint8_t)seconds,
                        .minutes = (uint8_t)minutes,
                        .hours   = (uint8_t)hours,
                        .day     = 1,
                        .date    = (uint8_t)date,
                        .month   = (uint8_t)month,
                        .year    = (uint8_t)year
                    };
                    rtc::ds3231_set_time(&dt);
                    printf("\nTime set: 20%02d-%02d-%02d %02d:%02d:%02d\n",
                        year, month, date, hours, minutes, seconds);
                    myTFT.TFTfillScreen(ST7735_BLACK);
                    myTFT.TFTdrawText(30, 5, "Time set!", ST7735_GREEN, ST7735_BLACK, 2);
                    time_set = true;
                    i = 0;
                }
            }
        }
        if (time_set) {
            rtc::DateTime now;
            rtc::ds3231_get_time(&now);
            char buf[20];
            sprintf(buf, "20%02d-%02d-%02d    %02d:%02d:%02d",
                now.year, now.month, now.date,
                now.hours, now.minutes, now.seconds);
            myTFT.TFTdrawText(20, 50, buf, ST7735_WHITE, ST7735_BLACK, 2);
        
            sleep_ms(1000);
        }
    }
}