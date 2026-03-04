/**
 * Fuel Gauge - ESP32-S3 Single Display
 * 
 * Receives data via UART from external source
 * Protocol: FUEL:<needle_0_to_1>:<boost_value>\n
 * Example:  FUEL:0.75:-15
 * 
 * Hardware:
 *   - ESP32-S3 DevKit
 *   - GC9A01 240x240 display (SPI)
 *   - UART RX on GPIO18 for data input (or USB with USE_USB_SERIAL)
 */

// Uncomment to receive data via USB instead of GPIO18
#define USE_USB_SERIAL

#include <TFT_eSPI.h>
#include <SPI.h>
#include "gauge_protocol.h"

// ============================================================================
// DISPLAY & SPRITE
// ============================================================================

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite gauge = TFT_eSprite(&tft);

// Small sprite for fast number updates
static const int READOUT_W = 70;
static const int READOUT_H = 30;
static const int READOUT_X = (240 - READOUT_W) / 2;
static const int READOUT_Y = 120 + 65 - READOUT_H / 2;
TFT_eSprite readout = TFT_eSprite(&tft);

// ============================================================================
// GAUGE CONFIGURATION
// ============================================================================

static const int SCREEN_W = 240;
static const int SCREEN_H = 240;
static const int CX = SCREEN_W / 2;
static const int CY = SCREEN_H / 2;

static const int R_TICK1 = 115;
static const int R_TICK2 = 105;
static const int R_NEEDLE = 90;

static const float ANGLE_MIN = -145.0f;
static const float ANGLE_MAX = -35.0f;

// Colors
static const uint16_t COL_BG     = TFT_BLACK;
static const uint16_t COL_DIAL   = TFT_BLACK;
static const uint16_t COL_TICKS  = TFT_WHITE;
static const uint16_t COL_TEXT   = TFT_WHITE;
static const uint16_t COL_NEEDLE = TFT_WHITE;
static const uint16_t COL_ACCENT = TFT_CYAN;

// ============================================================================
// UART PROTOCOL
// ============================================================================

GaugeProtocol protocol(GAUGE_ID_FUEL);

// Current values
float needleValue = 0.0f;
int digitalValue = 0;
float prevNeedleValue = -1.0f;

static const float NEEDLE_THRESHOLD = 0.01f;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

static inline float deg2rad(float d) {
  return d * 0.017453292519943295f;
}

static inline void polarPoint(int cx, int cy, int r, float angDeg, int &x, int &y) {
  float a = deg2rad(angDeg);
  x = cx + (int)lroundf(cosf(a) * r);
  y = cy + (int)lroundf(sinf(a) * r);
}

// ============================================================================
// GAS PUMP ICON
// ============================================================================

void drawGasPumpIcon(TFT_eSprite &spr, int cx, int cy, int w, int h,
                     uint16_t fg, uint16_t bg) {
  int x = cx - w / 2;
  int y = cy - h / 2;
  int r = max(2, w / 8);
  spr.fillRoundRect(x, y, w, h, r, fg);

  int inset = max(2, w / 10);
  spr.fillRoundRect(x + inset, y + inset, w - 2*inset, h - 2*inset, max(1, r-1), bg);

  int winW = (int)(w * 0.55f);
  int winH = (int)(h * 0.22f);
  int winX = cx - winW / 2;
  int winY = y + (int)(h * 0.18f);
  spr.drawRoundRect(winX, winY, winW, winH, max(1, r-2), fg);

  int sepY = y + (int)(h * 0.55f);
  spr.drawLine(x + inset, sepY, x + w - inset - 1, sepY, fg);

  int footH = max(2, h / 10);
  spr.fillRect(x + (int)(w * 0.18f), y + h - footH - inset, (int)(w * 0.64f), footH, fg);

  int hoseX = x + w - inset - 1;
  int hoseTop = winY + 1;
  int hoseMid = sepY;
  spr.drawLine(hoseX, hoseTop, hoseX + w/6, hoseTop + h/6, fg);
  spr.drawLine(hoseX + w/6, hoseTop + h/6, hoseX + w/6, hoseMid, fg);

  int nozX = hoseX + w/6;
  int nozY = hoseMid;
  spr.drawLine(nozX, nozY, nozX + w/8, nozY + h/10, fg);
  spr.drawLine(nozX + w/8, nozY + h/10, nozX + w/8, nozY + h/6, fg);
}

// ============================================================================
// GAUGE RENDERING
// ============================================================================

