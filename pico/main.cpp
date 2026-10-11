// ---------------------------------------------------------------------------
// Programm erweitert mit Claude Code 

// Das Programm soll den Lock-in-Verstärker digitalisieren und die Amplitude 
// der zweiten Harmonischen bestimmen mit den Vorzeichen abhängig von der 
// Phasendifferenz zwischen der 1. und 2. Harmonischen. Die Ausgabe am 
// Bildschirm erfolgt einmal pro 250 ms, Ausgabe in die Datei einmal pro 1 s.
// Am Bildschirm wird außerdem die Phasendifferenz ausgegeben.
// ---------------------------------------------------------------------------



#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include <math.h>
#include "pico/time.h"
#include "st7735/ST7735_TFT.hpp"
#include <algorithm>
#include "hw_config.h"
#include "f_util.h"
#include "ff.h"
#include "rtc/rtc.hpp"
#include <format>
#include <string>
using namespace std;

ST7735_TFT myTFT;
#define SCLK_TFT 2 
#define SDA_TFT  3
#define RS_TFT   4 
#define RST_TFT  6
#define CS_TFT   5
#define ST7735_PURPLE 0xf81f
#define ST7735_CYAN 0x07ff   


const double conversion_factor = 3.3*1000 / (1 << 12); // Umrechnung von ADC-Wert zu mV
const int LED_PIN = 25;

// Messparameter: N0 wird am Anfang bestimmt
const int NMAX = 700;
double N0 = 117.0;                 // Messwerte pro Periode
double f  = 500000.0 / N0;         // Frequenz: ADC hat 500000 Messungen pro Sekunde
int N     = 585;                   // N = 5*N0
// Sin und Cos Tabellen
float sin2[NMAX], cos2[NMAX]; //Referenztabellen für die 2. Harmonische
float sin1[NMAX], cos1[NMAX]; //Referenztabellen für die 1. Harmonische

//Vorzeichen-Bestimmung
double PHI0 = 0.0;                 // rad
double last_phi_diff = 0.0;        // rad
double phi_sum_c = 0, phi_sum_s = 0;   // für zirkuläres Mittel

const int ANZAHL = 100;            // Messungen pro Block
const int TRIM   = 20;             // je 20 Werte unten und oben verwerfen
const int MITTEL = 3;              // Anzahl Blöcke für die Anzeige-Mittelung

bool is_sd = true;
bool is_rtc = true;
FATFS fs;


void Setup(void) {	
    stdio_init_all();

    // Pico-LED einschalten
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    // GPIO23 auf HIGH: PWM-Modus der Spannungsversorgung (weniger Welligkeit für den ADC)
    gpio_init(23);
    gpio_set_dir(23, GPIO_OUT);
    gpio_put(23, 1);

    // ADC
    adc_init();
    adc_gpio_init(27);
    adc_select_input(1);
    
    // rtc
    rtc::init();
    is_rtc = rtc::is_present();
    
    // Display ST7735 
    myTFT.TFTInitSPIType(8000, spi0);
    myTFT.TFTSetupGPIO(RST_TFT, RS_TFT, CS_TFT, SCLK_TFT, SDA_TFT);
    myTFT.TFTInitScreenSize(0, 0, 128, 160);
    myTFT.TFTInitPCBType(TFT_ST7735S_Black);
    myTFT.TFTsetRotation(TFT_Degrees_90);
    myTFT.TFTfillScreen(ST7735_BLACK);

    // SD-Karte 
    FRESULT fr = f_mount(&fs, "", 1);
    is_sd = (fr == FR_OK);
}

// Messwert in Datei auf SD-Karte schreiben
void log_data(const string& data, const string& filename) {
    FIL fil;
    UINT bw;
    FRESULT fr = f_open(&fil, filename.c_str(), FA_WRITE | FA_OPEN_APPEND);
    if (FR_OK != fr) return;
    f_write(&fil, data.c_str(), data.size(), &bw);
    f_write(&fil, "\n", 1, &bw);  
    f_close(&fil); 
}

// Schnelle ADC-Messung (500 kHz, Free-Running über FIFO), float
void read(float* u_buf, int n) {
    adc_set_clkdiv(0);
    adc_fifo_setup(true, false, 1, false, false);
    adc_fifo_drain();
    adc_run(true);
    for(int i = 0; i < n; i++) {
        while (adc_fifo_is_empty());
        u_buf[i] = (float)adc_fifo_get() * (float)conversion_factor;
    }
    adc_run(false);
}

