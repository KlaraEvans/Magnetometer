#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include <math.h>
#include "pico/time.h"
#include "../ST7735/include/st7735/ST7735_TFT.hpp"
#include <vector>
#include <algorithm>
#include "hw_config.h"
#include "f_util.h"
#include "ff.h"
#include "../ds3231/ds3231.hpp"
#include <format>
#include <string>

using namespace std;

ST7735_TFT myTFT;
#define SCLK_TFT 2 
#define SDA_TFT  3
#define RS_TFT   4 
#define RST_TFT  6
#define CS_TFT   5
#define ST7735_TEAL 0x03ef
#define ST7735_GREENYELLOW 0xb7e0

#define pi atan(1)*4
const double conversion_factor = 3.3*1000 / (1 << 12); //mV
const int LED_PIN = 25;
const double f = 4260.0;
const double T = 1000000.0 / 4260.0; 
const int k = 5;
const int N0 = 53;
const int N0_f = 105;

int factor[2];
const int N = N0*k;
const int N_fast = N0_f*k;

const int delay_ms = 1000;
const int x = 150;

bool is_sd = true;
bool is_sd_write = true;
bool is_rtc = true;
int adcMethod = 0;

uint32_t load_time = 0;
uint32_t table_time[2]; 
uint32_t time_m[3];
uint32_t time_m_f[3];
uint32_t time_avg = 0; 
uint32_t rtc_no = 0;
uint32_t rtc_yes = 0;
uint32_t sd_rtc_no = 0;
uint32_t sd_rtc_yes = 0;

int method = 2;
double sin_werte[N_fast], cos_werte[N_fast];
FATFS fs; 
void Setup(void) {	
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_put(LED_PIN, 1);

    gpio_set_dir(23, GPIO_OUT);
    gpio_put(23, 1);

    adc_init();
    adc_gpio_init(27);
    adc_gpio_init(28);
    adc_select_input(1);
    
    rtc::ds3231_init();
    is_rtc = rtc::ds3231_scan();
    
    myTFT.TFTInitSPIType(8000, spi0);
    myTFT.TFTSetupGPIO(RST_TFT, RS_TFT, CS_TFT, SCLK_TFT, SDA_TFT);
    myTFT.TFTInitScreenSize(0, 0, 128, 160);
    myTFT.TFTInitPCBType(TFT_ST7735S_Black);
    myTFT.TFTsetRotation(TFT_Degrees_90);
    myTFT.TFTfillScreen(ST7735_BLACK);

    FRESULT fr = f_mount(&fs, "", 1);
    is_sd = (fr == FR_OK);
}


void log_data(const string& data, const string& filename) {
    FIL fil;
    UINT bw;

    FRESULT fr = f_open(&fil, filename.c_str(), FA_WRITE | FA_OPEN_APPEND);
    if (FR_OK != fr) return;

    f_write(&fil, data.c_str(), data.size(), &bw);
    f_write(&fil, "\n", 1, &bw);  
    f_close(&fil); 
}

