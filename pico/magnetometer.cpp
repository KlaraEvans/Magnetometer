#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/adc.h"
#include <math.h>
#include "pico/time.h"
#include "st7735/ST7735_TFT.hpp"
#include <vector>
#include <algorithm>
#include "hw_config.h"
#include "f_util.h"
#include "ff.h"
using namespace std;
#define ST7735_TEAL 0x03ef
#define ST7735_GREENYELLOW 0xb7e0
#define ST7735_TAN 0xd5b1

ST7735_TFT myTFT;
#define pi atan(1)*4
const double conversion_factor = 3.3*1000 / (1 << 12); //mV
const int LED_PIN = 25;
//const double T = 250; //µs //Periode der 1. Harmonischen
const int N = 192;
const double N0 = 96;
double sin_werte[N];
double cos_werte[N];

bool is_sd = true;
bool is_sd_write = true;

//double offcet_previous = 1.65;
void Setup(void) {	
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_init(15);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    gpio_set_dir(15, GPIO_OUT);
    gpio_put(LED_PIN, 1);
    // This example will use I2C0 on the default SDA and SCL pins (4, 5 on a Pico)
    adc_init();
    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(26); 
    adc_gpio_init(27);
    adc_gpio_init(28);
//*************** USER OPTION 0 SPI_SPEED + TYPE ***********
	bool bhardwareSPI = true; // true for hardware spi, 
	
	if (bhardwareSPI == true) { // hw spi
		uint32_t TFT_SCLK_FREQ =  8000 ; // Spi freq in KiloHertz , 1000 = 1Mhz
		myTFT.TFTInitSPIType(TFT_SCLK_FREQ, spi0); 
	} 
    else { // sw spi
		myTFT.TFTInitSPIType(); 
	}
//**********************************************************

// ******** USER OPTION 1 GPIO *********
// NOTE if using Hardware SPI clock and data pins will be tied to 
// the chosen interface eg Spi0 CLK=18 DIN=19)
	int8_t SCLK_TFT = 2; 
    int8_t SDA_TFT = 3;
    int8_t RS_TFT = 4; 
    int8_t RST_TFT = 6;
	int8_t CS_TFT = 5 ;  
	myTFT.TFTSetupGPIO(RST_TFT, RS_TFT, CS_TFT, SCLK_TFT, SDA_TFT);
//**********************************************************

// ****** USER OPTION 2 Screen Setup ****** 
	uint8_t OFFSET_COL = 0;  // 2, These offsets can be adjusted for any issues->
	uint8_t OFFSET_ROW = 0; // 3, with screen manufacture tolerance/defects
	uint16_t TFT_WIDTH = 128;// Screen width in pixels
	uint16_t TFT_HEIGHT = 160; // Screen height in pixels
	myTFT.TFTInitScreenSize(OFFSET_COL, OFFSET_ROW , TFT_WIDTH , TFT_HEIGHT);
// ******************************************

// ******** USER OPTION 3 PCB_TYPE  **************************
	myTFT.TFTInitPCBType(TFT_ST7735S_Black); // pass enum,4 choices,see README
//**********************************************************
    myTFT.TFTfillScreen(ST7735_BLACK);
	myTFT.TFTFontNum(TFTFont_Default);
    //myTFT.TFTsetRotation(TFT_rotate_e::TFT_90_degrees);
}
void log_data(const char* data) {
    FIL fil;
    UINT bw;

    FRESULT fr = f_open(&fil, "messung.txt", FA_WRITE | FA_OPEN_APPEND);
    if (FR_OK != fr) return;

    f_write(&fil, data, strlen(data), &bw);
     f_write(&fil, "\n", 1, &bw);  
    f_close(&fil); 
}
double find_K(){
    double spannung[N];
       double raw[N];
       //gpio_put(15, 1);
       for(int i = 0; i < N; i++){
            raw[i] = adc_read();
            //sleep_us(2);
       }
      // gpio_put(15, 0);
       double offcet = 0;
       for(int i = 0; i < N; i++){
            spannung[i] = raw[i] *conversion_factor;
             offcet += spannung[i];
            //sleep_us(2);
       }
       offcet /= N;
       //myTFT.TFTdrawText(5, 20, str2, ST7735_WHITE, ST7735_BLACK, 2);
       double A = 0, B = 0;
       for(int i = 0; i < N; i++){
            spannung[i] -= offcet;
            B += spannung[i]*sin_werte[i];
            A += spannung[i]*cos_werte[i];
       }
        int sign = 1;
        //offcet_previous = offcet;
       return sqrt(A*A + B * B)*sign;
}
int main() {
    Setup();
    char str[] = "Hallo!";
	myTFT.TFTdrawText(5, 5, str, ST7735_WHITE, ST7735_BLACK, 2);
    char str1[] = "U2=";
    myTFT.TFTdrawText(5, 40, str1, ST7735_GREENYELLOW, ST7735_BLACK, 2);
    for(int i = 0; i < N; i++) { // sin(2wt) u. cos(2wt)
        double phase = (double)i/N0 * (4.0*pi);
        sin_werte[i] = sin(phase);
        cos_werte[i] = cos(phase);
    }
    adc_select_input(1);

 // SD-Karte mounten
FATFS fs;
FRESULT fr = f_mount(&fs, "", 1);
is_sd = (fr == FR_OK);

if (!is_sd) { //keine SD Karte annerkant
    printf("f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
    char fehler[] = "SD Fehlt";
    myTFT.TFTdrawText(5, 80, fehler, ST7735_RED, ST7735_BLACK, 2);
}
else log_data("--------------");
while (1) {
        vector <double> K_series(100);
        for(int j = 0; j < 100; j++){
            double K = find_K();
            K_series[j] = K/(double)N;
        }
        sort(K_series.begin(), K_series.end());
        double K_average = 0;
        for(int i = 10; i < 90; i++){
             K_average +=K_series[i];
        }
        K_average /= 80.0;
        char buf[10];
        sprintf(buf, "%.3f  ", K_average);
        myTFT.TFTdrawText(40, 40, buf, ST7735_GREENYELLOW, ST7735_BLACK, 2);
        if(is_sd) log_data(buf);
        printf(buf);
    }
}
  