#include <dht.h>
#include <LiquidCrystal.h>
#include <RTClib.h>
#include <Wire.h>
#include <Stepper.h>

bool tempC;
bool tempC2;
bool waterL;
bool on = 0;

const int stepsPerRev = 360;
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const int dht_pin = A1;
const int rs = 13, en = 2, d4 = 4, d5 = 5, d6 = 6, d7 = 8;
int temp;
int motorState = 0;
int val = 0;

dht DHT;
RTC_DS1307 rtc;
Stepper myStepper(stepsPerRev, 47, 49, 51, 53);
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Define port register pointers
//digital pins 30-37
volatile unsigned char* port_c = (unsigned char*) 0x28;
volatile unsigned char* ddr_c = (unsigned char*) 0x27;
volatile unsigned char* pin_c = (unsigned char*) 0x26;
//digital pins 6,7,8,9 16,17
volatile unsigned char* port_h = (unsigned char*) 0x102;
volatile unsigned char* ddr_h = (unsigned char*) 0x101;
volatile unsigned char* pin_h = (unsigned char*) 0x100;
//digital pins 22-29
volatile unsigned char* port_a = (unsigned char*) 0x22;
volatile unsigned char* ddr_a = (unsigned char*) 0x21;
volatile unsigned char* pin_a = (unsigned char*) 0x20;
//digital pins 39, 41, 40, 4
volatile unsigned char* port_g = (unsigned char*) 0x34;
volatile unsigned char* ddr_g = (unsigned char*) 0x33;
volatile unsigned char* pin_g = (unsigned char*) 0x32;

//ADC registers
#define RDA 0x80
#define TBE 0x20  
volatile unsigned char *myUCSR0A = (unsigned char *)0x00C0;
volatile unsigned char *myUCSR0B = (unsigned char *)0x00C1;
volatile unsigned char *myUCSR0C = (unsigned char *)0x00C2;
volatile unsigned int  *myUBRR0  = (unsigned int *) 0x00C4;
volatile unsigned char *myUDR0   = (unsigned char *)0x00C6;
volatile unsigned char* my_ADMUX = (unsigned char*) 0x7C;
volatile unsigned char* my_ADCSRB = (unsigned char*) 0x7B;
volatile unsigned char* my_ADCSRA = (unsigned char*) 0x7A;
volatile unsigned int* my_ADC_DATA = (unsigned int*) 0x78;

void setup() {
U0init(9600); //initialize serial monitor at baud rate of 9600
adc_init();
#ifndef ESP8266 //waits for a serial connection to be established
  while (!Serial);
#endif
myStepper.setSpeed(40);//sets a speed for the stepper motor

while (!rtc.begin()){ //checks to ensure the real time clock module is running
   Serial.println("Couldn't find RTC");
}
  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running, let's set the time!");
  }

delay(500); //delay as to not instantly start the loop function
lcd.begin(16,2); //sets the lcd to be configured to have 16 characters per line and 2 lines
*ddr_c |= 0b01001000; //sets digital pin 31(motor) to output and yellow LED (pin 34) to OUTPUT
*ddr_c &= 0b11111010; //sets digital pin 37 (button down for motor) and 35 (on/off button) to INPUT
*ddr_h |= 0b00010000; //sets digital pin 7(water sensor power to output)
*ddr_a |= 0b10001010; //sets digital pin 25green led to OUTPUT, and 23(blue LED to OUTPUT), digital pin 29 RED LED OUTPUT
*ddr_g &= 0b11111011; //sets the button up pin 39 to an INPUT
*port_h &= 0b11101111; //initially sets water sensor to LOW
}

void loop() {

if (*pin_c & 0b000000100){//if on button is pushed system turns on
  on = 1;
  delay(1000);
  }
  *port_c &= 0b10111111; //write motor pin 31 LOW when system is off
  *port_a &= 0b01110101; //sets blue, green and red LED LOW
  *port_c |= 0b00001000; //sets yellow LED HIGH
while(on == 1){
*port_c &= 0b11110111; //SETS YELLOW LED LOW
if (*pin_c & 0b000000100){
  delay (1000);
  on = 0;
  }
    if ((*pin_g & 0b00000100) && (waterL == 1)){ //if button up pg2 is high and it is not in ERROR state move stepper up
     for(int i=0;i<80;i++){
      int steps=-1;
      myStepper.step(steps);
     }
    }
    else if ((*pin_c & 0b00000001) && (waterL == 1)){ // if button down pc0 goes high and it is not in ERROR state move stepper down
      for(int i=0;i<80;i++){
        int steps=1;
        myStepper.step(steps);
      }    
    }
    
    tempC = tempC2; //sets tempC variable equal to tempC2
    temp = DHT.temperature; //sets temperature variable to the reading of the temp sensor
    DHT.read11(dht_pin); //reads the value from the temp sensor
    if (waterL == 1){ //if the water level is within the proper threshold
      lcd.print("TEMP = "); //prints the temperature to the LCD
      lcd.print(DHT.temperature);
      lcd.print("C ");
      lcd.setCursor(0,1);
      lcd.print("HUM  = ");
      lcd.print(DHT.humidity);
      lcd.print("% ");
      delay(1000); //delay since without the delay the LCD flickers
    }
      if ((temp > 20) && (waterL == 1)){ //if the temperature and water lvl is above the threshold turn on the fan
      *port_a |= 0b00000010; //sets blue LED HIGH
      *port_a &= 0b01110111; //sets green and red led LOW pin 25
      *port_c |= 0b01000000; //write motor pin 31 HIGH           //FAN ON
          tempC = 1;
      }
      else if ((temp <= 20) && (waterL == 1)){ //if the temp is below threshold and water lvl is within threshold turn off fan
        *port_c &= 0b10111111; //write motor pin 31 LOW
        *port_a |= 0b00001000; //sets green LED HIGH       //IDLE MODE
        *port_a &= 0b01111101; //sets blue and red LED LOW
        tempC = 0;
      }
    readSensor();
    if (tempC != tempC2){ //If the state of fan changes the date and time are printed to the serial monitor
            DateTime now = rtc.now();
            Serial.print(now.year(), DEC);
            Serial.print('/');
            Serial.print(now.month(), DEC);
            Serial.print('/');
            Serial.print(now.day(), DEC);
            Serial.print(" (");
            Serial.print(daysOfTheWeek[now.dayOfTheWeek()]);
            Serial.print(") ");
            Serial.print(now.hour(), DEC);
            Serial.print(':');
            Serial.print(now.minute(), DEC);
            Serial.print(':');
            Serial.print(now.second(), DEC);
            Serial.println();
            tempC2 = tempC;
    }
    }
}
    void readSensor() { //function for reading the water depth sensor
      *port_h |= 0b00010000; //sets pin7 watersensor pin HIGH
      val = adc_read(0); //reads analog value from water sensor
      //print_int(val); //prints the value read from the sensor to the serial monitor
      *port_h &= 0b11101111; //sets pin7 watersensor pin LOW
      if (val < 150){
          waterL = 0;
          Serial.print("WARNING WATER LEVEL TOO LOW PLEASE REFILL\n");
          lcd.clear();
          lcd.print("ERROR WATER LOW");
          *port_a |= 0b10000000; //sets red LED HIGH
          *port_a &= 0b11110101; //sets green and blue LED LOW
          *port_c &= 0b10111111; //write motor pin31 LOW
          delay(1000);
          lcd.clear();
        } 
      else if(val >= 100){
        waterL = 1;
        *port_a &= 0b01111111; //sets red LED LOW
        lcd.clear();
      }
     }


