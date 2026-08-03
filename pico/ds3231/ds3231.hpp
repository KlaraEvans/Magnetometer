#pragma once
#include "pico/stdlib.h"
#include "hardware/i2c.h"

namespace rtc {

// ---------------------------------------------------------------------------
// Datenstruktur für Datum und Uhrzeit
// ---------------------------------------------------------------------------
struct DateTime {
    uint8_t seconds;  // Sekunden       (0–59)
    uint8_t minutes;  // Minuten        (0–59)
    uint8_t hours;    // Stunden        (0–23)
    uint8_t day;      // Wochentag      (1–7, 1 = Montag)
    uint8_t date;     // Tag im Monat   (1–31)
    uint8_t month;    // Monat          (1–12)
    uint8_t year;     // Jahr (2-stellig, z. B. 25 für 2025)
};

// ---------------------------------------------------------------------------
// Erkannter RTC-Chip-Typ
// ---------------------------------------------------------------------------
enum class RtcType {
    UNKNOWN,  // Kein bekannter Chip gefunden
    DS3231,   // DS3231 – temperaturkompensierter Quarz (TCXO), ±2 ppm
    DS1307    // DS1307 – externer Quarz, ±20 ppm, kein Temperatursensor
};

// ---------------------------------------------------------------------------
// Initialisierung
// ---------------------------------------------------------------------------

// Initialisiert den I2C-Bus und den DS3231 (400 kHz)
void ds3231_init();

// Initialisiert den I2C-Bus und den DS1307 (100 kHz, setzt CH-Bit zurück)
void ds1307_init();

// Erkennt den angeschlossenen Chip automatisch und initialisiert ihn
// entsprechend. Empfohlene Methode, wenn der Chip-Typ unbekannt ist.
void rtc_init_auto();

// ---------------------------------------------------------------------------
// Chip-Erkennung und -Typ
// ---------------------------------------------------------------------------

// Erkennt den angeschlossenen RTC-Chip anhand des Control-Registers (0x0E).
// DS3231: Bits 7–5 des Control-Registers sind immer 0 → DS3231.
// DS1307: Adresse 0x0E zeigt in den RAM → Wert meist 0xFF → DS1307.
// Hinweis: Nicht 100% zuverlässig, wenn DS1307-RAM den Wert 0x00 enthält.
RtcType ds3231_detect();

// Gibt den zuletzt erkannten Chip-Typ zurück (nach rtc_init_auto)
RtcType rtc_get_type();

// ---------------------------------------------------------------------------
// Uhrzeit lesen und schreiben (funktioniert für DS3231 und DS1307)
// ---------------------------------------------------------------------------

// Liest die aktuelle Uhrzeit und das Datum vom RTC in die DateTime-Struktur
void ds3231_get_time(DateTime *dt);

// Schreibt Uhrzeit und Datum in den RTC
void ds3231_set_time(const DateTime *dt);

// ---------------------------------------------------------------------------
// Hilfsfunktionen
// ---------------------------------------------------------------------------

// Scannt den gesamten I2C-Bus und gibt true zurück, wenn ein Gerät antwortet.
// Nützlich zur Diagnose von Verbindungsproblemen.
bool ds3231_scan();

// Liest die interne Chip-Temperatur des DS3231 (Auflösung: 0,25 °C).
// Gibt -273,0f zurück, wenn kein DS3231 vorhanden ist oder ein Fehler auftritt.
float ds3231_get_temp();

} // namespace rtc
