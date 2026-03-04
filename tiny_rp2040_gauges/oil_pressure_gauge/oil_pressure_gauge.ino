/**
 * Oil Pressure Gauge
 * Based on gauge_template
 */

#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite dial = TFT_eSprite(&tft);

// ============================================================================
// GAUGE-SPECIFIC CONFIGURATION
// ============================================================================

typedef void (*IconDrawFunc)(TFT_eSprite&, int, int, int, int, uint16_t, uint16_t);

// Forward declaration
void drawOilCanIcon(TFT_eSprite &spr, int cx, int cy, int w, int h, uint16_t fg, uint16_t bg);

static IconDrawFunc GAUGE_ICON_FUNC = &drawOilCanIcon;

static const int ICON_OFFSET_Y = -55;
static const int ICON_WIDTH = 28;
static const int ICON_HEIGHT = 32;

static const char* GAUGE_START_LABEL = "L";
static const char* GAUGE_END_LABEL = "H";
static const int LABEL_OFFSET_X = 85;
static const int LABEL_OFFSET_Y = -25;

static const char* GAUGE_SECONDARY_LABEL = "AFR";
static const int SECONDARY_LABEL_Y = 100;

static const int GAUGE_INTERMEDIATE_TICKS = 2;

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================

static const int SCREEN_W = 240;
static const int SCREEN_H = 240;

// ============================================================================
// GAUGE GEOMETRY
// ============================================================================

static const int CX = SCREEN_W / 2;
static const int CY = SCREEN_H / 2;

static const int R_TICK1 = 115;
static const int R_TICK2 = 105;
static const int R_NEEDLE = 90;

static const float ANGLE_MIN = -145.0f;
static const float ANGLE_MAX = -35.0f;

// ============================================================================
// COLORS
// ============================================================================

static const uint16_t COL_BG      = TFT_BLACK;
static const uint16_t COL_RING    = TFT_DARKGREY;
static const uint16_t COL_DIAL    = TFT_BLACK;
static const uint16_t COL_TICKS   = TFT_WHITE;
static const uint16_t COL_TEXT    = TFT_WHITE;
static const uint16_t COL_NEEDLE  = TFT_WHITE;
static const uint16_t COL_ACCENT  = TFT_CYAN;

// ============================================================================
// STATE VARIABLES
// ============================================================================

float gaugeValue = 0.5f;
int   digitalValue = 0;

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
                    uint16_t fg, uint16_t bg)
{
  // Genie lamp style oil can
  // Main body is an oval/ellipse shape
  int bodyW = (int)(w * 0.7f);
  int bodyH = (int)(h * 0.45f);
  int bodyX = cx - bodyW / 2 + 2;  // Shifted right slightly to balance spout
  int bodyY = cy + (int)(h * 0.1f);
  
  // Draw body as filled ellipse (approximate with rounded rect)
  spr.fillEllipse(bodyX + bodyW/2, bodyY + bodyH/2, bodyW/2, bodyH/2, fg);
  
  // Spout - curved like a genie lamp, going up and left
  int spoutStartX = bodyX - 2;
  int spoutStartY = bodyY + bodyH/3;
  
  // Spout neck going up
  int spoutMidX = cx - (int)(w * 0.4f);
  int spoutMidY = cy - (int)(h * 0.2f);
  
  // Spout tip curving left
  int spoutTipX = cx - (int)(w * 0.5f);
  int spoutTipY = cy - (int)(h * 0.15f);
  
  // Draw spout with thickness
  for (int t = -1; t <= 1; t++) {
    spr.drawLine(spoutStartX, spoutStartY + t, spoutMidX, spoutMidY + t, fg);
    spr.drawLine(spoutMidX, spoutMidY + t, spoutTipX, spoutTipY + t, fg);
  }
  
  // Spout opening (small circle at tip)
  spr.fillCircle(spoutTipX - 1, spoutTipY, 2, fg);
  
  // Handle on the right side - curved handle like a teapot
  int handleX = bodyX + bodyW - 2;
  int handleY = bodyY + bodyH/3;
  
  // Handle arc
  int handleOuterX = cx + (int)(w * 0.45f);
  int handleMidY = bodyY + bodyH/2;
  int handleBottomY = bodyY + bodyH - 2;
  
  for (int t = -1; t <= 1; t++) {
    spr.drawLine(handleX, handleY, handleOuterX + t, handleMidY, fg);
    spr.drawLine(handleOuterX + t, handleMidY, handleX, handleBottomY, fg);
  }
  
  // Lid/cap on top of body
  int lidW = (int)(bodyW * 0.3f);
  int lidH = 4;
  int lidX = bodyX + bodyW/2 - lidW/2;
  int lidY = bodyY - lidH + 2;
  spr.fillRect(lidX, lidY, lidW, lidH, fg);
  
  // Small knob on lid
  spr.fillCircle(lidX + lidW/2, lidY - 1, 2, fg);
  
  // Oil drop dripping from spout
  int dropX = spoutTipX - 3;
  int dropY = spoutTipY + 6;
  int dropH = 8;
  int dropW = 4;
  
  // Teardrop shape - circle at bottom, pointed at top
  spr.fillCircle(dropX, dropY + dropH - dropW/2, dropW/2, fg);
  // Triangle pointing up to connect to spout area
  spr.fillTriangle(
    dropX - dropW/2, dropY + dropH - dropW/2,  // bottom left
    dropX + dropW/2, dropY + dropH - dropW/2,  // bottom right
    dropX, dropY,                               // top point
    fg
  );
}

