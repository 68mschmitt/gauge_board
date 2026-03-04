/**
 * Water Temperature Gauge
 * Based on gauge_template with custom tick pattern
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
void drawThermometerWaterIcon(TFT_eSprite &spr, int cx, int cy, int w, int h, uint16_t fg, uint16_t bg);

static IconDrawFunc GAUGE_ICON_FUNC = &drawThermometerWaterIcon;

static const int ICON_OFFSET_Y = -55;
static const int ICON_WIDTH = 24;
static const int ICON_HEIGHT = 38;

static const char* GAUGE_START_LABEL = "C";
static const char* GAUGE_END_LABEL = "H";
static const int LABEL_OFFSET_X = 85;
static const int LABEL_OFFSET_Y = -25;

// Secondary label with degree symbol
static const char* GAUGE_SECONDARY_LABEL = "Oil";  // We'll draw the °F separately
static const int SECONDARY_LABEL_Y = 100;

// Custom tick pattern - handled specially in drawDialBase
// 2 ticks clustered at each end with 2px spacing, connected by arc

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
// THERMOMETER IN WATER ICON
// Mercury thermometer with 3 horizontal lines, floating in wavy water
// ============================================================================

void drawThermometerWaterIcon(TFT_eSprite &spr, int cx, int cy, int w, int h,
                               uint16_t fg, uint16_t bg)
{
  // Thermometer dimensions
  int thermW = 6;  // Width of thermometer tube
  int thermH = (int)(h * 0.7f);  // Height of thermometer
  int bulbR = 5;   // Radius of mercury bulb at bottom
  
  int thermX = cx;  // Center x of thermometer
  int thermTopY = cy - (int)(h * 0.35f);
  int thermBotY = thermTopY + thermH;
  int bulbY = thermBotY + bulbR - 2;
  
  // Draw thermometer tube (outline)
  spr.drawRect(thermX - thermW/2, thermTopY, thermW, thermH, fg);
  
  // Draw rounded top of thermometer
  spr.drawLine(thermX - thermW/2, thermTopY, thermX + thermW/2 - 1, thermTopY, fg);
  spr.drawPixel(thermX - thermW/2, thermTopY + 1, fg);
  spr.drawPixel(thermX + thermW/2 - 1, thermTopY + 1, fg);
  
  // Draw mercury bulb at bottom (circle)
  spr.drawCircle(thermX, bulbY, bulbR, fg);
  spr.fillCircle(thermX, bulbY, bulbR - 2, fg);  // Filled inner part (mercury)
  
  // Draw mercury line inside tube (partial fill to show temperature)
  int mercuryH = (int)(thermH * 0.5f);  // 50% filled
  int mercuryTop = thermBotY - mercuryH;
  spr.fillRect(thermX - thermW/2 + 2, mercuryTop, thermW - 4, mercuryH, fg);
  
  // Draw 3 horizontal lines extending left from thermometer
  // These are scale markers on the thermometer
  int lineLen = 5;
  int lineSpacing = (int)(thermH * 0.2f);
  int lineStartX = thermX - thermW/2 - 1;
  
  for (int i = 0; i < 3; i++) {
    int lineY = thermTopY + (int)(thermH * 0.25f) + i * lineSpacing;
    spr.drawLine(lineStartX - lineLen, lineY, lineStartX, lineY, fg);
  }
  
  // Water waves below thermometer
  int waveY1 = bulbY + bulbR + 3;  // First wave line
  int waveY2 = waveY1 + 5;          // Second wave line (underline)
  int waveAmplitude = 2;
  int waveStartX = cx - (int)(w * 0.45f);
  int waveEndX = cx + (int)(w * 0.45f);
  
  // First wavy line - split in the middle where thermometer bulb is
  // Left portion
  for (int x = waveStartX; x < thermX - bulbR - 2; x++) {
    float phase = (x - waveStartX) * 0.4f;
    int waveOffset = (int)(sinf(phase) * waveAmplitude);
    spr.drawPixel(x, waveY1 + waveOffset, fg);
    spr.drawPixel(x, waveY1 + waveOffset + 1, fg);  // 2px thick
  }
  
  // Right portion
  for (int x = thermX + bulbR + 2; x < waveEndX; x++) {
    float phase = (x - waveStartX) * 0.4f;
    int waveOffset = (int)(sinf(phase) * waveAmplitude);
    spr.drawPixel(x, waveY1 + waveOffset, fg);
    spr.drawPixel(x, waveY1 + waveOffset + 1, fg);  // 2px thick
  }
  
  // Second wavy line - continuous underline
  for (int x = waveStartX; x < waveEndX; x++) {
    float phase = (x - waveStartX) * 0.4f + 1.5f;  // Phase offset for variety
    int waveOffset = (int)(sinf(phase) * waveAmplitude);
    spr.drawPixel(x, waveY2 + waveOffset, fg);
    spr.drawPixel(x, waveY2 + waveOffset + 1, fg);  // 2px thick
  }
}

// ============================================================================
// DIAL RENDERING - Custom tick pattern for water temp gauge
// ============================================================================

void drawDialBase(TFT_eSprite &spr) {
  spr.fillSprite(COL_BG);

  // Custom tick pattern:
  // - Major thick tick at start (ANGLE_MIN)
  // - Major thick tick at end (ANGLE_MAX)  
  // - Major thick tick at center
  // - 2 thin ticks clustered 2px (angular) from start tick
  // - 2 thin ticks clustered 2px (angular) from end tick
  // - Arc connecting the tops of clustered ticks at each end

  // Calculate angular spacing for 2px at R_TICK1 radius
  // arc_length = radius * angle_radians, so angle_degrees = (arc_length / radius) * (180/PI)
  float tickSpacingPx = 4.0f;  // 2px spacing between ticks
  float tickSpacingDeg = (tickSpacingPx / (float)R_TICK1) * (180.0f / 3.14159f);

  // Draw major ticks (start, center, end)
  float majorAngles[] = { ANGLE_MIN, (ANGLE_MIN + ANGLE_MAX) / 2.0f, ANGLE_MAX };
  
  for (int m = 0; m < 3; m++) {
    float ang = majorAngles[m];
    int x1, y1, x3, y3;
    polarPoint(CX, CY, R_TICK1, ang, x1, y1);
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
  }

  // Draw clustered thin ticks near start (2 ticks, each 2px spacing inward)
  float startClusterAngles[] = { 
    ANGLE_MIN + tickSpacingDeg, 
    ANGLE_MIN + tickSpacingDeg * 2 
  };
  
  for (int i = 0; i < 2; i++) {
    float ang = startClusterAngles[i];
    int x1, y1, x2, y2;
    polarPoint(CX, CY, R_TICK1, ang, x1, y1);
    polarPoint(CX, CY, R_TICK2, ang, x2, y2);
    spr.drawLine(x1, y1, x2, y2, COL_TICKS);
  }

  // Draw clustered thin ticks near end (2 ticks, each 2px spacing inward)
  float endClusterAngles[] = { 
    ANGLE_MAX - tickSpacingDeg, 
    ANGLE_MAX - tickSpacingDeg * 2 
  };
  
  for (int i = 0; i < 2; i++) {
    float ang = endClusterAngles[i];
    int x1, y1, x2, y2;
    polarPoint(CX, CY, R_TICK1, ang, x1, y1);
    polarPoint(CX, CY, R_TICK2, ang, x2, y2);
    spr.drawLine(x1, y1, x2, y2, COL_TICKS);
  }

  // Draw arc connecting tops of start cluster (from major tick to last cluster tick)
  // Arc follows R_TICK1 radius, ~2px thick
  float arcStartAng = ANGLE_MIN;
  float arcEndAng = ANGLE_MIN + tickSpacingDeg * 2;
  int arcSteps = 20;
  
  for (int s = 0; s < arcSteps; s++) {
    float t = (float)s / (float)(arcSteps - 1);
    float ang = arcStartAng + t * (arcEndAng - arcStartAng);
    int x, y;
    polarPoint(CX, CY, R_TICK1, ang, x, y);
    // Draw 2px thick by drawing at R_TICK1 and R_TICK1-1
    spr.drawPixel(x, y, COL_TICKS);
    int x2, y2;
    polarPoint(CX, CY, R_TICK1 - 1, ang, x2, y2);
    spr.drawPixel(x2, y2, COL_TICKS);
  }

  // Draw arc connecting tops of end cluster
  float arcStartAng2 = ANGLE_MAX - tickSpacingDeg * 2;
  float arcEndAng2 = ANGLE_MAX;
  
  for (int s = 0; s < arcSteps; s++) {
    float t = (float)s / (float)(arcSteps - 1);
    float ang = arcStartAng2 + t * (arcEndAng2 - arcStartAng2);
    int x, y;
    polarPoint(CX, CY, R_TICK1, ang, x, y);
    spr.drawPixel(x, y, COL_TICKS);
    int x2, y2;
    polarPoint(CX, CY, R_TICK1 - 1, ang, x2, y2);
    spr.drawPixel(x2, y2, COL_TICKS);
  }

  // Draw icon
  if (GAUGE_ICON_FUNC != nullptr) {
    GAUGE_ICON_FUNC(spr, CX, CY + ICON_OFFSET_Y, ICON_WIDTH, ICON_HEIGHT, COL_TEXT, COL_DIAL);
  }

  // Draw start/end labels
  spr.setTextColor(COL_TEXT, COL_DIAL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(3);
  
  if (GAUGE_START_LABEL[0] != '\0') {
    spr.drawString(GAUGE_START_LABEL, CX - LABEL_OFFSET_X, CY + LABEL_OFFSET_Y);
  }
  if (GAUGE_END_LABEL[0] != '\0') {
    spr.drawString(GAUGE_END_LABEL, CX + LABEL_OFFSET_X, CY + LABEL_OFFSET_Y);
  }

  // Draw secondary label: "Oil °F"
  spr.setTextSize(1);
  // Draw "Oil " then degree symbol then "F"
  spr.setTextDatum(MC_DATUM);
  
  // Calculate positions for "Oil °F"
  // We'll draw it as a single string with the degree character
  // TFT_eSPI supports extended ASCII, degree symbol is char 176 or we can use °
  char tempLabel[16];
  snprintf(tempLabel, sizeof(tempLabel), "Oil %cF", (char)247);  // 247 is degree symbol in some fonts
  spr.drawString(tempLabel, CX, CY + SECONDARY_LABEL_Y);
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

    // Oil temp simulation (typical range 180-220°F)
    static int ddir = 1;
    digitalValue += ddir;
    if (digitalValue > 220) ddir = -1;
    if (digitalValue < 180) ddir = 1;

    renderGauge(gaugeValue, digitalValue);
  }
}