void drawDialBase(TFT_eSprite &spr) {
  spr.fillSprite(COL_BG);

  const int majorTicks = 3;
  const int intermediateTicks = 2;
  const int totalTicks = majorTicks + (intermediateTicks * 2);

  for (int i = 0; i < totalTicks; i++) {
    float t = (float)i / (float)(totalTicks - 1);
    float ang = ANGLE_MIN + t * (ANGLE_MAX - ANGLE_MIN);

    int x1, y1, x2, y2;
    polarPoint(CX, CY, R_TICK1, ang, x1, y1);
    polarPoint(CX, CY, R_TICK2, ang, x2, y2);

    bool isMajorTick = (i == 0) || (i == totalTicks - 1) || (i == totalTicks / 2);

    if (isMajorTick) {
      int x3, y3;
      polarPoint(CX, CY, R_TICK2 - 6, ang, x3, y3);

      float a = deg2rad(ang);
      float nx = -sinf(a);
      float ny = cosf(a);

      int thickness = 8;
      for (int tt = -thickness/2; tt <= thickness/2; tt++) {
        int ox = nx * tt;
        int oy = ny * tt;
        spr.drawLine(x1 + ox, y1 + oy, x3 + ox, y3 + oy, COL_TICKS);
      }
    } else {
      spr.drawLine(x1, y1, x2, y2, COL_TICKS);
    }
  }

  drawGasPumpIcon(spr, CX, CY - 55, 20, 35, COL_TEXT, COL_DIAL);

  spr.setTextColor(COL_TEXT, COL_DIAL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(3);
  spr.drawString("E", CX - 85, CY - 25);
  spr.drawString("F", CX + 85, CY - 25);

  spr.setTextSize(1);
  spr.drawString("BOOST", CX, CY + 100);
}

void drawNeedleCone(TFT_eSprite &spr, float value01) {
  value01 = constrain(value01, 0.0f, 1.0f);

  float angDeg = ANGLE_MIN + value01 * (ANGLE_MAX - ANGLE_MIN);
  float a = deg2rad(angDeg);

  int xTip, yTip;
  polarPoint(CX, CY, R_NEEDLE, angDeg, xTip, yTip);

  float nx = -sinf(a);
  float ny = cosf(a);

  const float baseW = 5.0f;
  const float tipW = 3.0f;
  float b = baseW * 0.5f;
  float t = tipW * 0.5f;

  int xBL = CX + (int)lroundf(nx * b);
  int yBL = CY + (int)lroundf(ny * b);
  int xBR = CX - (int)lroundf(nx * b);
  int yBR = CY - (int)lroundf(ny * b);

  int xTL = xTip + (int)lroundf(nx * t);
  int yTL = yTip + (int)lroundf(ny * t);
  int xTR = xTip - (int)lroundf(nx * t);
  int yTR = yTip - (int)lroundf(ny * t);

  spr.fillTriangle(xBL, yBL, xBR, yBR, xTL, yTL, COL_NEEDLE);
  spr.fillTriangle(xBR, yBR, xTR, yTR, xTL, yTL, COL_NEEDLE);

  spr.fillCircle(CX, CY, 20, TFT_DARKGREY);
  spr.fillCircle(CX, CY, 19, TFT_BLACK);
}

void drawDigitalReadout(TFT_eSprite &spr, int value) {
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(COL_ACCENT, COL_DIAL);
  spr.setTextSize(3);

  char buf[12];
  snprintf(buf, sizeof(buf), "%d", value);
  spr.drawString(buf, CX, CY + 65);
}

void renderFullGauge() {
  drawDialBase(gauge);
  drawNeedleCone(gauge, needleValue);
  drawDigitalReadout(gauge, digitalValue);
  gauge.pushSprite(0, 0);
}

void pushReadoutOnly() {
  readout.fillSprite(COL_BG);
  readout.setTextDatum(MC_DATUM);
  readout.setTextColor(COL_ACCENT, COL_BG);
  readout.setTextSize(3);

  char buf[12];
  snprintf(buf, sizeof(buf), "%d", digitalValue);
  readout.drawString(buf, READOUT_W / 2, READOUT_H / 2);

  readout.pushSprite(READOUT_X, READOUT_Y);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);  // Debug output
  protocol.begin();      // UART data input

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(COL_BG);

  gauge.setColorDepth(16);
  gauge.createSprite(SCREEN_W, SCREEN_H);

  readout.setColorDepth(16);
  readout.createSprite(READOUT_W, READOUT_H);

  // Initial render
  renderFullGauge();

  Serial.println("Fuel Gauge Ready");
  Serial.println("Format: FUEL:<percent>:<boost_min>:<boost_max>:<boost_actual>");
  Serial.println("Example: FUEL:75:-20:20:5");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  static unsigned long lastFpsTime = 0;
  static int frameCount = 0;
  
  // Process incoming UART data
  protocol.update();

  // Check for new data
  if (protocol.hasUpdate()) {
    needleValue = protocol.getNeedle();
    digitalValue = (int)protocol.getDigital();
  }

  // Update display based on what changed
  if (fabsf(needleValue - prevNeedleValue) > NEEDLE_THRESHOLD) {
    // Needle moved significantly - full redraw
    renderFullGauge();
    prevNeedleValue = needleValue;
    frameCount++;
  } else {
    // Just update the number (fast)
    pushReadoutOnly();
  }
  
  // Print FPS every second
  if (millis() - lastFpsTime >= 1000) {
    Serial.printf("FPS: %d\n", frameCount);
    frameCount = 0;
    lastFpsTime = millis();
  }
}