void adc_init()
{
  // setup the A register
  *my_ADCSRA |= 0b10000000;  // set bit   7 to 1 to enable the ADC
  *my_ADCSRA &= 0b11011111;  // clear bit 5 to 0 to disable the ADC trigger mode
  *my_ADCSRA &= 0b11011111;  // clear bit 5 to 0 to disable the ADC interrupt
  *my_ADCSRA &= 0b11011111;  // clear bit 5 to 0 to set prescaler selection to slow reading
  // setup the B register
  *my_ADCSRB &= 0b11110111;  // clear bit 3 to 0 to reset the channel and gain bits
  *my_ADCSRB &= 0b11111000;  // clear bit 2-0 to 0 to set free running mode
  // setup the MUX Register
  *my_ADMUX  &= 0b01111111; // clear bit 7 to 0 for AVCC analog reference
  *my_ADMUX  |= 0b01000000; // set bit   6 to 1 for AVCC analog reference
  *my_ADMUX  &= 0b11011111;  // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11011111;  // clear bit 5 to 0 for right adjust result
  *my_ADMUX  &= 0b11100000;  // clear bit 4-0 to 0 to reset the channel and gain bits
}
unsigned int adc_read(unsigned char adc_channel_num)
{
  // clear the channel selection bits (MUX 4:0)
  *my_ADMUX  &= 0b11100000;
  // clear the channel selection bits (MUX 5)
  *my_ADCSRB &= 0b11000000;
  // set the channel number
  if(adc_channel_num < 1)
  {
    // set the channel selection bits, but remove the most significant bit (bit 3)
    adc_channel_num -= 0;
    // set MUX bit 5
    *my_ADCSRB |= 0b00100000;
  }
  // set the channel selection bits
  *my_ADMUX  += adc_channel_num;
  // set bit 6 of ADCSRA to 1 to start a conversion
  *my_ADCSRA |= 0x40;
  // wait for the conversion to complete
  while((*my_ADCSRA & 0x40) != 0);
  // return the result in the ADC data register
  return *my_ADC_DATA;
}
void print_int(unsigned int out_num)
{
  // clear a flag (for printing 0's in the middle of numbers)
  unsigned char print_flag = 0;
  // if its greater than 1000
  if(out_num >= 1000)
  {
    // get the 1000's digit, add to '0' 
    U0putchar(out_num / 1000 + '0');
    // set the print flag
    print_flag = 1;
    // mod the out num by 1000
    out_num = out_num % 1000;
  }
  // if its greater than 100 or we've already printed the 1000's
  if(out_num >= 100 || print_flag)
  {
    // get the 100's digit, add to '0'
    U0putchar(out_num / 100 + '0');
    // set the print flag
    print_flag = 1;
    // mod the output num by 100
    out_num = out_num % 100;
  } 
  // if its greater than 10, or we've already printed the 10's
  if(out_num >= 10 || print_flag)
  {
    U0putchar(out_num / 10 + '0');
    print_flag = 1;
    out_num = out_num % 10;
  } 
  // always print the last digit (in case it's 0)
  U0putchar(out_num + '0');
  // print a newline
  U0putchar('\n');
}
void U0init(int U0baud)
{
 unsigned long FCPU = 16000000;
 unsigned int tbaud;
 tbaud = (FCPU / 16 / U0baud - 1);
 // Same as (FCPU / (16 * U0baud)) - 1;
 *myUCSR0A = 0x20;
 *myUCSR0B = 0x18;
 *myUCSR0C = 0x06;
 *myUBRR0  = tbaud;
}
unsigned char U0kbhit()
{
  return *myUCSR0A & RDA;
}
unsigned char U0getchar()
{
  
  return *myUDR0;
}
void U0putchar(unsigned char U0pdata)
{
  while((*myUCSR0A & TBE)==0);
  *myUDR0 = U0pdata;
}
