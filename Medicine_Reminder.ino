// simplestesp8266clock.ino
//
// Libraries needed:
//  Time.h & TimeLib.h:  https://github.com/PaulStoffregen/Time
//  Timezone.h: https://github.com/JChristensen/Timezone
//  SSD1306.h & SSD1306Wire.h:  https://github.com/squix78/esp8266-oled-ssd1306
//  NTPClient.h: https://github.com/arduino-libraries/NTPClient
//  ESP8266WiFi.h & WifiUDP.h: https://github.com/ekstrand/ESP8266wifi
// 
// Code from: http://www.instructables.com/id/TESTED-Timekeeping-on-ESP8266-Arduino-Uno-WITHOUT-/
//         by Ruben Marc Speybrouck
//
// Hacked for this UK-based project by Craig Marston
//
// 128x64 OLED pinout:
// GND goes to ground
// Vin goes to 3.3V
// Data to I2C SDA (GPIO 0)
// Clk to I2C SCL (GPIO 2)

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)

// IMPORTANT: To reduce NeoPixel burnout risk, add 1000 uF capacitor across
// pixel power leads, add 300 - 500 Ohm resistor on first pixel's data input
// and minimize distance between Arduino and first pixel. Avoid connecting
// on a live circuit... if you must, connect GND first.


#include <ESP8266WiFi.h>
#include <WifiUDP.h>
#include <String.h>
#include <Wire.h>
#include <SSD1306.h>
#include <SSD1306Wire.h>
#include <NTPClient.h>
#include <Time.h>
#include <TimeLib.h>
#include <Timezone.h>
#include <Adafruit_NeoPixel.h>
//#include <NeoPixelPainter.h>


#define PIXEL_PIN    5    // D1 Digital IO pin connected to the NeoPixels.
#define PIXEL_COUNT 12    // *** How many Neopixels? ***


// Parameter 1 = number of pixels in strip,  neopixel stick has 8
// Parameter 2 = pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_RGB     Pixels are wired for RGB bitstream
//   NEO_GRB     Pixels are wired for GRB bitstream, correct for neopixel stick
//   NEO_KHZ800  800 KHz bitstream (e.g. High Density LED strip), correct for neopixel stick
Adafruit_NeoPixel strip = Adafruit_NeoPixel(PIXEL_COUNT, PIXEL_PIN, NEO_GRB + NEO_KHZ800);

//NeoPixelPainterCanvas pixelcanvas = NeoPixelPainterCanvas(&strip); //create canvas, linked to the neopixels (must be created before the brush)
//NeoPixelPainterBrush pixelbrush = NeoPixelPainterBrush(&pixelcanvas); //crete brush, linked to the canvas to paint to


// Define NTP properties
#define NTP_OFFSET   0 * 60      // In seconds
#define NTP_INTERVAL 60 * 1000    // In miliseconds
#define NTP_ADDRESS  "uk.pool.ntp.org"  // 

/*
     server 0.uk.pool.ntp.org
     server 1.uk.pool.ntp.org
     server 2.uk.pool.ntp.org
     server 3.uk.pool.ntp.org
 */

// Set up the NTP UDP client
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, NTP_ADDRESS, NTP_OFFSET, NTP_INTERVAL);

// Create a display object
SSD1306  display(0x3d, 0, 2); //0x3d for the Adafruit 1.3" OLED, 0x3C being the usual address of the OLED


// -----------------------------------------------------------------------------------------------------------
// *** ---------------------------------------- own settings -------------------------------------------------

// === Set Your WiFI credentials ===

const char* ssid = "Area51_MQTT_Pi";                     // *** insert your own ssid ***
const char* password = "Bennett_Built_a_Time_Machine";   // *** and password ***

const byte firstAlarmHour = 7;                           // *** in 24 hour format, !!! NO LEADING ZEROES !!! ***
const byte firstAlarmMinute = 30;                        // *** minutes ***

const byte durationBetweenAlarms = 10;                   // *** set the time in MINUTES between alarm stages

const byte resetHour = 3;                                // *** time that the system resets - 3am
const byte resetMinute = 0;                             // ***   --- " ---



// *** --------------------------------------------------------------------------------------------------------
// ------------------------------------------------------------------------------------------------------------


unsigned long alarmGap; //  converts minutes duration to milliseconds
unsigned long alarmGap2 = 2 * alarmGap;

