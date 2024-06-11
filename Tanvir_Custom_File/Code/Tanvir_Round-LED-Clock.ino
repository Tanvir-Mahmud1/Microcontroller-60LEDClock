/*
  WiFi connected round LED Clock. It gets NTP time from the internet and translates to a 60 RGB WS2812B LED strip. 
  NTP and summer time code based on:
  https://tttapa.github.io/ESP8266/Chap15%20-%20NTP.html 
  https://github.com/SensorsIot/NTPtimeESP/blob/master/NTPtimeESP.cpp (for US summer time support check this link)
*/

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#define DEBUG_ON

const char ssid[] = "🌼 Aparajita 🌼";                // Your network SSID name here
const char pass[] = "b2nn@321";                        // Your network password here
unsigned long timeZone = 5.0;                          // Change this value to your local timezone (in my case +1 for Amsterdam)
const char* NTPServerName = "nl.pool.ntp.org";         // Change this to a ntpserver nearby, check this site for a list of servers: https://www.pool.ntp.org/en/
unsigned long intervalNTP = 24 * 60 * 60000;           // Request a new NTP time every 24 hours
unsigned long updateTimeNTPrequest = 4 * 60 * 60000;   // Request a new NTP time every ** hours

/*
Change the colors here if you want, reference: https://github.com/FastLED/FastLED/wiki/Pixel-reference#predefined-colors-list or https://www.rapidtables.com/web/color/RGB_Color.html
You can also set the colors with RGB values, for example (for red): CRGB(255, 0, 0) or CRGB::Red
*/

CRGB colorHour = CRGB(255, 0, 0);            //Red
CRGB colorMinute = CRGB(0, 255, 0);          //Green
CRGB colorSecond = CRGB(0, 80, 80);          //dark turquoise
CRGB colorHourMinute = CRGB(255, 255, 0);    //Yellow
CRGB colorHourSecond = CRGB(255, 0, 255);    //Magenta
CRGB colorMinuteSecond = CRGB(0, 0, 255);    //Blue
CRGB colorAll = CRGB(255, 255, 255);         //white.

// Set this to true if you want the hour LED to move between hours (if set to false the hour LED will only move every hour)
#define USE_LED_MOVE_BETWEEN_HOURS true

#define USE_NIGHTCUTOFF true  // Enable/Disable night brightness
#define MORNINGCUTOFF 6       // When does daybrightness begin?   6 am
#define NIGHTCUTOFF 22        // When does nightbrightness begin? 10 pm
#define NIGHTBRIGHTNESS 10    // Brightness level from 0 (off) to 255 (full brightness)

ESP8266WiFiMulti wifiMulti;
WiFiUDP UDP;
IPAddress timeServerIP;
const int NTP_PACKET_SIZE = 48;
byte NTPBuffer[NTP_PACKET_SIZE];

unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();
uint32_t timeUNIX = 0;
unsigned long prevActualTime = 0;

#define LEAP_YEAR(Y) (((1970 + Y) > 0) && !((1970 + Y) % 4) && (((1970 + Y) % 100) || !((1970 + Y) % 400)))
static const uint8_t monthDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

#define NUM_LEDS 60
#define DATA_PIN D5  //Check Data Pin where it is connected..............................
CRGB LEDs[NUM_LEDS];

struct DateTime {
  int year;
  byte month;
  byte day;
  byte hour;
  byte minute;
  byte second;
  byte dayofweek;
};

DateTime currentDateTime;

// ___________________________________________________________________________________________________
void setup() {

  FastLED.delay(3000);
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(LEDs, NUM_LEDS);

  Serial.begin(115200);
  delay(10);
  Serial.println("\r\n");

  startWiFi();
  startUDP();

  if (!WiFi.hostByName(NTPServerName, timeServerIP)) {
    Serial.println("DNS lookup failed. Rebooting.");
    Serial.flush();
    ESP.reset();
  }
  Serial.print("Time server IP:\t");
  Serial.println(timeServerIP);

  Serial.println("\r\nSending NTP request from void setup()...");
  sendNTPpacket(timeServerIP);

// Bellow code is Generaed by Tanvir. [NTP respond was not getting everytime it request, so bellow code will loop through it until it gets response.]
  uint32_t time = getTime();
  while (time == 0) {
    sendNTPpacket(timeServerIP);
    time = getTime();  // Check if an NTP response has arrived and get the (UNIX) time
    Serial.println("\r\nNo Response from NTP server. So, Sending NTP request again from void setup() but in loop...");
    delay(2000);
  } 
}


