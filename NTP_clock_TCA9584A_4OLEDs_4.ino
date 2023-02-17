/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/tca9548a-i2c-multiplexer-esp32-esp8266-arduino/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  v.0 - original version from Rui's site
  v.1 - original sketch by Nicu FLORICA (niq_ro)
      - NTP clock inspired by https://nicuflorica.blogspot.com/2022/10/ceas-ntp-pe-afisaj-cu-tm1637.html
      - hardware switch for DST (Daylight Saving Time) as at https://nicuflorica.blogspot.com/2022/12/ceas-ntp-pe-afisaj-oled-de-096-128x64.html
  v.2 - added 7-segment numbers, based in ideea from https://ta-laboratories.com/blog/2018/09/07/recreating-a-7-segment-display-with-adafruit-gfx-ssd1306-oled/
  v.2a - small correction in sketch (clean unusefull lines)
  ver.3 - display info just at changes
  ver.3a - autoreset after 1 minutes
  ver.4 - added 60 leds ring as in https://github.com/leonvandenbeukel/Round-LED-Clock
*********/

#include <Wire.h>
#include <Adafruit_GFX.h>  // https://github.com/adafruit/Adafruit-GFX-Library
#include <Adafruit_SSD1306.h>  // https://github.com/adafruit/Adafruit_SSD1306
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <FastLED.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

const char *ssid     = "niq_ro";
const char *password = "berelaburtica";
const long timezoneOffset = 2*3600; // GMT + seconds  (Romania)

#define DSTpin   14 // GPIO14 = D5, see https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", timezoneOffset);

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

int ora, minut, secunda;
byte DST = 0;
byte DST0;

byte oz1, ou1, mz1, mu1;
byte oz0 = 10;
byte ou0 = 10;
byte mz0 = 10;
byte mu0 = 10;
byte secunda0 = 60;

// define segment truth table for each digit
static const int digit_array[10][7] = {
  {1, 1, 1, 1, 1, 1, 0},  // 0
  {0, 1, 1, 0, 0, 0, 0},  // 1
  {1, 1, 0, 1, 1, 0, 1},  // 2
  {1, 1, 1, 1, 0, 0, 1},  // 3
  {0, 1, 1, 0, 0, 1, 1},  // 4
  {1, 0, 1, 1, 0, 1, 1},  // 5
  {1, 0, 1, 1, 1, 1, 1},  // 6
  {1, 1, 1, 0, 0, 0, 0},  // 7
  {1, 1, 1, 1, 1, 1, 1},  // 8
  {1, 1, 1, 1, 0, 1, 1}   // 9
};

uint8_t n1 = 38;
uint8_t n3 = 8;
uint8_t n2 = 51;
uint8_t r = 4;

unsigned long tpchange;
unsigned long tpscurs = 5000;

// Change the colors here if you want.
// Check for reference: https://github.com/FastLED/FastLED/wiki/Pixel-reference#predefined-colors-list
// You can also set the colors with RGB values, for example red:
// CRGB colorHour = CRGB(255, 0, 0);
CRGB colorHour = CRGB::Red;
CRGB colorMinute = CRGB::Green;
CRGB colorSecond = CRGB::Blue;
CRGB colorHourMinute = CRGB::Yellow;
CRGB colorHourSecond = CRGB::Magenta;
CRGB colorMinuteSecond = CRGB::Cyan;
CRGB colorAll = CRGB::White;

// Set this to true if you want the hour LED to move between hours (if set to false the hour LED will only move every hour)
#define USE_LED_MOVE_BETWEEN_HOURS true

// Cutoff times for day / night brightness.
#define USE_NIGHTCUTOFF true   // Enable/Disable night brightness
#define MORNINGCUTOFF 8         // When does daybrightness begin?   8am
#define NIGHTCUTOFF 19          // When does nightbrightness begin? 10pm
#define NIGHTBRIGHTNESS 4      // Brightness level from 0 (off) to 255 (full brightness)
#define DAYBRIGHTNESS 20


