/**
 * Gauge Cluster - Multi-Display Firmware
 * 
 * Drives 3x GC9A01 240x240 displays via SPI with manual CS control:
 *   GPIO5 = Fuel Gauge
 *   GPIO6 = Oil Pressure Gauge
 *   GPIO7 = Water Temperature Gauge
 * 
 * NOTE: GPIO4 is avoided - it's SPI0 MISO and conflicts with SPI peripheral
 * 
 * Hardware: Pimoroni Tiny RP2040
 * 
 * IMPORTANT: Copy User_Setup.h to your TFT_eSPI library folder,
 *            or include it via User_Setup_Select.h
 */

#include <TFT_eSPI.h>
#include <SPI.h>

// ============================================================================
// DISPLAY CS PIN DEFINITIONS
// ============================================================================

#define CS_FUEL      5   // Fuel gauge CS
#define CS_OIL       6   // Oil pressure gauge CS
#define CS_WATER     7   // Water temperature gauge CS

// ============================================================================
// DISPLAY & SPRITE INSTANCES
// ============================================================================

TFT_eSPI tft = TFT_eSPI();  // Single TFT instance for all displays

// Full gauge sprite - for needle updates (115KB)
TFT_eSprite gauge = TFT_eSprite(&tft);

// Small sprite for digital readout only - for fast number updates (~4KB)
// Positioned at center bottom of gauge
static const int READOUT_W = 70;
static const int READOUT_H = 30;
static const int READOUT_X = (240 - READOUT_W) / 2;  // Centered
static const int READOUT_Y = 120 + 65 - READOUT_H/2; // CY + 65 - half height
TFT_eSprite readout = TFT_eSprite(&tft);

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================

static const int SCREEN_W = 240;
static const int SCREEN_H = 240;

// ============================================================================
// GAUGE GEOMETRY (shared)
// ============================================================================

static const int CX = SCREEN_W / 2;
static const int CY = SCREEN_H / 2;

static const int R_TICK1 = 115;    // tick outer radius
static const int R_TICK2 = 105;    // tick inner radius
static const int R_NEEDLE = 90;    // needle length

static const float ANGLE_MIN = -145.0f;
static const float ANGLE_MAX = -35.0f;

// ============================================================================
// COLORS (shared)
// ============================================================================

static const uint16_t COL_BG      = TFT_BLACK;
static const uint16_t COL_DIAL    = TFT_BLACK;
static const uint16_t COL_TICKS   = TFT_WHITE;
static const uint16_t COL_TEXT    = TFT_WHITE;
static const uint16_t COL_NEEDLE  = TFT_WHITE;
static const uint16_t COL_ACCENT  = TFT_CYAN;

// ============================================================================
// GAUGE STATE
// ============================================================================

// Fuel gauge
float fuelLevel = 0.5f;      // 0.0..1.0
int   boostValue = -50;      // Vacuum/boost reading (-50 to 30)

// Oil pressure gauge
float oilPressure = 0.5f;    // 0.0..1.0
float afrValue = 14.0f;      // AFR reading (0.0 to 20.0)

// Water temperature gauge
float waterTemp = 0.5f;      // 0.0..1.0
int   oilTempValue = 50;     // Oil temp in °F (50 to 250)

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
// DISPLAY SELECTION (manual CS control)
// ============================================================================

void deselectAllDisplays() {
  digitalWrite(CS_FUEL, HIGH);
  digitalWrite(CS_OIL, HIGH);
  digitalWrite(CS_WATER, HIGH);
}

void selectAllDisplays() {
  digitalWrite(CS_FUEL, LOW);
  digitalWrite(CS_OIL, LOW);
  digitalWrite(CS_WATER, LOW);
}

// Push sprite to a specific display with proper SPI transaction handling
void pushSpriteToDisplay(uint8_t cs_pin, TFT_eSprite &sprite) {
  deselectAllDisplays();          // Ensure all displays are deselected first
  digitalWrite(cs_pin, LOW);      // Select target display
  tft.startWrite();               // Begin SPI transaction
  sprite.pushSprite(0, 0);        // Push sprite data
  tft.endWrite();                 // End SPI transaction
  digitalWrite(cs_pin, HIGH);     // Deselect display
}

// ============================================================================
// ICON DRAWING FUNCTIONS
// ============================================================================

