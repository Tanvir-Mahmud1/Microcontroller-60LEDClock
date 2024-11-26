/*
  WiFi connected round LED Clock. It gets NTP time from the internet and translates to a 60 RGB WS2812B LED strip. 
  NTP and summer time code based on:
  https://tttapa.github.io/ESP8266/Chap15%20-%20NTP.html 
  https://github.com/SensorsIot/NTPtimeESP/blob/master/NTPtimeESP.cpp (for US summer time support check this link)
*/

#include <WiFi.h>
#include <WiFiMulti.h>
#include <WiFiUdp.h>
#include <FastLED.h>
#define DEBUG_ON

const char ssid[] = "🌼 Aparajita 🌼";                // Your network SSID name here
const char pass[] = "b2nn@321";                        // Your network password here
unsigned long timeZone = 6.0;                          // Change this value to your local timezone (in my case +6 for Dhaka.)
const char* NTPServerName = "0.pool.ntp.org";          // Changed from "nl.pool.ntp.org" to "0.pool.ntp.org". Check this site for a list of servers: https://www.pool.ntp.org/en/
unsigned long intervalNTP = 24 * 60 * 60000;           // Request a new NTP time every 24 hours
unsigned long updateTimeNTPrequest = 6 * 60 * 60000;   // Request a new NTP time every ** hours

/*
Change the colors here if you want, reference: https://github.com/FastLED/FastLED/wiki/Pixel-reference#predefined-colors-list or https://www.rapidtables.com/web/color/RGB_Color.html
You can also set the colors with RGB values, for example (for red): CRGB(255, 0, 0) or CRGB::Red
*/

CRGB colorHour = CRGB(255, 0, 0);            //Red
CRGB colorMinute = CRGB(0, 255, 0);          //Green
CRGB colorSecond = CRGB(0, 0, 30);           //dim Blue
CRGB colorHourMinute = CRGB(255, 255, 0);    //Yellow
CRGB colorHourSecond = CRGB(255, 0, 255);    //Magenta
CRGB colorMinuteSecond = CRGB(0, 255, 255);  //Cyan
CRGB colorAll = CRGB(255, 255, 255);         //white.

// Set this to true if you want the hour LED to move between hours (if set to false the hour LED will only move every hour)
#define USE_LED_MOVE_BETWEEN_HOURS false

#define USE_NIGHTCUTOFF true  // Enable/Disable night brightness
#define MORNINGCUTOFF 6       // When does daybrightness begin?   6 am
#define NIGHTCUTOFF 23        // When does nightbrightness begin? 11 pm
#define NIGHTBRIGHTNESS 10    // Brightness level from 0 (off) to 255 (full brightness)

WiFiMulti wifiMulti;
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
#define DATA_PIN 18  //Check Data Pin where it is connected...............................
CRGB LEDs[NUM_LEDS];

// Below code is for Hour Notification sound, Developed by Tanvir.------------------------(1)
// ESP32 boards like the Wemos D1 Mini ESP32 do not support SoftwareSerial in the same way as the ESP8266 or Arduino Uno.
// Instead of using SoftwareSerial, you can use the hardware UART ports available on the ESP32.
// ESP32 boards typically have multiple UART ports (Serial, Serial1, Serial2, etc.). 
// using Serial2 (UART2) for communication with the DFPlayer Mini:
// #include <SoftwareSerial.h>    No need this line.
#include <DFRobotDFPlayerMini.h>

// SoftwareSerial mySoftwareSerial(16, 17); // RX, TX respectively, [connect RX pin to dfPlayer's TX pin and TX pin to dfPlayer's RX pin]
HardwareSerial hard_Serial(2); // Use Serial2 (UART2)
DFRobotDFPlayerMini myDFPlayer;

#define UART_RX_PIN 16       //Check Data Pin where it is connected...............................
#define UART_TX_PIN 17

int set_min_for_alrm = 0;  // This line is for demo purpose, we can remove this line and set the value diretly in playAlarm() function.
// ---------------------------------------------------------------------------------------(1)


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

  // setupDFPlayer_soft();    // Initialize DFPlayer Mini------------------------------(2)
  setupDFPlayer_hard();


  startWiFi();
  startUDP();

  if (!WiFi.hostByName(NTPServerName, timeServerIP)) {
    Serial.println("Can't connect to internet. After a delay it will Reboot the system.");
    circular_led_show();      // Will show circular led while no internet connectivity.
    delay(300000);            // Delay for ** seconds befor reseting the board.
    Serial.println("DNS lookup failed. Rebooting...");
    Serial.flush();
    // ESP.reset();
    ESP.restart();
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
    circular_led_show();    // Will show circular led while not connected.
    delay(2000);
  } 
}

void loop() {
  playAlarm();  //   -----------------------------------------------------------------------(3)

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
    ESP.restart();
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
// __________________________________________________________________________________________________________________________

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
  Serial.println("Connecting to Wifi...");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print('.');
    // Bellow code is for displaying random color while device is not connected to the internet/NTP Server. Customized by Tanvir.
    circular_led_show();
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
  Serial.println(123);
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
  time += (3600 * timeZone + 3);     // Here Time adjusted with time zone. [3600 sec x timezone(* Hour)] added to time. And * additional sec for accuracy.
                                     // As NTP response time is * sec slow with "https://time.is/UTC" and ohter servers so added * addition sec here.
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


#ifdef DEBUG_ON
  Serial.print("Time: ");
  if (currentDateTime.hour >= 24) Serial.print(00);
  else Serial.print(currentDateTime.hour);
  Serial.print(":");
  Serial.print(currentDateTime.minute);
  Serial.print(":");
  Serial.print(currentDateTime.second);

  Serial.print("; Date: ");
  if (currentDateTime.hour >= 24) Serial.print(currentDateTime.day + 1);
  else Serial.print(currentDateTime.day);
  Serial.print("/");
  Serial.print(currentDateTime.month);
  Serial.print("/");
  Serial.print(currentDateTime.year);
  
  Serial.print("; Day_of_week: ");
  if (currentDateTime.hour >= 24) Serial.print(getDayName(currentDateTime.dayofweek + 1));
  else Serial.print(getDayName(currentDateTime.dayofweek));  // getDayName() function will convert number to name which takes 1 argument "currentDateTime.dayofweek+1"


  Serial.print("; [Night_time: ");
  Serial.print(night());
  Serial.print("]");
  // Serial.print("; Second indicating LED: ");
  // Serial.print(getLEDMinuteOrSecond(currentDateTime.second););    // These lines are for Checking the SecondLED's LED No., just for Debugging.
  Serial.println();
#endif
}