//Bestimmen der tatsächlichen Frequenz / Werte pro Periode 
void find_period(const float* u, int n) {
    // Gleichanteil bestimmen
    double mean = 0;
    for (int i = 0; i < n; i++) mean += u[i];
    mean /= n;

    // Leistung bei Periode P (Lock-in über den ganzen Puffer)
    auto power = [&](float P) {
        float a = 0, b = 0, w = 2 * (float)M_PI / P;
        for (int i = 0; i < n; i++) {
            float v = (float)(u[i] - mean);
            a += v * cosf(w * i);
            b += v * sinf(w * i);
        }
        return a * a + b * b;
    };

    // Grobsuche: P = 110..125, Schritt 0,25
    float bestP = 0, best = 0;
    for (float P = 110; P <= 125; P += 0.25f) {
        float p = power(P);
        if (p > best) { best = p; bestP = P; }
    }
    // Feinsuche: um das Maximum herum, Schritt 0,02
    float c = bestP;
    for (float P = c - 0.25f; P <= c + 0.25f; P += 0.02f) {
        float p = power(P);
        if (p > best) { best = p; bestP = P; }
    }
    // Nur übernehmen, wenn ein deutliches Signal da ist (Sensor angeschlossen)
    double amp = 2.0 * sqrt((double)best) / n;   // mV
    if (amp > 50.0 && bestP > 0) N0 = bestP;
}

// Lock-in auf 2f mit Vorzeichen (Referenzphase = 2*Phase(1f) + PHI0)
double measure_signed(){
    float u[NMAX];
    read(u, N);

    // Offcet
    float m = 0;
    for(int i = 0; i < N; i++) m += u[i];
    m /= N;

    // Quadraturkomponenten für 1f (A1,B1) und 2f (A2,B2)
    float A1 = 0, B1 = 0, A2 = 0, B2 = 0;
    for(int i = 0; i < N; i++){
        float v = u[i] - m;
        A1 += v*cos1[i];       B1 += v*sin1[i];
        A2 += v*cos2[i];  B2 += v*sin2[i];
    }

    // Phasendifferenz bestimmen
    double phi1 = atan2((double)B1, (double)A1);
    double phi2 = atan2((double)B2, (double)A2);
    last_phi_diff = phi2 - 2.0*phi1;
    phi_sum_c += cos(last_phi_diff);
    phi_sum_s += sin(last_phi_diff);

    // Projektion von 2f auf die Referenzphase -> Wert mit Vorzeichen
    double ref = 2.0*phi1 + PHI0;
    return 2.0*(A2*cos(ref) + B2*sin(ref))/N;
}

// Medianfilter: TRIM Elemente am Anfang und Ende werden rausgeschmissen
double medianfilter(double* v, int n){
    sort(v, v + n);
    double s = 0;
    for(int i = TRIM; i < n - TRIM; i++) s += v[i];
    return s / (n - 2*TRIM);
}

