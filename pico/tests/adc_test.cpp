#include <stdio.h>
#include <iostream>
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

ST7735_TFT myTFT;
#define pi atan(1)*4
const double conversion_factor = 3.3*1000 / (1 << 12); //mV
const int LED_PIN = 25;
const double T = 1000000.0/4260; //µs //Periode der 1. Harmonischen
const int N0 = 91;//0,988 T = N0 * t0
const int N = N0; 

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
    myTFT.TFTsetRotation(TFT_Degrees_270);
}

int main() {
    Setup();
    adc_set_temp_sensor_enabled(false); 
    adc_select_input(1);
   sleep_ms(1000);
    while (1) {
        double spannung[N];
        for(int i = 0; i < N; i++){
            uint16_t raw = adc_read();
            spannung[i] = raw*conversion_factor-1650;
        } 
        for(int i = 0; i < N; i++) {
           char buf[10];
           sprintf(buf, "%.0f     \n", spannung[i]);
           printf(buf);
        }
}
}