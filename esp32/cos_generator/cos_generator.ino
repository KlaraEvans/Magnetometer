#include <DacESP32.h>
DacESP32 dac1(GPIO_NUM_25);
const int LED = 23;
void setup() {
  // put your setup code here, to run once:
   //dac1.outputCW(4100); //1 Blatt Sensor
  dac1.outputCW(4260); //4 Blatt Sensor
   pinMode(LED, OUTPUT);
   digitalWrite(LED, HIGH);
}

void loop() {
      //dac1.setCwScale(DAC_CW_SCALE_1); // 1 blatt Sensor 
      dac1.setCwScale(DAC_CW_SCALE_8);

}
