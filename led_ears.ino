#include <Adafruit_NeoPixel.h>
#include <math.h> // For sin() function

// Configuration
#define NUM_LEDS      160
#define DATA_PIN_1    0
#define DATA_PIN_2    1
#define MAX_BRIGHTNESS 150   // Max brightness (0–255)
#define MIN_BRIGHTNESS 20    // Min brightness
#define PULSE_SPEED     0.05 // Lower = slower breathing
#define HUE_STEP        1    // How fast hue changes (1 = slow)

// Create two NeoPixel objects for each LED strip
Adafruit_NeoPixel strip1(NUM_LEDS, DATA_PIN_1, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2(NUM_LEDS, DATA_PIN_2, NEO_GRB + NEO_KHZ800);

float angle = 0.0; // For breathing effect (sin wave)
uint16_t hue = 0;  // For color cycling

void setup() {
  strip1.begin();
  strip2.begin();
  strip1.show();
  strip2.show();
}

void loop() {
  // Calculate brightness using a sine wave
  float brightnessFactor = (sin(angle) + 1.0) / 2.0; // Normalize to 0–1
  uint8_t brightness = MIN_BRIGHTNESS + (brightnessFactor * (MAX_BRIGHTNESS - MIN_BRIGHTNESS));

  // Convert hue to RGB
  uint32_t color = HSVtoRGB(hue, 255, brightness); // full saturation, variable brightness

  // Set color on both strips
  fillStrip(strip1, color);
  fillStrip(strip2, color);

  strip1.show();
  strip2.show();

  // Update values for next frame
  angle += PULSE_SPEED;
  if (angle >= TWO_PI) angle -= TWO_PI;

  hue += HUE_STEP;
  if (hue >= 360) hue = 0;

  delay(10); // Small delay to control speed
}

void fillStrip(Adafruit_NeoPixel& strip, uint32_t color) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, color);
  }
}

// Convert HSV to packed RGB color
uint32_t HSVtoRGB(uint16_t h, uint8_t s, uint8_t v) {
  float r, g, b;

  float hf = h / 60.0;
  int i = floor(hf);
  float f = hf - i;
  float p = v * (1.0 - s / 255.0);
  float q = v * (1.0 - f * s / 255.0);
  float t = v * (1.0 - (1.0 - f) * s / 255.0);

  switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
  }

  return strip1.Color((int)r, (int)g, (int)b);
}
