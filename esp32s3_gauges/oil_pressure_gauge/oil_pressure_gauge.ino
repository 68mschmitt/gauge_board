/**
 * Oil Pressure Gauge - ESP32-S3 Single Display
 * 
 * Receives data via UART from external source
 * Protocol: OIL:<needle_0_to_1>:<afr_value>\n
 * Example:  OIL:0.50:14.7
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

GaugeProtocol protocol(GAUGE_ID_OIL);

// Current values
float needleValue = 0.5f;
float digitalValue = 14.7f;  // AFR is float
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
// OIL CAN ICON (Genie lamp style with dripping oil drop)
// ============================================================================

void drawOilCanIcon(TFT_eSprite &spr, int cx, int cy, int w, int h,
                    uint16_t fg, uint16_t bg) {
  int bodyW = (int)(w * 0.7f);
  int bodyH = (int)(h * 0.45f);
  int bodyX = cx - bodyW / 2 + 2;
  int bodyY = cy + (int)(h * 0.1f);

  spr.fillEllipse(bodyX + bodyW/2, bodyY + bodyH/2, bodyW/2, bodyH/2, fg);

  int spoutStartX = bodyX - 2;
  int spoutStartY = bodyY + bodyH/3;
  int spoutMidX = cx - (int)(w * 0.4f);
  int spoutMidY = cy - (int)(h * 0.2f);
  int spoutTipX = cx - (int)(w * 0.5f);
  int spoutTipY = cy - (int)(h * 0.15f);

  for (int t = -1; t <= 1; t++) {
    spr.drawLine(spoutStartX, spoutStartY + t, spoutMidX, spoutMidY + t, fg);
    spr.drawLine(spoutMidX, spoutMidY + t, spoutTipX, spoutTipY + t, fg);
  }

  spr.fillCircle(spoutTipX - 1, spoutTipY, 2, fg);

  int handleX = bodyX + bodyW - 2;
  int handleY = bodyY + bodyH/3;
  int handleOuterX = cx + (int)(w * 0.45f);
  int handleMidY = bodyY + bodyH/2;
  int handleBottomY = bodyY + bodyH - 2;

  for (int t = -1; t <= 1; t++) {
    spr.drawLine(handleX, handleY, handleOuterX + t, handleMidY, fg);
    spr.drawLine(handleOuterX + t, handleMidY, handleX, handleBottomY, fg);
  }

  int lidW = (int)(bodyW * 0.3f);
  int lidH = 4;
  int lidX = bodyX + bodyW/2 - lidW/2;
  int lidY = bodyY - lidH + 2;
  spr.fillRect(lidX, lidY, lidW, lidH, fg);
  spr.fillCircle(lidX + lidW/2, lidY - 1, 2, fg);

  int dropX = spoutTipX - 3;
  int dropY = spoutTipY + 6;
  int dropH = 8;
  int dropW = 4;

  spr.fillCircle(dropX, dropY + dropH - dropW/2, dropW/2, fg);
  spr.fillTriangle(
    dropX - dropW/2, dropY + dropH - dropW/2,
    dropX + dropW/2, dropY + dropH - dropW/2,
    dropX, dropY,
    fg
  );
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

  drawOilCanIcon(spr, CX, CY - 55, 28, 32, COL_TEXT, COL_DIAL);

  spr.setTextColor(COL_TEXT, COL_DIAL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(3);
  spr.drawString("L", CX - 85, CY - 25);
  spr.drawString("H", CX + 85, CY - 25);

  spr.setTextSize(1);
  spr.drawString("AFR", CX, CY + 100);
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

void drawDigitalReadout(TFT_eSprite &spr, float value) {
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(COL_ACCENT, COL_DIAL);
  spr.setTextSize(3);

  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f", value);
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
  snprintf(buf, sizeof(buf), "%.1f", digitalValue);
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

  Serial.println("Oil Pressure Gauge Ready");
  Serial.println("Expecting: OIL:<needle>:<afr>");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  protocol.update();

  if (protocol.hasUpdate()) {
    needleValue = protocol.getNeedle();
    digitalValue = protocol.getDigital();
  }

  if (fabsf(needleValue - prevNeedleValue) > NEEDLE_THRESHOLD) {
    renderFullGauge();
    prevNeedleValue = needleValue;
  } else {
    pushReadoutOnly();
  }
}