void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - prevNTP > intervalNTP) {  // If 24 hours has passed since last NTP request
    prevNTP = currentMillis;
    Serial.println("\r\nSending NTP request from void loop() after 'intervalNTP'...");
    sendNTPpacket(timeServerIP);  // Send an NTP request
  }

  uint32_t time = getTime();  // Check if an NTP response has arrived and get the (UNIX) time

  if (time) {                 // If a new timestamp has been received
    timeUNIX = time;
    Serial.print("\r\nNTP response:\t");
    Serial.println(timeUNIX);
    lastNTPResponse = currentMillis;
  } else if ((currentMillis - lastNTPResponse) > updateTimeNTPrequest) {
    Serial.println("\r\nMore than ** hour since last NTP response. Rebooting ...");
    Serial.flush();
    ESP.reset();
  }

  uint32_t actualTime = timeUNIX + (currentMillis - lastNTPResponse) / 1000;
  if (actualTime != prevActualTime && timeUNIX != 0) {  // If a second has passed since last update
    prevActualTime = actualTime;
    convertTime(actualTime);

    for (int i = 0; i < NUM_LEDS; i++)
      LEDs[i] = CRGB::Black;


    int second = getLEDMinuteOrSecond(currentDateTime.second);
    int minute = getLEDMinuteOrSecond(currentDateTime.minute);
    int hour = getLEDHour(currentDateTime.hour, currentDateTime.minute);

    // Set "Hands"
    LEDs[second] = colorSecond;
    LEDs[minute] = colorMinute;
    LEDs[hour] = colorHour;

    if (hour == minute)         //If Hour and min are on same spot.
      LEDs[hour] = colorHourMinute;

    if (hour == second)         //If Hour and sec are on same spot.
      LEDs[hour] = colorHourSecond;

    if (minute == second)       // Min and sec are on same spot.
      LEDs[minute] = colorMinuteSecond;

    if (minute == second && minute == hour) // Hour-Min-Sec are on same spot.
      LEDs[minute] = colorAll;

    set_night_brightness();
    FastLED.show();
  }
}
// ___________________________________________________________________________________________________


byte getLEDHour(byte hours, byte minutes) {
  if (hours > 12)   hours = hours - 12;
    
  // As RGB LED Starts from 0 but LED stripe starts from 1, so Bellow adjustments needed to work 60th 1st and 2nd LED. And Same for getLEDMinuteOrSecond(). 
  byte hourLED;
  if (hours <= 6) hourLED = (hours * 5) + 30;
  else hourLED = (hours * 5) - 30;

  if (USE_LED_MOVE_BETWEEN_HOURS == true) {
    if (minutes >= 12 && minutes < 24) hourLED += 1;
    else if (minutes >= 24 && minutes < 36) hourLED += 2;
    else if (minutes >= 36 && minutes < 48) hourLED += 3;
    else if (minutes >= 48) hourLED += 4;
  }

  if (hourLED > 60) hourLED -= 60;      // This line is needed to work (1-4)th LED to work when its 6:12 to 6:59.

  return hourLED - 1;   // for (int i = 0; i < NUM_LEDS; i++), So i need to make some adjustment here.
}

byte getLEDMinuteOrSecond(byte minuteOrSecond) {
  if (minuteOrSecond < 31)  return minuteOrSecond + 29;
  else  return minuteOrSecond - 31;
}

void startWiFi() {
  wifiMulti.addAP(ssid, pass);
  Serial.println("Connecting");

  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print('.');

    // Bellow code is for displaying random color while device is not connected to the internet/NTP Server. Customized by Tanvir.
    circular_led_show();
    // random_color_show();

    // LEDs[i++] = CRGB::Green;
    // FastLED.show();
  }
  
  Serial.println("\r\n");
  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("IP address:\t");
  Serial.print(WiFi.localIP());
  Serial.println("\r\n");
}

void startUDP() {
  Serial.println("Starting UDP");
  UDP.begin(123);  // Start listening for UDP messages on port 123
  Serial.print("Local port:\t");
  Serial.println(UDP.localPort());
  Serial.println();
}

uint32_t getTime() {
  if (UDP.parsePacket() == 0) {  // If there's no response (yet)
    return 0;
  }
  UDP.read(NTPBuffer, NTP_PACKET_SIZE);  // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
  // Convert NTP time to a UNIX timestamp:
  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
  const uint32_t seventyYears = 2208988800UL;
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  return UNIXTime;
}