//adc Messungen:
void fast(double* u_buf, int n) {
    adc_set_clkdiv(0);        
    adc_fifo_setup(true, false, 1, false, false);
    adc_fifo_drain();
    adc_run(true);
    for(int i = 0; i < n; i++) {
        while (adc_fifo_is_empty());
        u_buf[i] = (double)adc_fifo_get() * conversion_factor;
    }
    adc_run(false);
}
void standart(double* u_buf, int n){
    for (int i = 0; i < n; i++) {
        u_buf[i] = (double)adc_read() * conversion_factor;
    }
} 
//Berechnung:
double method0(){
        int n = N_fast;
        double spannung[N_fast];
        switch (adcMethod) {
        case 0:
            standart(spannung, N);
            n = N;
            break;
        case 1:
            fast(spannung, N_fast);
            break;
        }
       double offcet = 0;
       for(int i = 0; i < n; i++){
             offcet += spannung[i];
       }
       offcet /= n;
       double A = 0, B = 0;
       for(int i = 0; i < n; i++){
            spannung[i] -= offcet;
            B += spannung[i]*sin_werte[i];
            A += spannung[i]*cos_werte[i];
       }
        int sign = 1;
       return sqrt(A*A + B * B)*sign/n;
}
double method1(){
    int n = N_fast;
    double spannung[N_fast];
    switch (adcMethod) {
        case 0:
            standart(spannung, N);
            n = N;
            break;
        case 1:
            fast(spannung, N_fast);
            break;
        }
    //finden, wo lokales max ist und wo locales min
    int erste_max = 0, erste_min = 0;
    bool emax_set = false;
    bool emin_set = false;
    for(int i = 1; i < n-1; i++){
        if(spannung[i-1] < spannung[i] && spannung[i] > spannung[i+1] && !emax_set) {
            erste_max = i;
            emax_set = true;
        }
        if(spannung[i-1] > spannung[i] && spannung[i] < spannung[i+1] && !emin_set) {
            erste_min = i;
            emin_set = true;
        }
        if(emin_set && emax_set) break;
    }

    sort(spannung, spannung + n);
       double max = 0, min = 0;
       for(int i = 0; i < 2*k; i++) { //Die Spannungsdaten haben die Peiode von 2f
        min += spannung[i];
        max += spannung[n-1-i];
       }
       max /= (2*k); min /= (2*k);
       int sign = erste_max < erste_min ? 1 : -1;
       double U2 = (max - min)/2.0;
       return U2*sign;
}
double method2(){
    int n = N_fast;
    double spannung[N_fast];
    switch (adcMethod) {
        case 0:
            standart(spannung, N);
            n = N;
            break;
        case 1:
            fast(spannung, N_fast);
            break;
    }
    double sum_u = 0;
    for(int i = 0; i < n; i++) { sum_u += spannung[i];}
    double off_u = sum_u / n;

    double sq_u = 0;
    for(int i = 0; i < n; i++) {
        double du = spannung[i] - off_u;
        sq_u += du * du;
    }
    return sqrt(sq_u / n) * sqrt(2);
}
typedef double (*MeasureMethod)();
MeasureMethod methods[] = { method0, method1, method2};