// Tanvir have modified bellow code as it was throwing error, [check original file to see what was changed]
boolean night() {
  if (currentDateTime.hour >= NIGHTCUTOFF || currentDateTime.hour < MORNINGCUTOFF) return true;   //  Tanvir modified above code from logical AND to Logical OR.
  else  return false;
}

//__________________________________________________________________________________________________________________
//  Code generated by Tanvir.

void set_night_brightness() {
  if (currentDateTime.hour == 0)  FastLED.setBrightness(100);    // This line is used when system can't connect to wifi and returns hour = 0.
  else if (night() && USE_NIGHTCUTOFF == true)
    FastLED.setBrightness(NIGHTBRIGHTNESS);
  else FastLED.setBrightness(255);
}

String getDayName(int daysNumber) {
  switch(daysNumber) {
    case 1: return "Sunday";
    case 2: return "Monday";
    case 3: return "Tuesday";
    case 4: return "Wednesday";
    case 5: return "Thursday";
    case 6: return "Friday";
    case 7: return "Saturday";
    case 8: return "Sunday";    // This line is for making code simple, when its >=24 then i'm adding 1 to daysNumber which will make it 8, so i made 8 to sunday again to solve that problem easily.
    default: return "Invalid day";
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

// Below code is for Hour Notification sound, Developed by Tanvir.-------------------------------------(4)
/*
void setupDFPlayer_soft()  { 
  // This is software serial communication setup. [Check the software and hardware serial communication difference online/chatGPT]
  mySoftwareSerial.begin(9600);       // Begin software serial communication, to communicate with dfPlayer Mini.
  Serial.println("Initializing DFPlayer...");
  delay(100); // Add a delay to allow DFPlayer Mini to power up

  if (!myDFPlayer.begin(mySoftwareSerial))
    Serial.println(F("Unable to begin:: Please recheck the connection/ insert the SD card!"));

  else if (myDFPlayer.begin(mySoftwareSerial))  {
    Serial.println(F("DFPlayer Mini online."));
    myDFPlayer.volume(30); // Set volume value (0~30)
    myDFPlayer.play(1);    // Will Play initialization sound.
  }
}

*/

void setupDFPlayer_hard() {
  // This is hardware serial communication setup. [Check the software and hardware serial communication difference online/chatGPT]
  hard_Serial.begin(9600, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN); // Start UART2 (Serial2) on GPIO16 (RX) and GPIO17 (TX)
  Serial.println("Initializing DFPlayer...");
  delay(100); // Add a delay to allow DFPlayer Mini to power up
  if (!myDFPlayer.begin(hard_Serial))
    // Pass the *** object to DFPlayer's begin() function
    Serial.println(F("Unable to begin:: Please recheck the connection/ insert the SD card!"));
  else if (myDFPlayer.begin(hard_Serial))  {
    Serial.println(F("DFPlayer Mini online."));
    myDFPlayer.volume(20); // Set volume value (0~30)
    myDFPlayer.play(1);    // Will Play initialization sound.
  }
}



void playAlarm()
{
    static unsigned long alarmInterval = 3000;       // Interval of ** second for each alarm sound
    static unsigned long alarmPreMillis = 0;         // Previous time when the alarm was played

    unsigned long alarmCurrentMillis = millis();     // Current time

    static byte alarmCount = 0;                      // Count of alarms played in the current hour
    static bool alarmCompleted = false;              // Flag to indicate if all alarms for the hour have been completed

    // Check if the condition to play the alarm is met
    if ((currentDateTime.minute == set_min_for_alrm) && night() && !alarmCompleted)
    {
        int alrm_hour = currentDateTime.hour;

        if (alrm_hour == 0) alrm_hour = 12; // Handle midnight (0:00) as 12 AM
        else if (alrm_hour > 12) alrm_hour = alrm_hour - 12; // Convert 13-23 to 1-11 PM

        if (alarmCurrentMillis - alarmPreMillis >= alarmInterval)
        {
            // Play the alarm sound
            Serial.print("Playing alarm ");
            Serial.println(alarmCount + 1);
            myDFPlayer.play(1); // Adjust the file number according to your MP3 file on the SD card

            alarmCount++;
            alarmPreMillis = alarmCurrentMillis; // Update the previousMillis for the next iteration

            // Check if all alarms for the current hour have been played
            if (alarmCount >= alrm_hour)
            {
                alarmCompleted = true; // Mark the alarm as completed for this hour
                Serial.println("All alarms played for this hour.");
            }
        }
    }
    else
    {
        // Reset the alarm for the next hour
        if (alarmCompleted && (currentDateTime.minute != set_min_for_alrm))
        {
            // Reset for the next hour
            alarmCompleted = false;
            alarmCount = 0; // Reset alarm count for the next hour
            Serial.println("Ready for the next hour.");
        }
    }
}