// ============================================================================
// DIAL RENDERING
// ============================================================================

void drawDialBase(TFT_eSprite &spr) {
  spr.fillSprite(COL_BG);

  const int majorTicks = 3;
  const int totalTicks = majorTicks + (GAUGE_INTERMEDIATE_TICKS * 2);

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
      for (int t = -thickness/2; t <= thickness/2; t++) {
        int ox = nx * t;
        int oy = ny * t;
        spr.drawLine(x1 + ox, y1 + oy, x3 + ox, y3 + oy, COL_TICKS);
      }
    } else if (GAUGE_INTERMEDIATE_TICKS > 0) {
      spr.drawLine(x1, y1, x2, y2, COL_TICKS);
    }
  }

  if (GAUGE_ICON_FUNC != nullptr) {
    GAUGE_ICON_FUNC(spr, CX, CY + ICON_OFFSET_Y, ICON_WIDTH, ICON_HEIGHT, COL_TEXT, COL_DIAL);
  }

  spr.setTextColor(COL_TEXT, COL_DIAL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(3);
  
  if (GAUGE_START_LABEL[0] != '\0') {
    spr.drawString(GAUGE_START_LABEL, CX - LABEL_OFFSET_X, CY + LABEL_OFFSET_Y);
  }
  if (GAUGE_END_LABEL[0] != '\0') {
    spr.drawString(GAUGE_END_LABEL, CX + LABEL_OFFSET_X, CY + LABEL_OFFSET_Y);
  }

  if (GAUGE_SECONDARY_LABEL[0] != '\0') {
    spr.setTextSize(1);
    spr.drawString(GAUGE_SECONDARY_LABEL, CX, CY + SECONDARY_LABEL_Y);
  }
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

void renderGauge(float value01, int readoutValue) {
  drawDialBase(dial);
  drawNeedleCone(dial, value01);
  drawDigitalReadout(dial, readoutValue);
  dial.pushSprite(0, 0);
}

// ============================================================================
// ARDUINO SETUP & LOOP
// ============================================================================

void setup() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(COL_BG);

  dial.setColorDepth(16);
  dial.createSprite(SCREEN_W, SCREEN_H);
  dial.fillSprite(COL_BG);

  renderGauge(gaugeValue, digitalValue);
}

void loop() {
  static uint32_t last = 0;
  if (millis() - last > 40) {
    last = millis();

    static float dir = 0.006f;
    gaugeValue += dir;
    if (gaugeValue >= 1.0f) { gaugeValue = 1.0f; dir = -dir; }
    if (gaugeValue <= 0.0f) { gaugeValue = 0.0f; dir = -dir; }

    static int ddir = 1;
    digitalValue += ddir;
    if (digitalValue > 18) ddir = -1;
    if (digitalValue < 10) ddir = 1;

    renderGauge(gaugeValue, digitalValue);
  }
}