int main() {
    Setup();

    // Startbildschirm
    myTFT.TFTdrawText(32, 52, "Hallo!", ST7735_BLUE, ST7735_BLACK, 3);
    sleep_ms(1000);
    myTFT.TFTfillScreen(ST7735_BLACK); 

    // Beschriftung der Messwerte
    myTFT.TFTdrawText(5, 40, "U2:", ST7735_PURPLE, ST7735_BLACK, 2);
    myTFT.TFTdrawText(5, 70, "Ph:", ST7735_CYAN, ST7735_BLACK, 2);
    adc_select_input(1);

    // Status von SD-Karte und RTC anzeigen
    if(is_sd) {
        myTFT.TFTdrawText(5, 5, "SD hier", ST7735_WHITE, ST7735_BLACK, 1);
    }
    else {
        myTFT.TFTdrawText(5, 5, "Kein SD", ST7735_WHITE , ST7735_BLACK, 1);
    }
    if(!is_rtc) {
        myTFT.TFTdrawText(5, 20, "Kein RTC", ST7735_WHITE , ST7735_BLACK, 1);
    }

    // Bestimmen der Periode und Referenztabellen
    static float buf[NMAX];
    read(buf, N);
    find_period(buf, N);

    f = 500000.0 / N0;
    N = (int)lround(5 * N0);
    if (N > NMAX) N = NMAX;
    for (int i = 0; i < N; i++) {
        double ph1 = 2.0 * M_PI * i / N0;
        sin1[i] = sin(ph1);
        cos1[i] = cos(ph1);
        sin2[i] = sin(2.0 * ph1);
        cos2[i] = cos(2.0 * ph1);
    }

    // Gefundene Frequenz und Periode anzeigen
    string info = format("f={} Hz N0={:.2f}   ", (int)f, N0);
    myTFT.TFTdrawText(5, 120, info, ST7735_WHITE, ST7735_BLACK, 1);

    // Verlauf der letzten Blöcke (zweite Mittelung für die Anzeige)
    double hist_u[MITTEL];
    int hist_n = 0, hist_pos = 0;
    
    // Zustand für die Sekunden-Mittelung (SD-Karte)
    static double U_vals[ANZAHL];
    long last_key = -1;            // Sekundenkennung der letzten RTC-Auslesung
    bool synced = false;           // erst nach der ersten Sekundengrenze loggen
    double sec_sum = 0;            // Summe aller Messwerte der laufenden Sekunde
    int sec_cnt = 0;               // Anzahl der Messungen der laufenden Sekunde
    string last_dt = "", last_fn = "messung.txt";

    while (1) {
        // Block aus ANZAHL Messungen, Phase wird dabei mitgemittelt
        phi_sum_c = 0; phi_sum_s = 0;
        uint64_t t0 = time_us_64();
        for(int i = 0; i < ANZAHL; i++){
            U_vals[i] = measure_signed();
        }
        double rate = ANZAHL * 1e6 / (double)(time_us_64() - t0);   // Messungen pro Sekunde
        double phi_avg = atan2(phi_sum_s, phi_sum_c) * 180.0 / M_PI;

        // Ausreißer verwerfen und mitteln
        double U_avg = medianfilter(U_vals, ANZAHL);

        // Zweite Mittelung: Mittel der letzten MITTEL Blöcke
        hist_u[hist_pos] = U_avg;
        hist_pos = (hist_pos + 1) % MITTEL;
        if (hist_n < MITTEL) hist_n++;
        double U_anz = 0;
        for(int i = 0; i < hist_n; i++) U_anz += hist_u[i];
        U_anz /= hist_n;

        // Messwert, mittlere Phase und Messungen pro Sekunde anzeigen
        string u_value = format("{:.2f}  ", U_anz);
        myTFT.TFTdrawText(40, 40, u_value, ST7735_PURPLE, ST7735_BLACK, 2);
        string pd = format("{:.1f}   ", phi_avg);
        myTFT.TFTdrawText(40, 70, pd, ST7735_CYAN, ST7735_BLACK, 2);
        string ns = format("Mess/s: {:.0f}   ", rate);
        myTFT.TFTdrawText(5, 105, ns, ST7735_WHITE, ST7735_BLACK, 1);

        // Zeitstempel aus der RTC; Sekundenkennung zum Erkennen des Sekundenwechsels
        string filename = "messung.txt";
        string datetime = "";
        long key;
        is_rtc = rtc::is_present();;
        if(is_rtc){ 
            rtc::DateTime now{};
            rtc::get_time(&now);
            filename = format("{:02d}{:02d}{:02d}.txt", now.year, now.month, now.date);
            datetime = format("{:02d}.{:02d}.{:02d} {:02d}:{:02d}:{:02d} ", now.date, now.month, now.year, now.hours, now.minutes,now.seconds);
            myTFT.TFTdrawText(5, 20, datetime, ST7735_WHITE, ST7735_BLACK, 1);
            key = now.seconds + 60L*now.minutes + 3600L*now.hours + 86400L*now.date;
        }
        else {
            // Keine RTC: Sekundentakt vom Pico-Timer, RTC neu initialisieren
            myTFT.TFTdrawText(5, 20, "Kein RTC              ", ST7735_WHITE, ST7735_BLACK, 1);
            rtc::init(); 
            key = (long)(time_us_64() / 1000000);
        }

        // Sekundenwechsel erkannt: Mittelwert der vergangenen Sekunde schreiben
        // (geschrieben wird nur bei Wechsel -> keine doppelte Sekunde im Log;
        //  springt die RTC um 2 s, wird einfach über 2 s gemittelt)
        if (key != last_key) {
            if (synced && sec_cnt > 0 && is_sd) {
                string line = format("{:.2f}", sec_sum / sec_cnt);
                log_data(last_dt + line, last_fn);
            }
            last_key = key;
            synced = true;
            sec_sum = 0; sec_cnt = 0;
            last_dt = datetime;    // Zeitstempel der Sekunde, die jetzt gemittelt wird
            last_fn = filename;
        }

        // Block zur laufenden Sekunde addieren (Gewicht = Anzahl Messungen)
        sec_sum += U_avg * ANZAHL;
        sec_cnt += ANZAHL;
    }
}