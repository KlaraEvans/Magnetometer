#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "st7735/ST7735_TFT.hpp"
#include "rtc/rtc.hpp"

ST7735_TFT myTFT;
#define SCLK_TFT 2
#define SDA_TFT  3
#define RS_TFT   4
#define RST_TFT  6
#define CS_TFT   5

// Eingabe "DD MM YY HH MM SS" auswerten und in die RTC schreiben
bool setze_zeit(const char* text) {
    int d, mo, y, h, mi, s;
    if (sscanf(text, "%d %d %d %d %d %d", &d, &mo, &y, &h, &mi, &s) != 6) return false;
    if (d < 1 || d > 31 || mo < 1 || mo > 12 || y < 0 || y > 99) return false;
    if (h < 0 || h > 23 || mi < 0 || mi > 59 || s < 0 || s > 59) return false;

    rtc::DateTime dt = {
        .seconds = (uint8_t)s,
        .minutes = (uint8_t)mi,
        .hours   = (uint8_t)h,
        .day     = 1,                 // Wochentag wird nicht benutzt
        .date    = (uint8_t)d,
        .month   = (uint8_t)mo,
        .year    = (uint8_t)y
    };
    rtc::set_time(&dt);
    return true;
}

int main() {
    stdio_init_all();
    rtc::init();
    bool is_rtc = rtc::is_present();
    rtc::RtcType chip = rtc::get_type();

    // Display initialisieren
    myTFT.TFTInitSPIType(8000, spi0);
    myTFT.TFTSetupGPIO(RST_TFT, RS_TFT, CS_TFT, SCLK_TFT, SDA_TFT);
    myTFT.TFTInitScreenSize(0, 0, 128, 160);
    myTFT.TFTInitPCBType(TFT_ST7735S_Black);
    myTFT.TFTsetRotation(TFT_Degrees_90);
    myTFT.TFTfillScreen(ST7735_BLACK);

    if (!is_rtc) {
        myTFT.TFTdrawText(10, 55, "No RTC found!", ST7735_RED, ST7735_BLACK, 2);
        while (true) sleep_ms(1000);
    }

    // Chip-Typ anzeigen
    if (chip == rtc::RtcType::DS3231)
        myTFT.TFTdrawText(10, 5, "DS3231", ST7735_GREEN, ST7735_BLACK, 1);
    else if (chip == rtc::RtcType::DS1307)
        myTFT.TFTdrawText(10, 5, "DS1307", ST7735_GREEN, ST7735_BLACK, 1);

    myTFT.TFTdrawText(10, 55, "Warte auf USB", ST7735_WHITE, ST7735_BLACK, 2);

    // Warten, bis der Serial Monitor verbunden ist
    while (!stdio_usb_connected()) sleep_ms(100);
    sleep_ms(500);

    myTFT.TFTfillScreen(ST7735_BLACK);
    myTFT.TFTdrawText(10, 55, "Time not set", ST7735_WHITE, ST7735_BLACK, 2);
    printf("Enter date and time: DD MM YY HH MM SS\n");
    printf("Example: 16 04 26 12 57 20\n> ");
    fflush(stdout);

    char eingabe[32];
    int  i = 0;
    bool time_set = false;

    while (true) {
        int c = getchar_timeout_us(100000);

        if (c != PICO_ERROR_TIMEOUT) {
            printf("%c", c);                 // Echo
            fflush(stdout);

            if (c == '\n' || c == '\r') {
                if (i > 0) {
                    eingabe[i] = '\0';
                    if (setze_zeit(eingabe)) {
                        printf("\nTime set.\n");
                        myTFT.TFTfillScreen(ST7735_BLACK);
                        time_set = true;
                    } else {
                        printf("\nError: use DD MM YY HH MM SS. Try again:\n> ");
                    }
                    i = 0;
                }
            } else if (i < (int)sizeof(eingabe) - 1) {
                eingabe[i++] = (char)c;
            }
        }

        // Uhrzeit nach dem Setzen jede Sekunde anzeigen
        if (time_set) {
            rtc::DateTime now{};
            rtc::get_time(&now);
            char txt[32];
            snprintf(txt, sizeof(txt), "20%02d-%02d-%02d %02d:%02d:%02d",
                     now.year, now.month, now.date,
                     now.hours, now.minutes, now.seconds);
            myTFT.TFTdrawText(5, 50, txt, ST7735_WHITE, ST7735_BLACK, 1);
            sleep_ms(900);
        }
    }
}