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
#include "ds3231/ds3231.hpp"
#include <format>
#include <string>
#include "functions.hpp"
using namespace std;

ST7735_TFT myTFT;
#define SCLK_TFT 2 
#define SDA_TFT  3
#define RS_TFT   4 
#define RST_TFT  6
#define CS_TFT   5
#define ST7735_TEAL 0x03ef
#define ST7735_GREENYELLOW 0xb7e0


const int LED_PIN = 25;
const double f = 4260.0;
const double T = 1000000.0 / f; 
const int N0 = 96;
const int N0_f = 105;
const int k = 5;
const int N = k*N0;
const int N_fast = k*N0_f;

bool is_sd = true;
bool is_sd_write = true;
bool is_rtc = true;
int method = 2;
int adc = 1;
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

typedef double (*MeasureMethod)(int, int);
MeasureMethod methods[] = {functions::method0, functions::method1, functions::method2};
int main() {
    Setup();
	myTFT.TFTdrawText(32, 52, "Hallo!", ST7735_BLUE, ST7735_BLACK, 3);
    sleep_ms(1000);
    myTFT.TFTfillScreen(ST7735_BLACK); 
    myTFT.TFTdrawText(5, 40, "U2:", ST7735_GREENYELLOW, ST7735_BLACK, 2);
    myTFT.TFTdrawText(5, 80, "T:", ST7735_TAN, ST7735_BLACK, 2);
    adc_select_input(1);

 // SD-Karte mounten

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

while (1) {
    double U_vals[150];
    for(int i = 0; i < 150; i++){
        U_vals[i] = methods[method](adc, adc == 0? N : N_fast);
    }
    sort(U_vals, U_vals+150);
    double U_avg = 0;
    for(int i = 20; i < 130; i++){
        U_avg +=U_vals[i];
    }
    U_avg /= 110.0;
    string u_value = format("{:.4f}  ", U_avg);

    myTFT.TFTdrawText(40, 40, u_value, ST7735_GREENYELLOW, ST7735_BLACK, 2);
    string filename;
    string datetime = "";
    string temps = "";
    is_rtc = rtc::ds3231_scan();
    if(is_rtc){ 
        rtc::DateTime now;
        rtc::ds3231_get_time(&now);
        double temp = rtc::ds3231_get_temp();
        filename = format("{:02d}{:02d}{:02d}.txt", now.year, now.month, now.date);
        datetime = format("{:02d}.{:02d}.{:02d} {:02d}:{:02d}:{:02d} ", now.date, now.month, now.year, now.hours, now.minutes,now.seconds);
        temps = format("{:.02f}  ", temp);
        myTFT.TFTdrawText(5, 20, datetime, ST7735_WHITE, ST7735_BLACK, 1);
        myTFT.TFTdrawText(40, 80, temps, ST7735_TAN, ST7735_BLACK, 2);
    }
    else {
        filename = "messung.txt";
        myTFT.TFTdrawText(5, 20, "No RTC              ", ST7735_WHITE, ST7735_BLACK, 1);
        myTFT.TFTdrawText(40, 80, "inaktiv", ST7735_TAN, ST7735_BLACK, 2);
        rtc::ds3231_init(); 
    }
    if(is_sd) log_data(datetime + u_value + temps, filename);
    
}
}
  