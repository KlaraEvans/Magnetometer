#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/critical_section.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include <math.h>
#include <algorithm>
#include "st7735/ST7735_TFT.hpp"
#include "hw_config.h"
#include "f_util.h"
#include "ff.h"
#include "rtc/rtc.hpp"
#include <format>
#include <string>
using namespace std;

// ===================== Einstellungen =====================
#define SCLK_TFT 2
#define SDA_TFT  3
#define RS_TFT   4
#define RST_TFT  6
#define CS_TFT   5
#define ST7735_PURPLE 0xf81f
#define ST7735_CYAN   0x07ff

const int    LED_PIN    = 25;
const double ADC_RATE   = 500000.0;            // Abtastrate in Hz
const double MV_PRO_BIT = 3.3 * 1000 / 4096;   // 12 Bit, 3,3 V -> mV
const int    PERIODEN   = 5;                   // Perioden pro Messung
const int    NMAX       = 700;                 // max. Abtastwerte pro Messung
const double PHI0       = 0.0;                 // Vorzeichen-Kalibrierung (rad)
const int    ANZAHL     = 100;                 // Messungen pro Block
const int    TRIM       = 20;                  // je 20 Werte unten/oben verwerfen

// ===================== Globale Daten =====================
ST7735_TFT myTFT;
FATFS fs;
bool is_sd  = false;
bool is_rtc = false;

double N0 = 117.0;                 // Abtastwerte pro Periode (Suche beim Start)
int    N  = 585;                   // Abtastwerte pro Messung
float sin1[NMAX], cos1[NMAX];      // Referenz 1f
float sin2[NMAX], cos2[NMAX];      // Referenz 2f

// ---- Austausch zwischen core1 (misst) und core0 (zeigt/loggt) ----
struct Ergebnis {
    double u_summe = 0;            // Summe der Block-Mittelwerte
    double pc = 0, ps = 0;         // Summen cos/sin der Phase
    int    bloecke = 0;            // Anzahl fertiger Bloecke
    double rate = 0;               // Messungen pro Sekunde
};
Ergebnis geteilt;
critical_section_t sperre;

// ===================== Initialisierung =====================
void Setup() {
    stdio_init_all();

    // Pico-LED einschalten
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    // GPIO23 HIGH: PWM-Modus der Spannungsversorgung (weniger Welligkeit fuer den ADC)
    gpio_init(23);
    gpio_set_dir(23, GPIO_OUT);
    gpio_put(23, 1);

    // ADC, Eingang 1 (GPIO27)
    adc_init();
    adc_gpio_init(27);
    adc_select_input(1);

    // RTC suchen
    rtc::init();
    is_rtc = rtc::is_present();

    // Display ST7735
    myTFT.TFTInitSPIType(8000, spi0);
    myTFT.TFTSetupGPIO(RST_TFT, RS_TFT, CS_TFT, SCLK_TFT, SDA_TFT);
    myTFT.TFTInitScreenSize(0, 0, 128, 160);
    myTFT.TFTInitPCBType(TFT_ST7735S_Black);
    myTFT.TFTsetRotation(TFT_Degrees_90);
    myTFT.TFTfillScreen(ST7735_BLACK);

    // SD-Karte mounten
    is_sd = (f_mount(&fs, "", 1) == FR_OK);
}

// ===================== SD-Karte =====================
void log_data(const string& data, const string& filename) {
    FIL fil;
    UINT bw;
    if (f_open(&fil, filename.c_str(), FA_WRITE | FA_OPEN_APPEND) != FR_OK) return;
    f_write(&fil, data.c_str(), data.size(), &bw);
    f_write(&fil, "\n", 1, &bw);
    f_close(&fil);
}

// ===================== ADC =====================
// Schnelle Messung: n Werte (mV) mit voller Abtastrate
void fast(float* u, int n) {
    adc_set_clkdiv(0);
    adc_fifo_setup(true, false, 1, false, false);
    adc_fifo_drain();
    adc_run(true);
    for (int i = 0; i < n; i++) {
        while (adc_fifo_is_empty());
        u[i] = adc_fifo_get() * (float)MV_PRO_BIT;
    }
    adc_run(false);
}

