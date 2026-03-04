/**
 * Water Temperature Gauge - ESP32-S3 Single Display
 * 
 * Receives data via UART from external source
 * Protocol: WATER:<needle_0_to_1>:<oil_temp_f>\n
 * Example:  WATER:0.60:210
 * 
 * Hardware:
 *   - ESP32-S3 DevKit
 *   - GC9A01 240x240 display (SPI)
 *   - UART RX on GPIO18 for data input
 */

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

GaugeProtocol protocol(GAUGE_ID_WATER);

// Current values
float needleValue = 0.5f;
int digitalValue = 200;  // Oil temp in °F
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
// THERMOMETER IN WATER ICON
// ============================================================================

void drawThermometerWaterIcon(TFT_eSprite &spr, int cx, int cy, int w, int h,
                               uint16_t fg, uint16_t bg) {
  int thermW = 6;
  int thermH = (int)(h * 0.7f);
  int bulbR = 5;

  int thermX = cx;
  int thermTopY = cy - (int)(h * 0.35f);
  int thermBotY = thermTopY + thermH;
  int bulbY = thermBotY + bulbR - 2;

  spr.drawRect(thermX - thermW/2, thermTopY, thermW, thermH, fg);
  spr.drawLine(thermX - thermW/2, thermTopY, thermX + thermW/2 - 1, thermTopY, fg);
  spr.drawPixel(thermX - thermW/2, thermTopY + 1, fg);
  spr.drawPixel(thermX + thermW/2 - 1, thermTopY + 1, fg);

  spr.drawCircle(thermX, bulbY, bulbR, fg);
  spr.fillCircle(thermX, bulbY, bulbR - 2, fg);

  int mercuryH = (int)(thermH * 0.5f);
  int mercuryTop = thermBotY - mercuryH;
  spr.fillRect(thermX - thermW/2 + 2, mercuryTop, thermW - 4, mercuryH, fg);

  // 3 horizontal lines on right side of thermometer
  int lineLen = 5;
  int lineSpacing = (int)(thermH * 0.2f);
  int lineStartX = thermX + thermW/2 + 1;

  for (int i = 0; i < 3; i++) {
    int lineY = thermTopY + (int)(thermH * 0.25f) + i * lineSpacing;
    spr.drawLine(lineStartX, lineY, lineStartX + lineLen, lineY, fg);
  }

  // Water waves below thermometer (wider)
  int waveY1 = bulbY + bulbR + 3;
  int waveY2 = waveY1 + 5;
  int waveAmplitude = 2;
  int waveStartX = cx - (int)(w * 0.75f);
  int waveEndX = cx + (int)(w * 0.75f);

  // First wavy line - split around bulb
  for (int x = waveStartX; x < thermX - bulbR - 2; x++) {
    float phase = (x - waveStartX) * 0.4f;
    int waveOffset = (int)(sinf(phase) * waveAmplitude);
    spr.drawPixel(x, waveY1 + waveOffset, fg);
    spr.drawPixel(x, waveY1 + waveOffset + 1, fg);
  }

  for (int x = thermX + bulbR + 2; x < waveEndX; x++) {
    float phase = (x - waveStartX) * 0.4f;
    int waveOffset = (int)(sinf(phase) * waveAmplitude);
    spr.drawPixel(x, waveY1 + waveOffset, fg);
    spr.drawPixel(x, waveY1 + waveOffset + 1, fg);
  }

  // Second wavy line - continuous
  for (int x = waveStartX; x < waveEndX; x++) {
    float phase = (x - waveStartX) * 0.4f + 1.5f;
    int waveOffset = (int)(sinf(phase) * waveAmplitude);
    spr.drawPixel(x, waveY2 + waveOffset, fg);
    spr.drawPixel(x, waveY2 + waveOffset + 1, fg);
  }
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

  drawThermometerWaterIcon(spr, CX, CY - 55, 24, 38, COL_TEXT, COL_DIAL);

  spr.setTextColor(COL_TEXT, COL_DIAL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(3);
  spr.drawString("C", CX - 85, CY - 25);
  spr.drawString("H", CX + 85, CY - 25);

  spr.setTextSize(1);
  char tempLabel[16];
  snprintf(tempLabel, sizeof(tempLabel), "Oil %cF", (char)247);
  spr.drawString(tempLabel, CX, CY + 100);
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
  Serial.begin(115200);
  protocol.begin();

  tft.init();
  tft.setRotation(0);
  tft.fillScreen(COL_BG);

  gauge.setColorDepth(16);
  gauge.createSprite(SCREEN_W, SCREEN_H);

  readout.setColorDepth(16);
  readout.createSprite(READOUT_W, READOUT_H);

  renderFullGauge();

  Serial.println("Water Temperature Gauge Ready");
  Serial.println("Expecting: WATER:<needle>:<oil_temp>");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  protocol.update();

  if (protocol.hasUpdate()) {
    needleValue = protocol.getNeedle();
    digitalValue = (int)protocol.getDigital();
  }

  if (fabsf(needleValue - prevNeedleValue) > NEEDLE_THRESHOLD) {
    renderFullGauge();
    prevNeedleValue = needleValue;
  } else {
    pushReadoutOnly();
  }
}