int main() {
    //Load time
    Setup();
    uint32_t time1 = time_us_32();
	myTFT.TFTdrawText(32, 52, "Hallo!", ST7735_BLUE, ST7735_BLACK, 3);
    sleep_ms(delay_ms);
    myTFT.TFTfillScreen(ST7735_BLACK);
    myTFT.TFTdrawText(5, 40, "U2:", ST7735_GREENYELLOW, ST7735_BLACK, 2);
    myTFT.TFTdrawText(5, 80, "T:", ST7735_TAN, ST7735_BLACK, 2);
    if(is_sd) {
        myTFT.TFTdrawText(5, 5, "SD present", ST7735_WHITE, ST7735_BLACK, 1);
    }
    else { //keine SD Karte annerkant
        myTFT.TFTdrawText(5, 5, "No SD", ST7735_WHITE , ST7735_BLACK, 1);
    }
    if(!is_rtc) {
        myTFT.TFTdrawText(5, 20, "No RTC", ST7735_WHITE , ST7735_BLACK, 1);
    }
    myTFT.TFTdrawText(5, 120, "M"s+ to_string(method) + "  " + to_string((int)f) + " Hz  fast  " + to_string(N0) + "  "s + to_string(k), ST7735_WHITE, ST7735_BLACK, 1);
    uint32_t time2 = time_us_32();
    load_time = time2-time1;
    myTFT.TFTfillScreen(ST7735_BLACK);
    //N0 und N0_f überprüfen
   {uint32_t dt_av = 0;
    for(int rep = 0; rep < 10; rep++) {
        double spannung[N0];
        time1 = time_us_32();
        standart(spannung, N0);
        time2 = time_us_32();
        uint32_t dt = time2 - time1;
       dt_av += dt;
    }
    dt_av /= 10;
      factor[0] = floor(T / dt_av * N0);
    }
    {uint32_t dt_av = 0;
for(int rep = 0; rep < 10; rep++) {
    double spannung[N0_f];
    time1 = time_us_32();
    fast(spannung, N0_f);
    time2 = time_us_32();
    uint32_t dt = time2 - time1;
    dt_av += dt;
    }
    dt_av/=10;
    factor[1] = floor(T / dt_av * N0_f);
    }


    //Tabellenkalkulation
    //erst nur bis N ausfüllen
    time1 = time_us_32();
    for(int i = 0; i < N; i++) { // sin(2wt) u. cos(2wt)
        double phase = (double)i/N0 * (4.0*pi);
        sin_werte[i] = sin(phase);
        cos_werte[i] = cos(phase);
    }
    time2 = time_us_32();
    uint32_t dt = time2-time1;
    table_time[0] = dt; table_time[1] = dt*N_fast/N;

    //Dauer der Methode 0:
    {uint32_t dt_av = 0;
    for(int i = 0; i < 10; i++){
        time1 = time_us_32();
        double U = methods[0]();
        time2 = time_us_32();
        dt_av += time2-time1;
    }
     time_m[0] = dt_av/10;
    }
    
    //Dauer der Methode 1:
    {uint32_t dt_av = 0;
    for(int i = 0; i < 10; i++){
        time1 = time_us_32();
        double U = methods[1]();
        time2 = time_us_32();
        dt_av += time2-time1;
    }
     time_m[1] = dt_av/10;
    }

    //Dauer der Methode 2:
    {uint32_t dt_av = 0;
    for(int i = 0; i < 10; i++){
        time1 = time_us_32();
        double U = methods[2]();
        time2 = time_us_32();
        dt_av += time2-time1;
    }
     time_m[2] = dt_av/10;
    }
    adcMethod = 1;

    {for(int i = 0; i < N_fast; i++) { // sin(2wt) u. cos(2wt)
        double phase = (double)i/N0_f * (4.0*pi);
        sin_werte[i] = sin(phase);
        cos_werte[i] = cos(phase);
    }
    uint32_t dt_av = 0;
    for(int i = 0; i < 10; i++){
        time1 = time_us_32();
        double U = methods[0]();
        time2 = time_us_32();
        dt_av += time2-time1;
    }
     time_m_f[0] = dt_av/10;
    }

    

    {uint32_t dt_av = 0;
    for(int i = 0; i < 10; i++){
        time1 = time_us_32();
        double U = methods[1]();
        time2 = time_us_32();
        dt_av += time2-time1;
    }
     time_m_f[1] = dt_av/10;
    }
     
    //Dauer der Methode 2:
    {uint32_t dt_av = 0;
    for(int i = 0; i < 10; i++){
        time1 = time_us_32();
        double U = methods[2]();
        time2 = time_us_32();
        dt_av += time2-time1;
    }
     time_m_f[2] = dt_av/10;
    }

    //Dauer der Mittelung (ohne Auslesen) für x Werte:
    time1 = time_us_32();
    double U_vals[x]={};
    sort(U_vals, U_vals+x);
    double U_avg = 0;
    for(int i = (int)(0.2*x); i < (int)(0.8*x); i++){
        U_avg +=U_vals[i];
    }
    U_avg /= (0.6*x);
    string u_value = format("{:.4f}  ", U_avg);
    myTFT.TFTdrawText(40, 40, u_value, ST7735_GREENYELLOW, ST7735_BLACK, 2);
    time2 = time_us_32();
    time_avg = time2-time1;
    myTFT.TFTfillScreen(ST7735_BLACK);
    

    //Dauer des RTC Info:
    //1. No RTC
    string filename;
    string datetime = "";
    string temps = "";
    time1 = time_us_32();
    is_rtc = rtc::ds3231_scan();
    filename = "messung.txt";

    myTFT.TFTdrawText(5, 20, "No RTC              ", ST7735_WHITE, ST7735_BLACK, 1);
    myTFT.TFTdrawText(40, 80, "inaktiv", ST7735_TAN, ST7735_BLACK, 2);
    rtc::ds3231_init(); 
    time2 = time_us_32();
    rtc_no = time2 - time1;
    myTFT.TFTfillScreen(ST7735_BLACK);
    //SD Karte:
    for(int i = 0; i < 10; i++){
        time1 = time_us_32();
        if(is_sd) log_data(datetime + u_value + temps, filename);
        time2 = time_us_32();
        sd_rtc_no += time2-time1;
    }
    sd_rtc_no /= 10;
    
    //2. RTC
    time1 = time_us_32();
    is_rtc = rtc::ds3231_scan();
    rtc::DateTime now;
    rtc::ds3231_get_time(&now);
    double temp = rtc::ds3231_get_temp();
    filename = format("{:02d}{:02d}{:02d}.txt", now.year, now.month, now.date);
    datetime = format("{:02d}.{:02d}.{:02d} {:02d}:{:02d}:{:02d} ", now.date, now.month, now.year, now.hours, now.minutes,now.seconds);
    temps = format("{:.02f}  ", temp);
    myTFT.TFTdrawText(5, 20, datetime, ST7735_WHITE, ST7735_BLACK, 1);
    myTFT.TFTdrawText(40, 80, temps, ST7735_TAN, ST7735_BLACK, 2);
    time2 = time_us_32();
    rtc_yes=time2-time1;
    //SD
    for(int i = 0; i < 10; i++) {
        time1 = time_us_32();
        if(is_sd) log_data(datetime + u_value + temps, filename);
        time2 = time_us_32();
        sd_rtc_yes += time2-time1;
    }
    sd_rtc_yes /= 10;
  
    

    //Ausgabe der Ergebnisse
    myTFT.TFTsetRotation(TFT_Degrees_0);
    myTFT.TFTfillScreen(ST7735_BLACK);
    myTFT.TFTdrawText(5, 5, "Results (us): ", ST7735_GREENYELLOW, ST7735_BLACK, 1);
    myTFT.TFTdrawText(5, 15, format("Load: {:d}", load_time), ST7735_WHITE, ST7735_BLACK, 1);
    myTFT.TFTdrawText(5, 25, format("Tabl.: {:d} | {:d}", table_time[0], table_time[1]), ST7735_WHITE, ST7735_BLACK, 1);
    myTFT.TFTdrawText(5, 35, format("N0: {:d} | {:d} k: {:d}", N0, N0_f, k), ST7735_WHITE, ST7735_BLACK, 1);
    myTFT.TFTdrawText(5, 45, format("Right N0: {:d} | {:d}", factor[0], factor[1]), ST7735_WHITE, ST7735_BLACK, 1);
    
    myTFT.TFTdrawText(5, 55, format("M0: {:d} | {:d}", time_m[0], time_m_f[0]), ST7735_WHITE, ST7735_BLACK, 1);
    myTFT.TFTdrawText(5, 65, format("M1: {:d} | {:d}", time_m[1], time_m_f[1]), ST7735_WHITE, ST7735_BLACK, 1);
    myTFT.TFTdrawText(5, 75, format("M2: {:d} | {:d}", time_m[2], time_m_f[2]), ST7735_WHITE, ST7735_BLACK, 1);
    
    myTFT.TFTdrawText(5, 85, format("Avg. only: {:d}", time_avg), ST7735_WHITE, ST7735_BLACK, 1);
    myTFT.TFTdrawText(5, 105, format("D/T: {:d} | {:d}", rtc_no, rtc_yes), ST7735_WHITE, ST7735_BLACK, 1);
    if(is_sd){
        myTFT.TFTdrawText(5, 115, format("Sd: {:d} | {:d}", sd_rtc_no, sd_rtc_yes), ST7735_WHITE, ST7735_BLACK, 1);
    }
    else myTFT.TFTdrawText(5, 115, "No SD ", ST7735_WHITE, ST7735_BLACK, 1);
    while(true);
}
  