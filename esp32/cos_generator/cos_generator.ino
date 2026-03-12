#include <DacESP32.h>
DacESP32 dac1(GPIO_NUM_25);
const int LED = 2;
//const int sign = 23;
void setup() {
  // put your setup code here, to run once:
   dac1.outputCW(4100); 
   pinMode(LED, OUTPUT);
   //pinMode(23, INPUT);
   digitalWrite(LED, HIGH);
}

void loop() {
      dac1.setCwScale(DAC_CW_SCALE_1); 
}