#define NUM_LEDS 60     
#define DATA_PIN D6
CRGB LEDs[NUM_LEDS];

byte inel_ora;
byte inel_minut;
byte inel_secunda;
byte secundar;

// Select I2C BUS
void TCA9548A(uint8_t bus){
  Wire.beginTransmission(0x70);  // TCA9548A address
  Wire.write(1 << bus);          // send byte to select bus
  Wire.endTransmission();
  Serial.print(bus);
}
 
void setup() {
  pinMode (DSTpin, INPUT);

 //FastLED.delay(3000);
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(LEDs, NUM_LEDS);  

  
  Serial.begin(115200);
  Serial.println (" ");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay (500);
    Serial.print (".");
  }
    Serial.println (" ");
    
  if (digitalRead(DSTpin) == LOW)
   DST = 0;
  else
   DST = 1;
  timeClient.setTimeOffset(timezoneOffset + DST*3600);
  timeClient.begin();
  DST0 = DST;
  
  // Start I2C communication with the Multiplexer
  Wire.begin();

  // Init OLED display on bus number 2
  TCA9548A(2);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  } 
  display.setRotation(1); // 
  display.setTextColor(WHITE);
  display.println("1");
  // Clear the buffer
  display.clearDisplay();

  // Init OLED display on bus number 3
  TCA9548A(3);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  } 
  display.setRotation(1); // 
  display.setTextColor(WHITE);
  display.println("2");
  // Clear the buffer
  display.clearDisplay();

  // Init OLED display on bus number 4
  TCA9548A(4);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  } 
  display.setRotation(1); // 
  display.setTextColor(WHITE);
  display.println("3");
  // Clear the buffer
  display.clearDisplay();

  // Init OLED display on bus number 5
  TCA9548A(5);
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  } 
  display.setRotation(1); // 
  display.setTextColor(WHITE);
  display.println("4");
  // Clear the buffer
  display.clearDisplay();

delay(1000);
}
 
void loop() {
 if (digitalRead(DSTpin) == LOW)
   DST = 0;
  else
   DST = 1;
  
timeClient.update();
 ora = timeClient.getHours();
 minut = timeClient.getMinutes();
 secundar = timeClient.getSeconds();
/*
 ora = ora%12;  // 12-hour format
 if (ora == 0) ora = 12;  // 12-hour format
*/

oz1 = ora/10;
ou1 = ora%10;
mz1 = minut/10;
mu1 = minut%10;

Serial.println("-----------");
Serial.print(ora);
Serial.print(":");
Serial.println(minut);

if (oz1 != oz0)
{
 // Write to OLED on bus number 2
  TCA9548A(2);
  display.clearDisplay();
  number(0, 0, ora/10, WHITE);
  display.display(); 
}

if ((ou1 != ou0) or (secunda != secunda0))
{
 // Write to OLED on bus number 3
  TCA9548A(3);
  display.clearDisplay();
  number(0, 0, ora%10, WHITE);
  if (secunda%2 == 1)
  display.fillCircle (SCREEN_HEIGHT-3-r/2,SCREEN_WIDTH-3-r/2,r+1,WHITE);
  display.display(); 
}

if (mz1 != mz0)
{
 // Write to OLED on bus number 4
  TCA9548A(4);
  display.clearDisplay();
  number(0, 0, minut/10, WHITE);
  display.display(); 
}

if (mu1 != mu0)
{
  // Write to OLED on bus number 5
  TCA9548A(5);
  display.clearDisplay();
  number(0, 0, minut%10, WHITE);
  display.display(); 
}

if (DST != DST0)
{
  timeClient.setTimeOffset(timezoneOffset + DST*3600);
  timeClient.begin();
DST0 = DST;
}

oz0 = oz1;
ou0 = ou1;
mz0 = mz1;
mu0 = mu1;
secunda0 = secunda;

secunda = secundar;
delay(50);

if (millis() - tpchange > tpscurs)
{
oz0 = 11;
ou0 = 11;
mz0 = 11;
mu0 = 11;
secunda0 = 61;
tpchange = millis();
}

// --------- display ring clock part ------------------------------------
    for (int i=0; i<NUM_LEDS; i++)  // clear all leds
      LEDs[i] = CRGB::Black;

    int inel_secunda = getLEDMinuteOrSecond(secundar);
    int inel_minut = getLEDMinuteOrSecond(minut);
    int inel_ora = getLEDHour(ora, minut);

    // Set "Hands"
    LEDs[inel_secunda] = colorSecond;
    LEDs[inel_minut] = colorMinute;  
    LEDs[inel_ora] = colorHour;  

    // Hour and min are on same spot
    if ( inel_ora == inel_minut)
      LEDs[inel_ora] = colorHourMinute;

    // Hour and sec are on same spot
    if ( inel_ora == inel_secunda)
      LEDs[inel_ora] = colorHourSecond;

    // Min and sec are on same spot
    if ( inel_minut == inel_secunda)
      LEDs[inel_minut] = colorMinuteSecond;

    // All are on same spot
    if ( inel_minut == inel_secunda && inel_minut == inel_ora)
      LEDs[inel_minut] = colorAll;

    if ( night() && USE_NIGHTCUTOFF == true )
      FastLED.setBrightness (NIGHTBRIGHTNESS); 
      else
      FastLED.setBrightness (DAYBRIGHTNESS); 
      
    FastLED.show();
// ---------end of display ring clock part ------------------------------------
} // end main loop

