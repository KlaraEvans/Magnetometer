#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "ds3231.hpp"

// ---------------------------------------------------------------------------
// Hardware-Konfiguration
// Pins und I2C-Port hier anpassen, falls nötig
// ---------------------------------------------------------------------------
#define DS3231_ADDR  0x68   // I2C-Adresse für DS3231 und DS1307 (identisch!)
#define I2C_SDA_PIN  14
#define I2C_SCL_PIN  15
#define I2C_PORT     i2c1

namespace rtc {

// ---------------------------------------------------------------------------
// Interner Zustand – welcher Chip wurde erkannt?
// ---------------------------------------------------------------------------
static RtcType s_rtc_type = RtcType::UNKNOWN;

// ---------------------------------------------------------------------------
// BCD-Hilfsfunktionen
// Der DS3231/DS1307 speichert Zeitwerte im BCD-Format (Binary Coded Decimal)
// ---------------------------------------------------------------------------

// BCD → Dezimal: z. B. 0x25 → 25
static uint8_t bcd_to_dec(uint8_t bcd) {
    return (bcd >> 4) * 10 + (bcd & 0x0F);
}

// Dezimal → BCD: z. B. 25 → 0x25
static uint8_t dec_to_bcd(uint8_t dec) {
    return ((dec / 10) << 4) | (dec % 10);
}

// ---------------------------------------------------------------------------
// Interne Hilfsfunktion: I2C-Bus initialisieren
// Wird von ds3231_init(), ds1307_init() und rtc_init_auto() genutzt
// ---------------------------------------------------------------------------
static void i2c_bus_init(uint32_t baudrate_hz) {
    i2c_init(I2C_PORT, baudrate_hz);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    // Interne Pull-Ups aktivieren (externe 4,7 kΩ empfohlen für lange Leitungen)
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

// ---------------------------------------------------------------------------
// Initialisierung: DS3231
// Taktfrequenz: 400 kHz (Fast Mode)
// ---------------------------------------------------------------------------
void ds3231_init() {
    i2c_bus_init(400 * 1000);
    s_rtc_type = RtcType::DS3231;
}

// ---------------------------------------------------------------------------
// Initialisierung: DS1307
// Taktfrequenz: max. 100 kHz (Standard Mode) – DS1307 unterstützt kein Fast Mode!
// Zusätzlich: CH-Bit (Clock Halt, Bit 7 im Sekundenregister) zurücksetzen,
// da der DS1307 nach dem ersten Einschalten mit gestoppter Uhr ausgeliefert wird.
// ---------------------------------------------------------------------------
void ds1307_init() {
    i2c_bus_init(100 * 1000);

    // Sekundenregister (0x00) lesen, um CH-Bit zu prüfen
    uint8_t reg = 0x00;
    uint8_t sec = 0;

    int ret = i2c_write_blocking_until(I2C_PORT, DS3231_ADDR, &reg, 1, true,
                                       make_timeout_time_ms(10));
    if (ret < 0) return;  // Chip antwortet nicht

    ret = i2c_read_blocking_until(I2C_PORT, DS3231_ADDR, &sec, 1, false,
                                  make_timeout_time_ms(10));
    if (ret < 0) return;

    // CH-Bit (Bit 7) ist gesetzt → Uhr steht still → zurücksetzen
    if (sec & 0x80) {
        uint8_t buf[2] = { 0x00, (uint8_t)(sec & 0x7F) };
        i2c_write_blocking_until(I2C_PORT, DS3231_ADDR, buf, 2, false,
                                 make_timeout_time_ms(10));
    }

    s_rtc_type = RtcType::DS1307;
}

// ---------------------------------------------------------------------------
// Automatische Erkennung und Initialisierung
// Startet mit 100 kHz (kompatibel mit beiden Chips),
// erkennt den Chip und wechselt bei DS3231 auf 400 kHz.
// ---------------------------------------------------------------------------
void rtc_init_auto() {
    // Mit langsamerer Geschwindigkeit starten – sicher für beide Chips
    i2c_bus_init(100 * 1000);

    s_rtc_type = ds3231_detect();

    if (s_rtc_type == RtcType::DS3231) {
        // DS3231 unterstützt 400 kHz → auf Fast Mode umschalten
        i2c_set_baudrate(I2C_PORT, 400 * 1000);
    } else if (s_rtc_type == RtcType::DS1307) {
        // CH-Bit prüfen und ggf. zurücksetzen (Uhr starten)
        ds1307_init();
    }
    // Bei UNKNOWN: Bus bleibt auf 100 kHz initialisiert
}

// ---------------------------------------------------------------------------
// Chip-Erkennung über Control-Register (Adresse 0x0E)
//
// DS3231: Bits 7–5 des Control-Registers sind laut Datenblatt immer 0,
//         Standardwert nach Reset: 0x1C → (0x1C & 0xE0) == 0 → DS3231
//
// DS1307: Adresse 0x0E liegt im freien RAM-Bereich (56 Byte, 0x08–0x3F).
//         RAM-Inhalt ist nach dem Einschalten undefiniert, oft 0xFF.
//         → (0xFF & 0xE0) != 0 → DS1307
//
// Einschränkung: Falls DS1307-RAM an 0x0E zufällig 0x00–0x1F enthält,
//                wird fälschlich DS3231 erkannt. In diesem Fall lieber
//                den Chip-Typ per #define oder Konfigurationsparameter festlegen.
// ---------------------------------------------------------------------------
RtcType ds3231_detect() {
    uint8_t reg = 0x0E;
    uint8_t val = 0xFF;

    int ret = i2c_write_blocking_until(I2C_PORT, DS3231_ADDR, &reg, 1, true,
                                       make_timeout_time_ms(10));
    if (ret < 0) return RtcType::UNKNOWN;  // Kein Chip auf 0x68 gefunden

    ret = i2c_read_blocking_until(I2C_PORT, DS3231_ADDR, &val, 1, false,
                                  make_timeout_time_ms(10));
    if (ret < 0) return RtcType::UNKNOWN;

    // Bits 7–5 müssen beim DS3231 immer 0 sein
    if ((val & 0b11100000) == 0) {
        return RtcType::DS3231;
    }
    return RtcType::DS1307;
}

// Zuletzt erkannten Chip-Typ zurückgeben
RtcType rtc_get_type() {
    return s_rtc_type;
}

// ---------------------------------------------------------------------------
// Uhrzeit lesen
// Liest 7 Byte ab Register 0x00 (Sekunden bis Jahr).
// Registerlayout ist bei DS3231 und DS1307 identisch.
// ---------------------------------------------------------------------------
void ds3231_get_time(DateTime *dt) {
    uint8_t reg = 0x00;
    uint8_t buf[7];

    int ret = i2c_write_blocking_until(I2C_PORT, DS3231_ADDR, &reg, 1, true,
                                       make_timeout_time_ms(10));
    if (ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) return;

    ret = i2c_read_blocking_until(I2C_PORT, DS3231_ADDR, buf, 7, false,
                                  make_timeout_time_ms(10));
    if (ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) return;

    // Masken entfernen Steuer-Bits, die nicht zum Zeitwert gehören:
    dt->seconds = bcd_to_dec(buf[0] & 0x7F);  // Bit 7: CH-Bit (DS1307) / immer 0 (DS3231)
    dt->minutes = bcd_to_dec(buf[1] & 0x7F);  // Bit 7: reserviert
    dt->hours   = bcd_to_dec(buf[2] & 0x3F);  // Bit 6: 12/24h-Modus, Bit 5: AM/PM
    dt->day     = bcd_to_dec(buf[3] & 0x07);  // Wochentag (1–7)
    dt->date    = bcd_to_dec(buf[4] & 0x3F);  // Tag im Monat
    dt->month   = bcd_to_dec(buf[5] & 0x1F);  // Monat (Bit 7: Jahrhundert-Bit beim DS3231)
    dt->year    = bcd_to_dec(buf[6]);          // Jahr 2-stellig (00–99)
}

// ---------------------------------------------------------------------------
// Uhrzeit schreiben
// Schreibt 7 Byte ab Register 0x00.
// Funktioniert für DS3231 und DS1307 gleich.
// ---------------------------------------------------------------------------
void ds3231_set_time(const DateTime *dt) {
    uint8_t buf[8] = {
        0x00,                       // Startadresse: Register 0x00 (Sekunden)
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

// ---------------------------------------------------------------------------
// Temperatur lesen (nur DS3231)
// Register 0x11 (MSB) und 0x12 (LSB):
//   - MSB: vorzeichenbehaftete Ganzzahl (int8_t)
//   - LSB: Bits 7–6 = Nachkommastellen, Auflösung 0,25 °C pro Schritt
// Rückgabe: -273,0f bei Fehler oder wenn kein DS3231 erkannt wurde
// ---------------------------------------------------------------------------
float ds3231_get_temp() {
    // Temperaturregister existiert nur beim DS3231
    if (s_rtc_type == RtcType::DS1307) return -273.0f;

    uint8_t reg = 0x11;
    uint8_t buf[2];

    int ret = i2c_write_blocking_until(I2C_PORT, DS3231_ADDR, &reg, 1, true,
                                       make_timeout_time_ms(10));
    if (ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) return -273.0f;

    ret = i2c_read_blocking_until(I2C_PORT, DS3231_ADDR, buf, 2, false,
                                  make_timeout_time_ms(10));
    if (ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) return -273.0f;

    int8_t  ganzzahl    = (int8_t)buf[0];         // Vorzeichenbehafteter Ganzzahlanteil
    uint8_t nachkomma   = (buf[1] >> 6) & 0x03;  // 2 Bits → 0, 0,25, 0,50, 0,75 °C

    return (float)ganzzahl + nachkomma * 0.25f;
}

// ---------------------------------------------------------------------------
// I2C-Bus-Scan
// Probiert alle 128 möglichen Adressen und gibt true zurück,
// sobald ein Gerät mit ACK antwortet. Nützlich zur Fehlerdiagnose.
// ---------------------------------------------------------------------------
bool ds3231_scan() {
    for (int addr = 0; addr < 128; addr++) {
        uint8_t buf;
        int ret = i2c_read_blocking_until(I2C_PORT, addr, &buf, 1, false,
                                          make_timeout_time_ms(5));
        if (ret >= 0) return true;
    }
    return false;
}

} // namespace rtc