String date;
String t;
const char * days[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"} ;
const char * months[] = {"Jan", "Feb", "Mar", "Apr", "May", "June", "July", "Aug", "Sep", "Oct", "Nov", "Dec"} ;
const char * ampm[] = {"AM", "PM"} ;

const byte administeredButtonPin = 13;                  // D7 — N.O. momentary button connects to ground
bool administeredFlag = false;                          // variable 'flag' for noting administration of meds
bool alarmCondition = false;

unsigned long startTime;                                // how long since the button was first pressed 
unsigned long previousTime;
byte startFlag = 0;
bool firstTime = true;

unsigned long pressTime;                                // used to check how long button was held for
unsigned long alarmTriggeredTime;                       // capture the time the alarm was triggered in millis
unsigned long nextAlarm;
unsigned long finalAlarm;

time_t t_1stAlarm;

byte x;
byte spinCounter;

 
void setup () 
{
  Serial.begin(115200); // 
  timeClient.begin();                                   // Start the NTP UDP client

  Wire.pins(0, 2);                                      // Start the OLED with GPIO 0 and 2 (pins D3 and D4)
  Wire.begin(0, 2);                                     // 0=sda (D3), 2=scl (D4)
  display.init();
  display.flipScreenVertically();   

  pinMode(administeredButtonPin, INPUT_PULLUP);         // Meds administered button to my WEMOS GPIO 14 (pin D5)
  
  strip.begin();
  strip.show();                                         // Initialize all pixels to 'off'

  // Connect to wifi
  Serial.println("");
  Serial.print("Connecting to ");
  Serial.print(ssid);
  display.drawString(0, 10, "Connecting to Wifi...");
  display.display();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) 
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to WiFi at ");
  Serial.print(WiFi.localIP());
  Serial.println("");
  display.drawString(0, 24, "Connected.");
  display.display();
  delay(1000);
 
}

// --------------------∆   End of Setup     ∆-------------------


// Declaration of the functions used
void AmberAlert();
void ClearLEDs();
void RedAlert();
void Warning();
void acknowledge();
void confirm();
void RedBrighten();                                     // the 'brighten' function
void RedDarken();                                       // the 'darken' function
void RedFlash();
void OEbrighten();
void OEdarken();
void ClockDisplayUpdate();



// ---------------------V- Main Routine -V-----------------
void loop() 
{  
  if (WiFi.status() == WL_CONNECTED)                    //Check WiFi connection status
  {   
    date = "";                                          // clear the variables
    t = "";
    
    // update the NTP client and get the UNIX UTC timestamp 
    timeClient.update();
    unsigned long epochTime =  timeClient.getEpochTime();

    // convert received time stamp to time_t object
    time_t local, utc;
    utc = epochTime;
    
    // Then convert the UTC UNIX timestamp to local time
    // {abbrev, week [first/second/third/fourth/last], DoW [Sun, …, Sat], Month [Jan, …, Dec], hour, offset [mins]}
    // hour is that at which the change occurs, offset is the amount of change (some nations have 30 min offsets)
    TimeChangeRule GMT = {"GMT", Last, Sun, Oct, 2, 0};  //GMT ~ UTC - Greenwich Mean Time jumps back to 1 am
    TimeChangeRule BST = {"BST", Last, Sun, Mar, 1, 60};   //UTC+1 ~ GMT+1 British Summer Time jumps fwd to 2am
    Timezone UK(GMT, BST);
    local = UK.toLocal(utc);
  
    // now format the Time variables into strings with proper names for month, day etc
    date += days[weekday(local)-1];
    date += ", ";
    date += months[month(local)-1];
    date += " ";
    date += day(local);
    date += ", ";
    date += year(local);

    // format the time to 12-hour format with AM/PM and no seconds
    t += hourFormat12(local);
    t += ":";
    if(minute(local) < 10)  // add a zero if minute is under 10 - curly braces not necessary 
    t += "0";
    t += minute(local);
    t += " ";
    t += ampm[isPM(local)];

    if(second() % 10 == 0)  // so that the update is only every 10 seconds
    {
      // Display the date and time
      Serial.println("");
      Serial.print("Local date: ");
      Serial.print(date);
      Serial.println("");
      Serial.print("Local time: ");
      Serial.print(t);
    }

// =========================  Everything above works — Don't bugger about with it!  ======================


// =========================================== Display Update every 10 s ==========================================

    if (second() % 10 == 0)
    { // so that the update is only every 10 seconds — source of 'seconds' irrelevant
      ClockDisplayUpdate();
    }


 // ==================== Button Algorithm ===========================

    
    
    if ( digitalRead(administeredButtonPin) == LOW )               // button is pressed 
    {
      ButtonPressedDuration();                                     // this is the function to check how long the button pressed for
    }

    if ( hour(local) == resetHour && minute(local) >= resetMinute ) // meant to reset the system
    {
      administeredFlag = false;
      return;//  
    }


    
// ============================================ Alert Sequences =============================================

    
    if ( hour(local) == firstAlarmHour  &&  minute(local) >= firstAlarmMinute ) //  FIRST ALARM!
    { 
      // alarmGap is in milliseconds
      alarmTriggeredTime = millis();
      alarmGap = (60000 * durationBetweenAlarms); //  converts minutes duration to milliseconds 60000 * 1 = 60 seconds


      while ( administeredFlag == false && ( millis() - alarmTriggeredTime ) < alarmGap ) 
      {
        alarmCondition = true; // change flag, 
        AmberAlert(); // early reminder
      }

      
      while ( ( millis() - alarmTriggeredTime ) >= alarmGap && alarmCondition == true && administeredFlag == false && ( millis() - alarmTriggeredTime ) < ( 2 * alarmGap ) ) 
      {
        RedAlert(); // 
      }

      
      while ( ( millis() - alarmTriggeredTime ) >= alarmGap2 && alarmCondition == true && administeredFlag == false ) 
      {
        Warning();//  
      }    
    }
        
    else
    {
      alarmCondition = false;
      return;
    }     
  }
  
  else // attempt to connect to wifi again if disconnected
  {
    display.clear();
    display.drawString(0, 10, "Connecting to Wifi...");
    display.display();
    WiFi.begin(ssid, password);
    display.drawString(0, 24, "Connected.");
    display.display();
    delay(1000);
  }
  
}
// ----------------------------------------∆ End of Main Loop ∆------------------------------------------------