// ===================== Periode finden (nur beim Start) =====================
void find_period(const float* u, int n) {
    float mean = 0;
    for (int i = 0; i < n; i++) mean += u[i];
    mean /= n;

    // Leistung bei Periode P
    auto power = [&](float P) {
        float a = 0, b = 0, w = 2 * (float)M_PI / P;
        for (int i = 0; i < n; i++) {
            float v = u[i] - mean;
            a += v * cosf(w * i);
            b += v * sinf(w * i);
        }
        return a * a + b * b;
    };

    // Grobsuche P = 110..125, Schritt 0,25
    float bestP = 0, best = 0;
    for (float P = 110; P <= 125; P += 0.25f) {
        float p = power(P);
        if (p > best) { best = p; bestP = P; }
    }
    // Feinsuche um das Maximum, Schritt 0,02
    float c = bestP;
    for (float P = c - 0.25f; P <= c + 0.25f; P += 0.02f) {
        float p = power(P);
        if (p > best) { best = p; bestP = P; }
    }
    // Nur uebernehmen, wenn ein deutliches Signal da ist (Sensor angeschlossen)
    double amp = 2.0 * sqrt((double)best) / n;   // mV
    if (amp > 50.0) N0 = bestP;
}

// Referenztabellen fuer 1f und 2f aus der Periode N0 berechnen
void build_tables() {
    N = (int)lround(PERIODEN * N0);
    if (N > NMAX) N = NMAX;
    for (int i = 0; i < N; i++) {
        double ph = 2.0 * M_PI * i / N0;
        sin1[i] = sin(ph);      cos1[i] = cos(ph);
        sin2[i] = sin(2 * ph);  cos2[i] = cos(2 * ph);
    }
}

// ===================== Messung (laeuft auf core1) =====================
// Eine Messung: 2f mit Vorzeichen (Referenz = 2 * Phase von 1f + PHI0).
// dphi = Phase von 2f relativ zu 2 * Phase von 1f (rad)
double messe_2f(double& dphi) {
    static float u[NMAX];          // static: der Stack von core1 ist klein
    fast(u, N);

    // Gleichanteil bestimmen
    float m = 0;
    for (int i = 0; i < N; i++) m += u[i];
    m /= N;

    // Lock-in: Komponenten bei 1f (A1,B1) und 2f (A2,B2)
    float A1 = 0, B1 = 0, A2 = 0, B2 = 0;
    for (int i = 0; i < N; i++) {
        float v = u[i] - m;
        A1 += v * cos1[i];  B1 += v * sin1[i];
        A2 += v * cos2[i];  B2 += v * sin2[i];
    }

    // Phasen berechnen
    double phi1 = atan2((double)B1, (double)A1);
    double phi2 = atan2((double)B2, (double)A2);
    dphi = phi2 - 2.0 * phi1;

    // 2f auf die Referenzphase projizieren -> Wert mit Vorzeichen
    double ref = 2.0 * phi1 + PHI0;
    return 2.0 * (A2 * cos(ref) + B2 * sin(ref)) / N;
}

// Mittelwert ohne je TRIM kleinste und groesste Werte
double trimmed_mean(double* v, int n) {
    sort(v, v + n);
    double s = 0;
    for (int i = TRIM; i < n - TRIM; i++) s += v[i];
    return s / (n - 2 * TRIM);
}

// Endlosschleife auf core1: Block messen, Ergebnis an core0 uebergeben
void core1_messen() {
    static double werte[ANZAHL];
    while (true) {
        double pc = 0, ps = 0;
        uint64_t t0 = time_us_64();
        for (int i = 0; i < ANZAHL; i++) {
            double dphi;
            werte[i] = messe_2f(dphi);
            pc += cos(dphi);
            ps += sin(dphi);
        }
        double rate  = ANZAHL * 1e6 / (double)(time_us_64() - t0);
        double u_blk = trimmed_mean(werte, ANZAHL);

        // Ergebnis zur Summe der laufenden Sekunde addieren
        critical_section_enter_blocking(&sperre);
        geteilt.u_summe += u_blk;
        geteilt.pc += pc;
        geteilt.ps += ps;
        geteilt.bloecke++;
        geteilt.rate = rate;
        critical_section_exit(&sperre);
    }
}

// core0: Summe der letzten Sekunde abholen und zuruecksetzen
Ergebnis nimm_ergebnis() {
    critical_section_enter_blocking(&sperre);
    Ergebnis e = geteilt;
    geteilt = Ergebnis();
    critical_section_exit(&sperre);
    return e;
}

// ===================== Zeit (RTC, core0) =====================
struct Zeit {
    string text  = "";              // Datum und Uhrzeit
    string datei = "messung.txt";   // Dateiname des Tages
    long   key   = 0;               // Sekundenkennung (erkennt den Wechsel)
    bool   gueltig = true;          // false: Lesefehler
};

