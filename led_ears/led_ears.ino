#include <Adafruit_NeoPixel.h>

// Configuration
#define NUM_LEDS     160
#define DATA_PIN_1   0
#define DATA_PIN_2   1
#define BRIGHTNESS   10
#define CHASE_DELAY  50  // milliseconds between steps

// Create two NeoPixel objects for each LED strip
Adafruit_NeoPixel strip1(NUM_LEDS, DATA_PIN_1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUM_LEDS, DATA_PIN_2, NEO_GRB + NEO_KHZ800);

void setup() {
  // Initialize both strips
  strip1.begin();
  strip2.begin();

  strip1.setBrightness(BRIGHTNESS);
  strip2.setBrightness(BRIGHTNESS);

  strip1.show(); // Initialize all pixels to 'off'
  strip2.show();
}

void loop() {
  // Run the chase pattern
  colorChase(strip1, strip2, strip1.Color(255, 0, 0));  // Red
  colorChase(strip1, strip2, strip1.Color(0, 255, 0));  // Green
  colorChase(strip1, strip2, strip1.Color(0, 0, 255));  // Blue
  colorChase(strip1, strip2, strip1.Color(255, 255, 0)); // Yellow
  colorChase(strip1, strip2, strip1.Color(255, 0, 255)); // Magenta
  colorChase(strip1, strip2, strip1.Color(0, 255, 255)); // Cyan
}

// Function to perform color chase
void colorChase(Adafruit_NeoPixel& s1, Adafruit_NeoPixel& s2, uint32_t color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    s1.setPixelColor(i, color);
    s2.setPixelColor(i, color);
    
    if (i > 0) {
      s1.setPixelColor(i - 1, 0); // turn off previous
      s2.setPixelColor(i - 1, 0);
    }

    s1.show();
    s2.show();
    delay(CHASE_DELAY);
  }

  // Turn off last LED
  s1.setPixelColor(NUM_LEDS - 1, 0);
  s2.setPixelColor(NUM_LEDS - 1, 0);
  s1.show();
  s2.show();

  delay(300); // pause before next color
}