// This function checks how long the button was pushed for and works out what to do!
void ButtonPressedDuration()
{
  while ( digitalRead(administeredButtonPin) == LOW )               // button is pressed
  {
    delay(20); // de-bounce!! **** need more !!! ============================= **** ===================== ***
    
    if ( firstTime == true )               // fresh start
    {
      startTime = millis();             // capture the time-stamp NOW
      firstTime = false;                    // button has been pressed, change flag
    }
    pressTime = millis() - startTime;   // difference between NOW and when first pressed as loops pass
                                        // millis() continues to increase
    
    if (pressTime >= 25 &&  digitalRead(administeredButtonPin) == LOW && administeredFlag == false )               // if button held for 25 ms — debounce
    {
      acknowledge(); 
    }
  
    if (pressTime>3000 && digitalRead(administeredButtonPin) == LOW && administeredFlag == false)
    {              // if button held for 3 s
      confirm();
      administeredFlag = true;
      firstTime = true;                    // reset flag 
      return;
    }
    
    if ( digitalRead(administeredButtonPin) == HIGH && administeredFlag == false)           // button released so break
    {
      delay(20); //de-bounce
      buttonFail();
      firstTime = true;
      return;
    }   
  }
  

  if(firstTime == false && administeredFlag == true) 
  {
    alreadyDone(); 
    firstTime = true; 
  }
}


void ClockDisplayUpdate() // print the date and time on the OLED
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_24);
  display.drawStringMaxWidth(64, 10, 128, t);
  display.setFont(ArialMT_Plain_10);
  display.drawStringMaxWidth(64, 38, 128, date);
  display.display();
}

// --------------------------------V- Colour Alert Functions -V-------------------------

void AmberAlert() 
{   // 
  OEbrighten();   // the 'brighten' function
  OEdarken();     // the 'darken' function
  ClockDisplayUpdate();
  if ( digitalRead(administeredButtonPin) == LOW )               // button is pressed 
    {
      ButtonPressedDuration();     // this is the function to check how long the button pressed for
    }
  return; 
}


void RedAlert() 
{   // 
  RedBrighten();   // the 'brighten' function
  RedDarken();     // the 'darken' function
  ClockDisplayUpdate();
  if ( digitalRead(administeredButtonPin) == LOW )               // button is pressed 
    {
      ButtonPressedDuration();     // this is the function to check how long the button pressed for
    }
  return; 
}


