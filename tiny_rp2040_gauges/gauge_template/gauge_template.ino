/**
 * Gauge Template
 * 
 * A reusable template for creating analog-style gauges on GC9A01 240x240 displays.
 * 
 * GAUGE-SPECIFIC CONFIGURATION (customize these for each gauge):
 * - GAUGE_ICON_FUNC: Function pointer to draw custom icon (set to nullptr for none)
 * - GAUGE_START_LABEL / GAUGE_END_LABEL: Labels at start/end of arc (e.g., "E"/"F")
 * - GAUGE_SECONDARY_LABEL: Text label below the gauge (e.g., "BOOST")
 * - GAUGE_INTERMEDIATE_TICKS: Number of small tick marks between major ticks (0 to disable)
 */

#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite dial = TFT_eSprite(&tft);

// ============================================================================
// GAUGE-SPECIFIC CONFIGURATION - Customize these for each gauge type
// ============================================================================

// Icon drawing function type: void drawIcon(TFT_eSprite &spr, int cx, int cy, int w, int h, uint16_t fg, uint16_t bg)
typedef void (*IconDrawFunc)(TFT_eSprite&, int, int, int, int, uint16_t, uint16_t);

// Set to your custom icon function, or nullptr for no icon
static IconDrawFunc GAUGE_ICON_FUNC = nullptr;  // Example: &drawGasPumpIcon

// Icon position and size (relative to center)
static const int ICON_OFFSET_Y = -55;  // Negative = above center
static const int ICON_WIDTH = 20;
static const int ICON_HEIGHT = 35;

// Start and end labels (set to "" for no label)
static const char* GAUGE_START_LABEL = "";  // Example: "E" for fuel
static const char* GAUGE_END_LABEL = "";    // Example: "F" for fuel
static const int LABEL_OFFSET_X = 85;       // Distance from center
static const int LABEL_OFFSET_Y = -25;      // Negative = above center

// Secondary label at bottom (set to "" for no label)
static const char* GAUGE_SECONDARY_LABEL = "";  // Example: "BOOST"
static const int SECONDARY_LABEL_Y = 100;       // Offset from center (positive = below)

// Intermediate tick marks between major ticks
// Set to 0 to show NO intermediate ticks (only major ticks at ends and center)
// Set to desired count to show small ticks between major positions
static const int GAUGE_INTERMEDIATE_TICKS = 0;  // Example: 2 for fuel gauge

// ============================================================================
// DISPLAY CONFIGURATION - Usually constant across gauges
// ============================================================================

static const int SCREEN_W = 240;
static const int SCREEN_H = 240;

// ============================================================================
// GAUGE GEOMETRY - May adjust per gauge if needed
// ============================================================================

static const int CX = SCREEN_W / 2;
static const int CY = SCREEN_H / 2;

static const int R_TICK1 = 115;    // tick outer radius
static const int R_TICK2 = 105;    // tick inner radius
static const int R_NEEDLE = 90;    // needle length

// Gauge sweep angles (degrees, 0 = right, positive = CCW)
static const float ANGLE_MIN = -145.0f;  // Start angle (left side)
static const float ANGLE_MAX = -35.0f;   // End angle (right side)

// ============================================================================
// COLORS - Customize palette as needed
// ============================================================================

static const uint16_t COL_BG      = TFT_BLACK;
static const uint16_t COL_RING    = TFT_DARKGREY;
static const uint16_t COL_DIAL    = TFT_BLACK;
static const uint16_t COL_TICKS   = TFT_WHITE;
static const uint16_t COL_TEXT    = TFT_WHITE;
static const uint16_t COL_NEEDLE  = TFT_WHITE;
static const uint16_t COL_ACCENT  = TFT_CYAN;  // For digital readout

// ============================================================================
// STATE VARIABLES
// ============================================================================

float gaugeValue = 0.5f;   // 0.0..1.0 (needle position)
int   digitalValue = 0;    // Digital readout value

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
// EXAMPLE ICON: Gas Pump (copy this pattern for custom icons)
// ============================================================================

void drawGasPumpIcon(TFT_eSprite &spr, int cx, int cy, int w, int h,
                     uint16_t fg, uint16_t bg)
{
  // Body
  int x = cx - w / 2;
  int y = cy - h / 2;

  int r = max(2, w / 8);  // corner radius
  spr.fillRoundRect(x, y, w, h, r, fg);

  // Inner cut (gives a subtle outline look)
  int inset = max(2, w / 10);
  spr.fillRoundRect(x + inset, y + inset, w - 2*inset, h - 2*inset, max(1, r-1), bg);

  // "Screen" window (top portion)
  int winW = (int)(w * 0.55f);
  int winH = (int)(h * 0.22f);
  int winX = cx - winW / 2;
  int winY = y + (int)(h * 0.18f);
  spr.drawRoundRect(winX, winY, winW, winH, max(1, r-2), fg);

  // Separator line
  int sepY = y + (int)(h * 0.55f);
  spr.drawLine(x + inset, sepY, x + w - inset - 1, sepY, fg);

  // Little base foot
  int footH = max(2, h / 10);
  spr.fillRect(x + (int)(w * 0.18f), y + h - footH - inset, (int)(w * 0.64f), footH, fg);

  // Hose on right side
  int hoseX = x + w - inset - 1;
  int hoseTop = winY + 1;
  int hoseMid = sepY;
  spr.drawLine(hoseX, hoseTop, hoseX + w/6, hoseTop + h/6, fg);
  spr.drawLine(hoseX + w/6, hoseTop + h/6, hoseX + w/6, hoseMid, fg);

  // Nozzle at end of hose
  int nozX = hoseX + w/6;
  int nozY = hoseMid;
  spr.drawLine(nozX, nozY, nozX + w/8, nozY + h/10, fg);
  spr.drawLine(nozX + w/8, nozY + h/10, nozX + w/8, nozY + h/6, fg);
}

