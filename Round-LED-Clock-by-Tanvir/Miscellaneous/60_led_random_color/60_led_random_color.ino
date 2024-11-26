#include <FastLED.h>

#define LED_PIN     D5 // Pin connected to the LED strip
#define LED_COUNT   60 // Number of LED pixels
#define LEDS_PER_LINE 6 // Number of LEDs to print per line

#define BRIGHTNESS  255 // Brightness (0-255)

CRGB leds[LED_COUNT];

void setup() {
  Serial.begin(9600); // Initialize serial communication
  while (!Serial);    // Wait for Serial Monitor to open
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  // Fill the LED array with random colors
  for (int i = 0; i < LED_COUNT; i++) {
    if (i == 0) {
      leds[i] = randomColor();
    } else {
      CRGB newColor = randomColor();
      while (abs(newColor.r - leds[i-1].r) < 50 && abs(newColor.g - leds[i-1].g) < 50 && abs(newColor.b - leds[i-1].b) < 50) {
        newColor = randomColor();
      }
      leds[i] = newColor;
    }
  }

  // Display the updated LED array
  FastLED.show();

  // Print RGB values to serial monitor
  for (int i = 0; i < LED_COUNT; i++) {
    Serial.print("LED-");
    Serial.print(i);
    Serial.print(": RGB(");
    Serial.print(leds[i].r);
    Serial.print(",");
    Serial.print(leds[i].g);
    Serial.print(",");
    Serial.print(leds[i].b);
    Serial.print(") ");
    
    // Start new line after printing 6 RGB values
    if ((i + 1) % LEDS_PER_LINE == 0) {
      Serial.println();
    }
  }

  Serial.println("_______________________________________________________________________________________________________________________________________________________"); // Print a new line for better readability

  // Wait for 10 seconds
  delay(1*2*1000);   // min*sec*milliSecond
}

// Function to generate a random CRGB color
CRGB randomColor() {
  return CRGB(random(256), random(256), random(256));
}