void drawGasPumpIcon(TFT_eSprite &spr, int cx, int cy, int w, int h,
                     uint16_t fg, uint16_t bg)
{
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

void drawOilCanIcon(TFT_eSprite &spr, int cx, int cy, int w, int h,
                    uint16_t fg, uint16_t bg)
{
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

void drawThermometerWaterIcon(TFT_eSprite &spr, int cx, int cy, int w, int h,
                               uint16_t fg, uint16_t bg)
{
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
  int lineStartX = thermX + thermW/2 + 1;  // Right side of thermometer
  
  for (int i = 0; i < 3; i++) {
    int lineY = thermTopY + (int)(thermH * 0.25f) + i * lineSpacing;
    spr.drawLine(lineStartX, lineY, lineStartX + lineLen, lineY, fg);
  }
  
  int waveY1 = bulbY + bulbR + 3;
  int waveY2 = waveY1 + 5;
  int waveAmplitude = 2;
  int waveStartX = cx - (int)(w * 0.75f);  // 2/3 wider (was 0.45)
  int waveEndX = cx + (int)(w * 0.75f);    // 2/3 wider (was 0.45)
  
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
  
  for (int x = waveStartX; x < waveEndX; x++) {
    float phase = (x - waveStartX) * 0.4f + 1.5f;
    int waveOffset = (int)(sinf(phase) * waveAmplitude);
    spr.drawPixel(x, waveY2 + waveOffset, fg);
    spr.drawPixel(x, waveY2 + waveOffset + 1, fg);
  }
}

// ============================================================================
// NEEDLE DRAWING (shared)
// ============================================================================

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

void drawDigitalReadoutFloat(TFT_eSprite &spr, float value) {
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(COL_ACCENT, COL_DIAL);
  spr.setTextSize(3);

  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f", value);
  spr.drawString(buf, CX, CY + 65);
}

// ============================================================================
// FAST READOUT-ONLY UPDATE (small sprite pushed to number region)
// ============================================================================

void pushReadoutInt(uint8_t cs_pin, int value) {
  readout.fillSprite(COL_BG);
  readout.setTextDatum(MC_DATUM);
  readout.setTextColor(COL_ACCENT, COL_BG);
  readout.setTextSize(3);
  
  char buf[12];
  snprintf(buf, sizeof(buf), "%d", value);
  readout.drawString(buf, READOUT_W/2, READOUT_H/2);
  
  deselectAllDisplays();
  digitalWrite(cs_pin, LOW);
  tft.startWrite();
  readout.pushSprite(READOUT_X, READOUT_Y);
  tft.endWrite();
  digitalWrite(cs_pin, HIGH);
}

void pushReadoutFloat(uint8_t cs_pin, float value) {
  readout.fillSprite(COL_BG);
  readout.setTextDatum(MC_DATUM);
  readout.setTextColor(COL_ACCENT, COL_BG);
  readout.setTextSize(3);
  
  char buf[12];
  snprintf(buf, sizeof(buf), "%.1f", value);
  readout.drawString(buf, READOUT_W/2, READOUT_H/2);
  
  deselectAllDisplays();
  digitalWrite(cs_pin, LOW);
  tft.startWrite();
  readout.pushSprite(READOUT_X, READOUT_Y);
  tft.endWrite();
  digitalWrite(cs_pin, HIGH);
}

// ============================================================================
// FUEL GAUGE RENDERING
// ============================================================================

void drawFuelDialBase(TFT_eSprite &spr) {
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

void renderFuelGauge(TFT_eSprite &spr, float value, int digital) {
  drawFuelDialBase(spr);
  drawNeedleCone(spr, value);
  drawDigitalReadout(spr, digital);
}

// ============================================================================
// OIL PRESSURE GAUGE RENDERING
// ============================================================================

void drawOilDialBase(TFT_eSprite &spr) {
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

void renderOilGauge(TFT_eSprite &spr, float value, float digital) {
  drawOilDialBase(spr);
  drawNeedleCone(spr, value);
  drawDigitalReadoutFloat(spr, digital);
}

// ============================================================================
// WATER TEMPERATURE GAUGE RENDERING
// ============================================================================

void drawWaterDialBase(TFT_eSprite &spr) {
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

void renderWaterGauge(TFT_eSprite &spr, float value, int digital) {
  drawWaterDialBase(spr);
  drawNeedleCone(spr, value);
  drawDigitalReadout(spr, digital);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  // Configure CS pins as outputs
  pinMode(CS_FUEL, OUTPUT);
  pinMode(CS_OIL, OUTPUT);
  pinMode(CS_WATER, OUTPUT);
  
  // Deselect all displays initially
  deselectAllDisplays();
  
  // Initialize all displays simultaneously
  selectAllDisplays();
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(COL_BG);
  deselectAllDisplays();
  
  // Create full gauge sprite (for needle updates)
  gauge.setColorDepth(16);
  gauge.createSprite(SCREEN_W, SCREEN_H);
  
  // Create small readout sprite (for fast number updates)
  readout.setColorDepth(16);
  readout.createSprite(READOUT_W, READOUT_H);
  
  // Initial full render of all gauges
  renderFuelGauge(gauge, fuelLevel, boostValue);
  pushSpriteToDisplay(CS_FUEL, gauge);
  
  renderOilGauge(gauge, oilPressure, afrValue);
  pushSpriteToDisplay(CS_OIL, gauge);
  
  renderWaterGauge(gauge, waterTemp, oilTempValue);
  pushSpriteToDisplay(CS_WATER, gauge);
}

// ============================================================================
// MAIN LOOP - Dual update strategy:
//   - Full gauge redraw: Only when needle moves significantly (every ~100ms)
//   - Number update: Every frame for low latency
// ============================================================================

// Track previous needle positions to detect significant movement
static float prevFuelLevel = -1;
static float prevOilPressure = -1;
static float prevWaterTemp = -1;

// Threshold for needle redraw (0.01 = 1% movement)
static const float NEEDLE_THRESHOLD = 0.01f;

// Timing
static uint32_t lastFrameTime = 0;
static uint32_t frameCount = 0;

void loop() {
  uint32_t now = millis();
  uint32_t deltaMs = now - lastFrameTime;
  lastFrameTime = now;
  
  // Scale animation by time delta for consistent speed
  float deltaScale = deltaMs / 40.0f;
  
  // ---- Update demo values ----
  // Needle values change VERY SLOWLY, number values change fast
  
  // Fuel gauge - needle moves very slowly
  static float fuelDir = 1.0f;
  fuelLevel += 0.0003f * fuelDir * deltaScale;  // ~50x slower
  if (fuelLevel >= 1.0f) { fuelLevel = 1.0f; fuelDir = -1.0f; }
  if (fuelLevel <= 0.0f) { fuelLevel = 0.0f; fuelDir = 1.0f; }
  
  // Boost number changes fast
  static float boostDir = 1.0f;
  boostValue += (int)(1.0f * boostDir * deltaScale);
  if (boostValue > 30) boostDir = -1.0f;
  if (boostValue < -50) boostDir = 1.0f;
  
  // Oil gauge - needle moves very slowly
  static float oilDir = 1.0f;
  oilPressure += 0.0004f * oilDir * deltaScale;  // ~50x slower
  if (oilPressure >= 1.0f) { oilPressure = 1.0f; oilDir = -1.0f; }
  if (oilPressure <= 0.0f) { oilPressure = 0.0f; oilDir = 1.0f; }
  
  // AFR number changes fast
  static float afrDir = 1.0f;
  afrValue += 0.1f * afrDir * deltaScale;
  if (afrValue > 20.0f) afrDir = -1.0f;
  if (afrValue < 0.0f) afrDir = 1.0f;
  
  // Water gauge - needle moves very slowly
  static float waterDir = 1.0f;
  waterTemp += 0.00025f * waterDir * deltaScale;  // ~50x slower
  if (waterTemp >= 1.0f) { waterTemp = 1.0f; waterDir = -1.0f; }
  if (waterTemp <= 0.0f) { waterTemp = 0.0f; waterDir = 1.0f; }
  
  // Oil temp number changes fast
  static float oilTempDir = 1.0f;
  oilTempValue += (int)(2.0f * oilTempDir * deltaScale);
  if (oilTempValue > 250) oilTempDir = -1.0f;
  if (oilTempValue < 50) oilTempDir = 1.0f;
  
  // ---- FUEL GAUGE ----
  if (fabsf(fuelLevel - prevFuelLevel) > NEEDLE_THRESHOLD) {
    // Needle moved significantly - full redraw
    renderFuelGauge(gauge, fuelLevel, boostValue);
    pushSpriteToDisplay(CS_FUEL, gauge);
    prevFuelLevel = fuelLevel;
  } else {
    // Just update the number (fast)
    pushReadoutInt(CS_FUEL, boostValue);
  }
  
  // ---- OIL PRESSURE GAUGE ----
  if (fabsf(oilPressure - prevOilPressure) > NEEDLE_THRESHOLD) {
    renderOilGauge(gauge, oilPressure, afrValue);
    pushSpriteToDisplay(CS_OIL, gauge);
    prevOilPressure = oilPressure;
  } else {
    pushReadoutFloat(CS_OIL, afrValue);
  }
  
  // ---- WATER TEMP GAUGE ----
  if (fabsf(waterTemp - prevWaterTemp) > NEEDLE_THRESHOLD) {
    renderWaterGauge(gauge, waterTemp, oilTempValue);
    pushSpriteToDisplay(CS_WATER, gauge);
    prevWaterTemp = waterTemp;
  } else {
    pushReadoutInt(CS_WATER, oilTempValue);
  }
  
  frameCount++;
}