void sendNTPpacket(IPAddress& address) {
  memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  NTPBuffer[0] = 0b11100011;  // LI, Version, Mode
  // send a packet requesting a timestamp:
  UDP.beginPacket(address, 123);  // NTP requests are to port 123
  UDP.write(NTPBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}

void convertTime(uint32_t time) {
  time += (3600 * timeZone);    // Correct time zone

  currentDateTime.second = time % 60;
  currentDateTime.minute = time / 60 % 60;
  currentDateTime.hour = time / 3600 % 24;
  time /= 60;  // To minutes
  time /= 60;  // To hours
  time /= 24;  // To days
  currentDateTime.dayofweek = ((time + 4) % 7) + 1;
  int year = 0;
  int days = 0;
  while ((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
    year++;
  }
  days -= LEAP_YEAR(year) ? 366 : 365;
  time -= days;  // To days in this year, starting at 0
  days = 0;
  byte month = 0;
  byte monthLength = 0;
  for (month = 0; month < 12; month++) {
    if (month == 1) {  // February
      if (LEAP_YEAR(year)) monthLength = 29;
      else monthLength = 28;

    } else monthLength = monthDays[month];

    if (time >= monthLength) time -= monthLength;
    else break;
  }

  currentDateTime.day = time + 1;
  currentDateTime.year = year + 1970;
  currentDateTime.month = month + 1;

  if (summerTime()) currentDateTime.hour += 1;        // Correct European Summer time

#ifdef DEBUG_ON
  Serial.print("Time: ");
  Serial.print(currentDateTime.hour);
  Serial.print(":");
  Serial.print(currentDateTime.minute);
  Serial.print(":");
  Serial.print(currentDateTime.second);

  Serial.print("; Date: ");
  Serial.print(currentDateTime.day);
  Serial.print("/");
  Serial.print(currentDateTime.month);
  Serial.print("/");
  Serial.print(currentDateTime.year);
  Serial.print("; Day_of_week: ");
  
  Serial.print(getDayName(currentDateTime.dayofweek+1));  // getDayName() function will convert number to name which takes 1 argument "currentDateTime.dayofweek+1"
  // Serial.print(currentDateTime.dayofweek+1);

  Serial.print("; Summer_time: ");
  Serial.print(summerTime());
  Serial.print("; Night_time: ");
  Serial.print(night());

  // Serial.print("; Second indicating LED: ");
  // Serial.print(getLEDMinuteOrSecond(currentDateTime.second););    // These lines are for Checking the SecondLED's LED No., just for Debugging.

  Serial.println();
#endif
}


boolean summerTime() {
  if (currentDateTime.month < 3 || currentDateTime.month > 10) return false;  // No summer time in Jan, Feb, Nov, Dec
  if (currentDateTime.month > 3 && currentDateTime.month < 10) return true;   // Summer time in Apr, May, Jun, Jul, Aug, Sep
  if (currentDateTime.month == 3 && (currentDateTime.hour + 24 * currentDateTime.day) >= (3 + 24 * (31 - (5 * currentDateTime.year / 4 + 4) % 7)) || currentDateTime.month == 10 && (currentDateTime.hour + 24 * currentDateTime.day) < (3 + 24 * (31 - (5 * currentDateTime.year / 4 + 1) % 7)))
    return true;
  else  return false;
}


// Tanvir have modified bellow code as it was throwing error, [check original file to see what was changed]
boolean night() {
  if (currentDateTime.hour >= NIGHTCUTOFF || currentDateTime.hour < MORNINGCUTOFF) return true;   //  Tanvir modified above code from logical AND to Logical OR.
  else  return false;
}


//__________________________________________________________________________________________________________________
//  Code generated by Tanvir.

void set_night_brightness() {
  if (night() && USE_NIGHTCUTOFF == true)
    FastLED.setBrightness(NIGHTBRIGHTNESS);
  else FastLED.setBrightness(255);
}


String getDayName(int day) {
  switch(day) {
    case 1: return "Saturday";
    case 2: return "Sunday";
    case 3: return "Monday";
    case 4: return "Tuesday";
    case 5: return "Wednesday";
    case 6: return "Thursday";
    case 7: return "Friday";
    default:  return "Invalid day";
  }
}


void circular_led_show() {
  for (int i = 0; i < NUM_LEDS; i++) {
    uint8_t hue = (millis() / 10 - (i * 255 / (NUM_LEDS - 2))) % 255;     // Here 10 is for speed of LED circle and "-" is for clockwise rotation. and if i use (NUM_LEDS - 2) it looks more smoother to me.
    LEDs[i] = CHSV(hue, 255, 255);
    // Serial.printf("R: %d, G: %d, B: %d\n", LEDs[i].r, LEDs[i].g, LEDs[i].b);    // This line is used for seing the RGB values in the serial monitor.
    set_night_brightness();
    FastLED.show();
  }
}

void random_color_show() {
  for (int i = 0; i < NUM_LEDS; i++) {    // Fill the LED array with random colors.
    CRGB newColor;   // Here "newColor" is a CRGB variable.
    do newColor = CRGB(random(256), random(256), random(256));
      while (i > 0 && abs(newColor.r - LEDs[i-1].r) < 50 && abs(newColor.g - LEDs[i-1].g) < 50 && abs(newColor.b - LEDs[i-1].b) < 50);
    
    LEDs[i] = newColor;
    // Serial.printf("R: %d, G: %d, B: %d\n", LEDs[i].r, LEDs[i].g, LEDs[i].b);    // This line is used for seing the RGB values in the serial monitor.
    set_night_brightness();
    FastLED.show();   // Display the updated LED array, This line must be here, if we use it outside for loop it only changes 1st LED.
  }
}


