#include <SPI.h>
//using esp32 vspi pin configuraton
#define sck_pin 18
#define ADC_miso 19
#define DAC_mosi 23
#define rc_pin 4
#define ADC_cs_pin 5
#define DAC_cs_pin 17
#define busy_pin 27
#define range 20.0 //table 4 page 20 of datasheet
#define DAC_refV 2.5 //reference voltage 2.5V from datasheet, can change if needed
float dac_volts = 0; 

//Helper functions

bool busySetting(int setting, int buffer) { //whether busy_pin is set to HIGH(1) or LOW(0)
  int time = micros(); //counting microseconds
  while (digitalRead(busy_pin) != setting) { //digitalRead returns bool of HIGH/LOW setting of busy_pin
    if (micros() - time > buffer) return false; //buffer time becasue digitalRead might read old setting before it changes
  }
  return true;
}

bool ADCmeasure() {
  digitalWrite(ADC_cs_pin, LOW);  // Ensure ADC is selected during the pulse
  digitalWrite(rc_pin, HIGH);
  delayMicroseconds(1);
  digitalWrite(rc_pin, LOW);      // Falling edge triggers conversion
  delayMicroseconds(1);           // Wait for tw1 pulse width requirement
  digitalWrite(rc_pin, HIGH);     // Bring it back HIGH immediately

  if (!busySetting(LOW, 50) || !busySetting(HIGH, 50)) { 
    digitalWrite(ADC_cs_pin, HIGH); 
    return false; // failed to measure correctly
  }
  
  digitalWrite(ADC_cs_pin, HIGH); 
  return true;
}

//SPISettings ADCSetting(speedMaximum, dataOrder, dataMode);
//https://docs.arduino.cc/language-reference/en/functions/communication/SPI/SPISettings/ 

//ADC datasheet external clock speed limit 0.1 -> 26 MHz, choosing 5MHz for now
SPISettings ADCsetting(5000000, MSBFIRST, SPI_MODE1);
SPISettings DACsetting(5000000, MSBFIRST, SPI_MODE0);

uint16_t ADC_read(){
  uint16_t data;
  SPI.beginTransaction(ADCsetting);
  digitalWrite(ADC_cs_pin, LOW);   
  data = SPI.transfer16(0); //returns the data, 16 bits, inside is 0 because there is no mosi wire
  digitalWrite(ADC_cs_pin, HIGH);  
  SPI.endTransaction();
  return data;
}

void DAC_write(float voltage) {
  if (voltage < 0){
    voltage = 0;
  }
  if (voltage > DAC_refV){
    voltage = DAC_refV;
  }

  uint32_t code = (uint32_t)(voltage * 65536.0 / DAC_refV);
  if (code > 65535) code = 65535;
  SPI.beginTransaction(DACsetting);
  digitalWrite(DAC_cs_pin, LOW);  // Select DAC, CS low=active
  SPI.transfer16((uint16_t)code);
  digitalWrite(DAC_cs_pin, HIGH); // Deselect DAC
  SPI.endTransaction();
}

void setup() {
  Serial.begin(115200);

  pinMode(ADC_cs_pin, OUTPUT);  
  pinMode(DAC_cs_pin, OUTPUT);
  pinMode(rc_pin, OUTPUT);
  pinMode(busy_pin, INPUT);
  digitalWrite(rc_pin, HIGH); //default setting
  digitalWrite(ADC_cs_pin, HIGH); 
  digitalWrite(DAC_cs_pin, HIGH);
  
  //(sck, miso, mosi, cs)
  //mosi is for recieving data, so dac pin
  //no cs pin because we want to manually control the two from ADC and DAC
  SPI.begin(sck_pin,ADC_miso,DAC_mosi,-1); 
  if (ADCmeasure()){
    ADC_read(); //first conversion following a powerup is invalid
  }
}

void loop() {
  uint16_t data;
  int16_t data2;
  float voltage_unsigned;
  float voltage_signed;

  if (ADCmeasure()){
    data = ADC_read();
    //case for unsigned
    voltage_unsigned = (float)data * range/65536.0; //65536 = 2^16
    
    //case for signed?
    data2 = (int16_t)data;//change type to signed

    // casting to an int16_t divides the register into + and - segments
    voltage_signed = (float)data2 * (range / 2.0) / 32768.0; 
    
    Serial.print("Unsigned Voltage: ");
    Serial.println(voltage_unsigned,3);
    Serial.print("Signed Voltage: ");
    Serial.println(voltage_signed,3);
  }
  else{
    Serial.println("No response");
  }
  
  DAC_write(dac_volts); //sends result written in dac_volts
  Serial.print("DAC = ");
  Serial.println(dac_volts, 3);

  dac_volts += 0.25;
  if (dac_volts > DAC_refV) dac_volts = 0;   //ramp 0 -> DAC_refV, then repeat
  
  delay(1000); 
}
