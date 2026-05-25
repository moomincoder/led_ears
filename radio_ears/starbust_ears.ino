#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <math.h>

#define NUM_LEDS  160
#define PIN_LEFT  0
#define PIN_RIGHT 1
#define NUM_ROWS  7

// ── Left ear row widths, bottom (widest) to top (tip) ────────────────────────
const uint8_t L_BACK_ROWS[NUM_ROWS]  = {13, 14, 13, 12, 11, 10, 8};
const uint8_t L_FRONT_ROWS[NUM_ROWS] = {14, 14, 13, 12, 10,  9, 7};

// ── Right ear row widths, bottom (widest) to top (tip) ───────────────────────
const uint8_t R_BACK_ROWS[NUM_ROWS]  = {13, 14, 13, 12, 11, 10, 8};
const uint8_t R_FRONT_ROWS[NUM_ROWS] = {14, 14, 13, 12, 10,  9, 7};

// ── Effect tuning ─────────────────────────────────────────────────────────────
#define Y_SCALE            1.5f  // Vertical row spacing vs horizontal LED pitch.
                                  // Raise if burst looks squashed top/bottom;
                                  // lower if squashed left/right.
#define TRAIL_LEN          4.5f  // Distance units of fading trail behind wavefront
#define MAX_BRIGHTNESS     200   // Peak brightness at wavefront (0–255)
#define HUE_STEP_PER_BURST  20   // Hue advance each burst cycle (slow rainbow)

// ── BLE speed control ─────────────────────────────────────────────────────────
// At 0 packets/30 s  → 0.25× speed (2400 ms per burst — idle drift)
// At BLE_MAX_PKTS    → 4.0× speed  ( 150 ms per burst — frenetic)
#define BLE_WINDOW_MS   30000    // rolling window length in milliseconds
#define BLE_MAX_PKTS      300    // packet count that saturates the speed scale
#define BLE_SPEED_MIN    0.25f   // multiplier at 0 packets
#define BLE_SPEED_MAX    4.00f   // multiplier at BLE_MAX_PKTS packets
#define BLE_SMOOTH       0.05f   // low-pass weight toward target (per frame)

// Timestamp ring buffer — one entry per received advertisement.
// 512 slots covers ~17 pkt/s sustained for 30 s before wrapping;
// older entries drop off the window naturally once they age out.
#define BLE_BUF_SIZE      512
static uint32_t bleTimestamps[BLE_BUF_SIZE];
static volatile uint16_t bleHead  = 0;  // next write slot
static volatile uint16_t bleTotal = 0;  // how many valid entries (caps at BLE_BUF_SIZE)

class AdvCallback : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice dev) override {
    uint16_t h = bleHead;
    bleTimestamps[h] = millis();
    bleHead = (h + 1) % BLE_BUF_SIZE;
    if (bleTotal < BLE_BUF_SIZE) bleTotal++;
  }
};

static BLEScan*      bleScan;
static AdvCallback   advCallback;

// Count advertisements received within the last BLE_WINDOW_MS milliseconds.
// Walks the ring buffer backward from the most recent entry; stops as soon as
// it hits an entry older than the window (buffer is filled in time order).
uint16_t countRecentBLE() {
  uint32_t cutoff = millis() - BLE_WINDOW_MS;
  uint16_t total  = bleTotal;          // snapshot (written by ISR)
  uint16_t head   = bleHead;
  uint16_t count  = 0;

  for (uint16_t i = 0; i < total; i++) {
    uint16_t idx = (head - 1 - i + BLE_BUF_SIZE) % BLE_BUF_SIZE;
    if (bleTimestamps[idx] >= cutoff) {
      count++;
    } else {
      break;  // entries are chronological, so all earlier ones are also expired
    }
  }
  return count;
}