void Warning()
{   //  Warning with audible alert 
  RedFlash();
  ClockDisplayUpdate();
  if ( digitalRead(administeredButtonPin) == LOW )               // button is pressed 
    {
      ButtonPressedDuration();     // this is the function to check how long the button pressed for
    }
  return; 
  //tone(pin, frequency, duration)
}


void OEbrighten() 
{ 
  uint16_t i, j;
  
  for (j = 5; j < 192; j++) {
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, j, j/2, 0);
    }
    strip.show();
    delay(5);
  }
  delay(50);
} // 


void OEdarken() 
{ 
  uint16_t i, j;
  
  for (j = 192; j > 5; j--) {
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, j, j/2, 0);
    }
    strip.show();
    delay(5);
  }
  delay(200);
}


void RedBrighten() 
{ 
  uint16_t i, j;

  for (j = 25; j < 175; j++) {
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, j, 0, 0);
    }
    strip.show();
    delay(5);
  }
  delay(20);
} // 


void RedDarken() 
{ // ***** *** *** Max to 0 darken *** *** *****
  uint16_t i, j;

  for (j = 175; j > 25; j--) {
    for (i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, j, 0, 0);
    }
    strip.show();
    delay(5);
  }
  delay(100);
}


void RedFlash() 
{ 
  uint16_t i;  
  uint32_t low = strip.Color(0, 0, 0); 
  uint32_t high = strip.Color(225, 0, 0);
  
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, high);
      strip.show();
  }  
   
  delay(333);
    
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, low);
      strip.show();
  }
  
  delay(333);
  
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, high);
      strip.show();
  }   
   
  delay(333);
    
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, low);
      strip.show();
  } 
  delay(333);
} 


void acknowledge() // shows that the button push has been noticed
{ 
  uint16_t i;  
  uint32_t low = strip.Color(0, 0, 0); 
  uint32_t high = strip.Color(0, 15, 5);
  
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, high);
      strip.show();
      if ( digitalRead(administeredButtonPin) == HIGH )
      {
        break;
      }
  }  
  
  delay(333);
    
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, low);
      strip.show();
      if ( digitalRead(administeredButtonPin) == HIGH )
      {
        break;
      }
  } 
  delay (100);  
} 


void confirm() // flashes bright green to confirm administration of meds
{ 
  uint16_t i;  
  uint32_t low = strip.Color(0, 0, 0); 
  uint32_t high = strip.Color(0, 75, 0);
  
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, high);
      strip.show();
  }  

  delay(333);
    
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, low);
      strip.show();
  } 

  delay(250);
  
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, high);
      strip.show();
  }  

  delay(1500);

  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, low);
      strip.show();
  } 
return;
}  


void buttonFail() // to show button not held long enough
{ 
  uint16_t i;  
  uint32_t low = strip.Color(0, 0, 0); 
  uint32_t high = strip.Color(75, 10, 0);
  
  for( int i = 0; i<strip.numPixels(); i++) // clear it all first!
  {
      strip.setPixelColor(i, low);
      strip.show();
  } 
  
  for( int i = 0; i<strip.numPixels(); i+=2)
  {
      strip.setPixelColor(i, high);
      strip.show();
  }  
   
  delay(100);
    
  for( int i = 0; i<strip.numPixels(); i+=2)
  {
      strip.setPixelColor(i, low);
      strip.show();
  }
  
  delay(333);
  
  for( int i = 0; i<strip.numPixels(); i+=2)
  {
      strip.setPixelColor(i, high);
      strip.show();
  }   
   
  delay(100);
    
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, low);
      strip.show();
  } 
}  


void alreadyDone()               // * temporarily a simple flash — ought to be a nice chaser!
{ 
  uint16_t i;  
  uint32_t low = strip.Color(0, 0, 0); 
  uint32_t high = strip.Color(50, 10, 50);
  
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, high);
      strip.show();
  }  

  delay(333);
    
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, low);
      strip.show();
  } 

  delay(250);
  
  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, high);
      strip.show();
  }  

  delay(333);

  for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, low);
      strip.show();
  } 

} // 


void ClearLEDs()
{
  uint16_t i;  
  uint32_t low = strip.Color(0, 0, 0); 

    for( int i = 0; i<strip.numPixels(); i++)
  {
      strip.setPixelColor(i, low);
      strip.show();
  }
}