void number(uint8_t pos_x, uint8_t pos_y,
                  uint8_t digit, uint8_t color) {
  // loop through 7 segments
  for (uint8_t i = 0; i < 7; i++) {
    bool seg_on = digit_array[digit][i];
    // if seg_on is true draw segment
    if (seg_on) {
      switch (i) {
        case 0:
          display.fillRoundRect(n3 + pos_x, 0 + pos_y, n1, n3, r, color); // SEG a   // https://learn.adafruit.com/adafruit-gfx-graphics-library/graphics-primitives
          break;
        case 1:
          display.fillRoundRect(n1+n3 + pos_x, n3 + pos_y, n3, n2, r, color); // SEG b
          break;
        case 2:
          display.fillRoundRect(n1+n3 + pos_x, n2+2*n3 + pos_y, n3, n2, r, color); // SEG c
          break;
        case 3:
          display.fillRoundRect(n3 + pos_x, 2*n2+2*n3+ pos_y, n1, n3, r, color); // SEG d
          break;
        case 4:
          display.fillRoundRect(0 + pos_x, n2+2*n3 + pos_y, n3, n2, r, color); // SEG e
          break;
        case 5:
          display.fillRoundRect(0 + pos_x, n3 + pos_y, n3, n2, r, color); // SEG f
          break;
        case 6:
          display.fillRoundRect(n3 + pos_x, n2+n3 + pos_y, n1, n3, r, color); // SEG g
          break;
      }
      seg_on = false;
    }
  }
}

byte getLEDHour(byte orele, byte minutele) {
  if (orele > 12)
    orele = orele - 12;

  byte hourLED;
  if (orele <= 5) 
    hourLED = (orele * 5) + 30;
  else
    hourLED = (orele * 5) - 30;

  if (USE_LED_MOVE_BETWEEN_HOURS == true) {
    if        (minutele >= 12 && minutele < 24) {
      hourLED += 1;
    } else if (minutele >= 24 && minutele < 36) {
      hourLED += 2;
    } else if (minutele >= 36 && minutele < 48) {
      hourLED += 3;
    } else if (minutele >= 48) {
      hourLED += 4;
    }
  }

  return hourLED;  
}

byte getLEDMinuteOrSecond(byte minuteOrSecond) {
  if (minuteOrSecond < 30) 
    return minuteOrSecond + 30;
  else 
    return minuteOrSecond - 30;
}

boolean night() { 
  if (ora >= NIGHTCUTOFF || ora < MORNINGCUTOFF) 
    return true;  
    else
    return false;  
}
