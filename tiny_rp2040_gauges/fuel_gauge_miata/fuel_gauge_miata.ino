#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite dial = TFT_eSprite(&tft);

// --------- Adjust for your display ----------
static const int SCREEN_W = 240;
static const int SCREEN_H = 240;

// --------- Gauge geometry ----------
static const int CX = SCREEN_W / 2;
static const int CY = SCREEN_H / 2;

static const int R_TICK1 = 115;    // tick outer
static const int R_TICK2 = 105;    // tick inner
static const int R_NEEDLE = 90;   // needle length

// Fuel sweep: left to right
// (matches the look: needle pivots upward-ish with a moderate sweep)
static const float ANGLE_MIN = -145.0f; // "E" side
static const float ANGLE_MAX =  -35.0f; // "F" side

// --------- Colors ----------
static const uint16_t COL_BG      = TFT_BLACK;
static const uint16_t COL_RING    = TFT_DARKGREY;
static const uint16_t COL_DIAL    = TFT_BLACK;     // inside is near-black
static const uint16_t COL_TICKS   = TFT_WHITE;
static const uint16_t COL_TEXT    = TFT_WHITE;
static const uint16_t COL_NEEDLE  = TFT_WHITE;
static const uint16_t COL_BLUE    = TFT_CYAN;      // tweak if you want deeper blue

// --------- State for erasing needle if you skip sprite (we use sprite) ----------
float fuel = 0.5f;   // 0.0..1.0
int   boost = -21;   // example: -21 vacuum

// Convert degrees to radians
static inline float deg2rad(float d) { return d * 0.017453292519943295f; }

// Point on circle at angle degrees (0 deg points right; positive CCW)
static inline void polarPoint(int cx, int cy, int r, float angDeg, int &x, int &y) {
  float a = deg2rad(angDeg);
  x = cx + (int)lroundf(cosf(a) * r);
  y = cy + (int)lroundf(sinf(a) * r);
}

// Draw a simple gas pump icon centered at (cx, cy)
// w/h control overall size; colors can be tweaked
void drawGasPumpIcon(TFT_eSprite &spr, int cx, int cy, int w, int h,
                     uint16_t fg, uint16_t bg)
{
  // Body
  int x = cx - w / 2;
  int y = cy - h / 2;

  int r = max(2, w / 8);              // corner radius
  spr.fillRoundRect(x, y, w, h, r, fg);

  // Inner cut (gives a subtle outline look if bg != fg)
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
  spr.drawLine(hoseX, hoseTop, hoseX + w/6, hoseTop + h/6, fg);  // small bend
  spr.drawLine(hoseX + w/6, hoseTop + h/6, hoseX + w/6, hoseMid, fg);

  // Nozzle at end of hose
  int nozX = hoseX + w/6;
  int nozY = hoseMid;
  spr.drawLine(nozX, nozY, nozX + w/8, nozY + h/10, fg);
  spr.drawLine(nozX + w/8, nozY + h/10, nozX + w/8, nozY + h/6, fg);
}

void drawDialBase(TFT_eSprite &spr) {
  spr.fillSprite(COL_BG);

  // Tick marks along upper arc
  const int tickCount = 7; // similar density to the screenshot
  for (int i = 0; i < tickCount; i++) {
    float t = (float)i / (float)(tickCount - 1);
    float ang = ANGLE_MIN + t * (ANGLE_MAX - ANGLE_MIN);

    int x1, y1, x2, y2;
    polarPoint(CX, CY, R_TICK1, ang, x1, y1);
    polarPoint(CX, CY, R_TICK2, ang, x2, y2);

    // Make center ticks slightly longer (optional)
    if (i == tickCount / 2 || i == 0 || i == tickCount - 1) {
      int x3, y3;
      polarPoint(CX, CY, R_TICK2 - 6, ang, x3, y3);

      float a = deg2rad(ang);
      float nx = -sinf(a);   // normal vector
      float ny =  cosf(a);

      int thickness = 8;     // change this for thicker/thinner

      for (int t = -thickness/2; t <= thickness/2; t++) {
        int ox = nx * t;
        int oy = ny * t;

        spr.drawLine(
          x1 + ox, y1 + oy,
          x3 + ox, y3 + oy,
          COL_TICKS
        );
      }
    } else if (i != 2 && i != 4) {
      spr.drawLine(x1, y1, x2, y2, COL_TICKS);
    }
  }

  // Title text: FUEL
  spr.setTextColor(COL_TEXT, COL_DIAL);
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(2);
  // spr.drawString("FUEL", CX, CY - 60);
  drawGasPumpIcon(spr, CX, CY - 55, 20, 35, COL_TEXT, COL_DIAL);

  // E and F labels near bottom left/right
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(3);
  spr.drawString("E", CX - 85, CY - 25);
  spr.drawString("F", CX + 85, CY - 25);

  // Boost label at the bottom
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(1);
  spr.drawString("BOOST", CX, CY + 100);
}