Adafruit_NeoPixel leftEar (NUM_LEDS, PIN_LEFT,  NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel rightEar(NUM_LEDS, PIN_RIGHT, NEO_GRB + NEO_KHZ800);

float ledDistL[NUM_LEDS];  // radial distance from face-center, left ear
float ledDistR[NUM_LEDS];  // radial distance from face-center, right ear
float maxDist = 0.0f;      // farthest LED across both ears (set in setup)

float    burstRadius  = 0.0f;
float    speedMul     = 1.0f;  // current smoothed speed multiplier
uint16_t hue          = 0;

uint32_t HSVtoRGB(uint16_t h, uint8_t s, uint8_t v);

// Build a per-LED radial-distance map for one ear.
// Rows are interleaved: back[0], front[0], back[1], front[1], ...
// Each face is centered independently (row 3, column center of that row).
// Returns the maximum distance found (used to set the burst reset threshold).
float buildLedMap(const uint8_t backRows[], const uint8_t frontRows[], float dist[]) {
  const float centerRow = (NUM_ROWS - 1) / 2.0f;  // 3.0
  float maxD = 0.0f;
  int   idx  = 0;

  for (int row = 0; row < NUM_ROWS; row++) {
    float yOff = (row - centerRow) * Y_SCALE;

    uint8_t bw  = backRows[row];
    float   bCx = (bw - 1) / 2.0f;
    for (int col = 0; col < bw; col++) {
      float xOff = col - bCx;
      float d    = sqrtf(xOff * xOff + yOff * yOff);
      dist[idx++] = d;
      if (d > maxD) maxD = d;
    }

    uint8_t fw  = frontRows[row];
    float   fCx = (fw - 1) / 2.0f;
    for (int col = 0; col < fw; col++) {
      float xOff = col - fCx;
      float d    = sqrtf(xOff * xOff + yOff * yOff);
      dist[idx++] = d;
      if (d > maxD) maxD = d;
    }
  }
  return maxD;
}

void setup() {
  leftEar.begin();
  rightEar.begin();
  leftEar.show();
  rightEar.show();

  float mL = buildLedMap(L_BACK_ROWS, L_FRONT_ROWS, ledDistL);
  float mR = buildLedMap(R_BACK_ROWS, R_FRONT_ROWS, ledDistR);
  maxDist = max(mL, mR);  // reset only after both ears have fully burst

  // Start passive BLE scanning — we only need to count advertisements,
  // not connect to or read data from any device.
  BLEDevice::init("");
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(&advCallback, true);  // true = keep duplicates
  bleScan->setActiveScan(false);   // passive — no scan-request packets
  bleScan->setInterval(100);
  bleScan->setWindow(99);
  bleScan->start(0, nullptr, false);  // 0 = scan indefinitely, non-blocking
}

void loop() {
  leftEar.clear();
  rightEar.clear();

  // ── Update speed multiplier from BLE packet rate ───────────────────────────
  uint16_t pkts   = countRecentBLE();
  float    target = BLE_SPEED_MIN + (BLE_SPEED_MAX - BLE_SPEED_MIN)
                    * min((float)pkts / BLE_MAX_PKTS, 1.0f);
  speedMul += BLE_SMOOTH * (target - speedMul);  // gentle low-pass

  float step = (maxDist / 60.0f) * speedMul;  // distance increment per 10 ms frame

  uint16_t hue2 = (hue + 180) % 360;  // Complementary color for right ear

  for (int i = 0; i < NUM_LEDS; i++) {
    // Left ear
    float dL = burstRadius - ledDistL[i];
    if (dL >= 0.0f && dL <= TRAIL_LEN) {
      float   bf = expf(-dL * 3.0f / TRAIL_LEN);
      uint8_t br = (uint8_t)(bf * MAX_BRIGHTNESS);
      leftEar.setPixelColor(i, HSVtoRGB(hue, 255, br));
    }

    // Right ear
    float dR = burstRadius - ledDistR[i];
    if (dR >= 0.0f && dR <= TRAIL_LEN) {
      float   bf = expf(-dR * 3.0f / TRAIL_LEN);
      uint8_t br = (uint8_t)(bf * MAX_BRIGHTNESS);
      rightEar.setPixelColor(i, HSVtoRGB(hue2, 255, br));
    }
  }

  leftEar.show();
  rightEar.show();

  burstRadius += step;
  if (burstRadius > maxDist + TRAIL_LEN) {
    burstRadius = 0.0f;
    hue = (hue + HUE_STEP_PER_BURST) % 360;
  }

  delay(10);
}

// Convert HSV (h: 0–359, s: 0–255, v: 0–255) to packed RGB color
uint32_t HSVtoRGB(uint16_t h, uint8_t s, uint8_t v) {
  float r, g, b;
  float hf = h / 60.0f;
  int   i  = (int)hf;
  float f  = hf - i;
  float p  = v * (1.0f - s / 255.0f);
  float q  = v * (1.0f - f * s / 255.0f);
  float t  = v * (1.0f - (1.0f - f) * s / 255.0f);
  switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
    default: r = 0; g = 0; b = 0; break;
  }
  return leftEar.Color((uint8_t)r, (uint8_t)g, (uint8_t)b);
}