Zeit lese_zeit() {
    static uint64_t letzter_versuch = 0;
    Zeit z;
    is_rtc = rtc::is_present();
    if (is_rtc) {
        rtc::DateTime t{};
        rtc::get_time(&t);
        if (t.month < 1 || t.month > 12) { z.gueltig = false; return z; }   // Lesefehler
        z.datei = format("{:02d}{:02d}{:02d}.txt", t.year, t.month, t.date);
        z.text  = format("{:02d}.{:02d}.{:02d} {:02d}:{:02d}:{:02d} ",
                         t.date, t.month, t.year, t.hours, t.minutes, t.seconds);
        z.key   = t.seconds + 60L * t.minutes + 3600L * t.hours + 86400L * t.date;
    } else {
        // Keine RTC: Sekundentakt vom Pico-Timer, alle 5 s neu initialisieren
        uint64_t jetzt = time_us_64();
        if (jetzt - letzter_versuch > 5000000) { rtc::init(); letzter_versuch = jetzt; }
        z.key = (long)(jetzt / 1000000);
    }
    return z;
}

// ===================== Anzeige (core0) =====================
void zeige(double u2, double phase, double rate, const Zeit& z) {
    string s1 = format("{:.2f}  ", u2);
    string s2 = format("{:.1f}   ", phase);
    string s3 = format("Mess/s: {:.0f}   ", rate);
    myTFT.TFTdrawText(40, 40, s1, ST7735_PURPLE, ST7735_BLACK, 2);
    myTFT.TFTdrawText(40, 70, s2, ST7735_CYAN,   ST7735_BLACK, 2);
    myTFT.TFTdrawText(5, 105, s3, ST7735_WHITE,  ST7735_BLACK, 1);
    string zt = is_rtc ? z.text : string("Kein RTC              ");
    myTFT.TFTdrawText(5, 20, zt, ST7735_WHITE, ST7735_BLACK, 1);
}

// ===================== Hauptprogramm (core0) =====================
int main() {
    Setup();

    // Startbildschirm
    myTFT.TFTdrawText(32, 52, "Hallo!", ST7735_BLUE, ST7735_BLACK, 3);
    sleep_ms(1000);
    myTFT.TFTfillScreen(ST7735_BLACK);

    // Beschriftung und Status
    myTFT.TFTdrawText(5, 40, "U2:", ST7735_PURPLE, ST7735_BLACK, 2);
    myTFT.TFTdrawText(5, 70, "Ph:", ST7735_CYAN, ST7735_BLACK, 2);
    myTFT.TFTdrawText(5, 5, is_sd ? "SD hier" : "Kein SD", ST7735_WHITE, ST7735_BLACK, 1);

    // Beim Start (noch nur core0): Periode finden, Tabellen aufbauen
    static float buf[NMAX];
    fast(buf, N);
    find_period(buf, N);
    build_tables();
    string info = format("f={} Hz N0={:.2f}   ", (int)(ADC_RATE / N0), N0);
    myTFT.TFTdrawText(5, 120, info, ST7735_WHITE, ST7735_BLACK, 1);

    // Ab hier misst core1 dauerhaft (ADC gehoert nun core1)
    critical_section_init(&sperre);
    multicore_launch_core1(core1_messen);

    // Zustand der Sekunden-Ausgabe
    long   key_alt = -1;                    // Sekunde der letzten Ausgabe
    bool   bereit = false;                  // erst nach der ersten vollen Sekunde loggen
    string text_alt = "", datei_alt = "messung.txt";
    double u_letzt = 0, phase_letzt = 0, rate_letzt = 0;

    while (1) {
        Zeit z = lese_zeit();

        // Sekundenwechsel: Ergebnis der vergangenen Sekunde ausgeben
        if (z.gueltig && z.key != key_alt) {
            Ergebnis e = nimm_ergebnis();
            if (e.bloecke > 0) {            // sonst alten Wert behalten
                u_letzt     = e.u_summe / e.bloecke;
                phase_letzt = atan2(e.ps, e.pc) * 180.0 / M_PI;
                rate_letzt  = e.rate;
            }

            // Erst anzeigen, dann auf die Karte schreiben (SD kann dauern)
            zeige(u_letzt, phase_letzt, rate_letzt, z);
            if (bereit && is_sd)
                log_data(text_alt + format("{:.2f}", u_letzt), datei_alt);

            // Zeitstempel der Sekunde merken, die jetzt gemessen wird
            key_alt = z.key;
            bereit = true;
            text_alt = z.text;
            datei_alt = z.datei;
        }

        sleep_ms(10);                       // RTC alle 10 ms abfragen
    }
}