void drawNeedleCone(TFT_eSprite &spr, float fuel01) {
  fuel01 = constrain(fuel01, 0.0f, 1.0f);

  float angDeg = ANGLE_MIN + fuel01 * (ANGLE_MAX - ANGLE_MIN);
  float a = deg2rad(angDeg);

  // Tip position
  int xTip, yTip;
  polarPoint(CX, CY, R_NEEDLE, angDeg, xTip, yTip);

  // Normal (perpendicular) unit-ish vector
  float nx = -sinf(a);
  float ny =  cosf(a);

  // Widths in pixels (total width)
  const float baseW = 5.0f;  // ~5 px at base
  const float tipW  = 3.0f;  // ~3 px at tip

  // Half-width offsets
  float b = baseW * 0.5f;
  float t = tipW  * 0.5f;

  // Define trapezoid corners:
  // Base left/right around (CX,CY)
  int xBL = CX + (int)lroundf(nx * b);
  int yBL = CY + (int)lroundf(ny * b);
  int xBR = CX - (int)lroundf(nx * b);
  int yBR = CY - (int)lroundf(ny * b);

  // Tip left/right around (xTip,yTip)
  int xTL = xTip + (int)lroundf(nx * t);
  int yTL = yTip + (int)lroundf(ny * t);
  int xTR = xTip - (int)lroundf(nx * t);
  int yTR = yTip - (int)lroundf(ny * t);

  // Fill trapezoid as two triangles
  spr.fillTriangle(xBL, yBL, xBR, yBR, xTL, yTL, COL_NEEDLE);
  spr.fillTriangle(xBR, yBR, xTR, yTR, xTL, yTL, COL_NEEDLE);

  // Optional: sharpen/brighten outline (comment out if you want softer)
  // spr.drawLine(xBL, yBL, xTL, yTL, COL_NEEDLE);
  // spr.drawLine(xBR, yBR, xTR, yTR, COL_NEEDLE);

  // Center hub on top
  spr.fillCircle(CX, CY, 20, TFT_DARKGREY);
  spr.fillCircle(CX, CY, 19, TFT_BLACK);
}

void drawBoostReadout(TFT_eSprite &spr, int boostValue) {
  // Blue digital number centered below hub (like screenshot)
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(COL_BLUE, COL_DIAL);

  // Make it bigger than labels
  spr.setTextSize(2);

  char buf[12];
  // show sign like "-21" or "+3"
  spr.setTextSize(3);
  snprintf(buf, sizeof(buf), "%d", boostValue);
  spr.drawString(buf, CX, CY + 65);
}

void renderGauge(float fuel01, int boostValue) {
  // Draw base each frame (simple + flicker free via sprite).
  // If you need higher FPS, you can keep a separate "static" sprite and copy it.
  drawDialBase(dial);
  drawNeedleCone(dial, fuel01);
  drawBoostReadout(dial, boostValue);

  dial.pushSprite(0, 0);
}

void setup() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(COL_BG);

  dial.setColorDepth(16);
  dial.createSprite(SCREEN_W, SCREEN_H);
  dial.fillSprite(COL_BG);

  renderGauge(fuel, boost);
}

void loop() {
  // Demo animation: fuel swings, boost oscillates negative
  static uint32_t last = 0;
  if (millis() - last > 40) {
    last = millis();

    static float dir = 0.006f;
    fuel += dir;
    if (fuel >= 1.0f) { fuel = 1.0f; dir = -dir; }
    if (fuel <= 0.0f) { fuel = 0.0f; dir = -dir; }

    // Fake vacuum/boost number
    static int bdir = 1;
    boost += bdir;
    if (boost > -5)  bdir = -1;
    if (boost < -21) bdir = 1;

    renderGauge(fuel, boost);
  }
}