// ============================================================================
// DIAL RENDERING
// ============================================================================

void drawDialBase(TFT_eSprite &spr) {
  spr.fillSprite(COL_BG);

  // Calculate total tick count based on intermediate ticks
  // Always have 3 major ticks: start, center, end
  // Intermediate ticks fill the gaps
  const int majorTicks = 3;
  const int totalTicks = majorTicks + (GAUGE_INTERMEDIATE_TICKS * 2);  // 2 gaps between 3 major ticks

  for (int i = 0; i < totalTicks; i++) {
    float t = (float)i / (float)(totalTicks - 1);
    float ang = ANGLE_MIN + t * (ANGLE_MAX - ANGLE_MIN);

    int x1, y1, x2, y2;
    polarPoint(CX, CY, R_TICK1, ang, x1, y1);
    polarPoint(CX, CY, R_TICK2, ang, x2, y2);

    // Determine if this is a major tick (first, middle, last)
    bool isMajorTick = (i == 0) || (i == totalTicks - 1) || (i == totalTicks / 2);

    if (isMajorTick) {
      // Draw thick major tick
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
      // Draw thin intermediate tick
      spr.drawLine(x1, y1, x2, y2, COL_TICKS);
    }
  }

  // Draw icon if configured
  if (GAUGE_ICON_FUNC != nullptr) {
    GAUGE_ICON_FUNC(spr, CX, CY + ICON_OFFSET_Y, ICON_WIDTH, ICON_HEIGHT, COL_TEXT, COL_DIAL);
  }

  // Draw start/end labels if configured
  spr.setTextColor(COL_TEXT, COL_DIAL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(3);
  
  if (GAUGE_START_LABEL[0] != '\0') {
    spr.drawString(GAUGE_START_LABEL, CX - LABEL_OFFSET_X, CY + LABEL_OFFSET_Y);
  }
  if (GAUGE_END_LABEL[0] != '\0') {
    spr.drawString(GAUGE_END_LABEL, CX + LABEL_OFFSET_X, CY + LABEL_OFFSET_Y);
  }

  // Draw secondary label if configured
  if (GAUGE_SECONDARY_LABEL[0] != '\0') {
    spr.setTextSize(1);
    spr.drawString(GAUGE_SECONDARY_LABEL, CX, CY + SECONDARY_LABEL_Y);
  }
}

void drawNeedleCone(TFT_eSprite &spr, float value01) {
  value01 = constrain(value01, 0.0f, 1.0f);

  float angDeg = ANGLE_MIN + value01 * (ANGLE_MAX - ANGLE_MIN);
  float a = deg2rad(angDeg);

  // Tip position
  int xTip, yTip;
  polarPoint(CX, CY, R_NEEDLE, angDeg, xTip, yTip);

  // Normal (perpendicular) unit vector
  float nx = -sinf(a);
  float ny = cosf(a);

  // Widths in pixels
  const float baseW = 5.0f;
  const float tipW = 3.0f;

  // Half-width offsets
  float b = baseW * 0.5f;
  float t = tipW * 0.5f;

  // Define trapezoid corners
  int xBL = CX + (int)lroundf(nx * b);
  int yBL = CY + (int)lroundf(ny * b);
  int xBR = CX - (int)lroundf(nx * b);
  int yBR = CY - (int)lroundf(ny * b);

  int xTL = xTip + (int)lroundf(nx * t);
  int yTL = yTip + (int)lroundf(ny * t);
  int xTR = xTip - (int)lroundf(nx * t);
  int yTR = yTip - (int)lroundf(ny * t);

  // Fill trapezoid as two triangles
  spr.fillTriangle(xBL, yBL, xBR, yBR, xTL, yTL, COL_NEEDLE);
  spr.fillTriangle(xBR, yBR, xTR, yTR, xTL, yTL, COL_NEEDLE);

  // Center hub
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
  // Demo animation
  static uint32_t last = 0;
  if (millis() - last > 40) {
    last = millis();

    static float dir = 0.006f;
    gaugeValue += dir;
    if (gaugeValue >= 1.0f) { gaugeValue = 1.0f; dir = -dir; }
    if (gaugeValue <= 0.0f) { gaugeValue = 0.0f; dir = -dir; }

    static int ddir = 1;
    digitalValue += ddir;
    if (digitalValue > 10) ddir = -1;
    if (digitalValue < -21) ddir = 1;

    renderGauge(gaugeValue, digitalValue);
  }
}
