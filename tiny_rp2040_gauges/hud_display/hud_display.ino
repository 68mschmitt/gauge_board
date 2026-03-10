/**
 * HUD Display - Oil Pressure & AFR
 * 
 * Displays oil pressure gauge and AFR (air-fuel ratio) on an ST7735S 160x80 RGB TFT.
 * Data will be transmitted via CAN bus to ESP32 gauge displays.
 * 
 * Hardware:
 *   - Pimoroni Tiny RP2040
 *   - ST7735S 160x80 RGB TFT Display
 *   - Future: CAN bus module for data transmission
 * 
 * Protocol: HUD:<oil_pressure_psi>:<afr_value>
 * Example:  HUD:45.0:14.7
 * 
 * The data displayed here will be transmitted to:
 *   - ESP32 Oil Pressure Gauge (esp32s3_gauges/oil_pressure_gauge/)
 * 
 * IMPORTANT: Copy User_Setup.h to your TFT_eSPI library folder
 */

#include <TFT_eSPI.h>
#include <SPI.h>

// ============================================================================
// FORWARD DECLARATIONS (required for Arduino preprocessor)
// ============================================================================

enum GaugeStatus { STATUS_OK, STATUS_WARNING, STATUS_CAUTION };

GaugeStatus getOilPressureStatus(float psi);
GaugeStatus getAFRStatus(float afr);
uint16_t statusToColor(GaugeStatus status);
void parseHUDMessage(const char* msg);

// ============================================================================
// DISPLAY CONFIGURATION
// ============================================================================

TFT_eSPI tft = TFT_eSPI();
TFT_eSprite hud = TFT_eSprite(&tft);

static const int SCREEN_W = 128;
static const int SCREEN_H = 128;

// ============================================================================
// COLORS
// ============================================================================

static const uint16_t COL_BG         = TFT_BLACK;
static const uint16_t COL_TEXT       = TFT_WHITE;
static const uint16_t COL_VALUE      = TFT_CYAN;
static const uint16_t COL_LABEL      = TFT_DARKGREY;
static const uint16_t COL_WARNING    = TFT_RED;
static const uint16_t COL_CAUTION    = TFT_YELLOW;
static const uint16_t COL_GOOD       = TFT_GREEN;
static const uint16_t COL_DIVIDER    = 0x4208;  // Dark gray for divider line

// ============================================================================
// GAUGE DATA STRUCTURE
// ============================================================================

struct HUDData {
  // Oil Pressure
  float oilPressure;      // PSI (0-100)
  float oilPressureMin;   // Warning threshold low
  float oilPressureMax;   // Warning threshold high
  
  // AFR (Air-Fuel Ratio)
  float afr;              // AFR value (typically 10-18)
  float afrMin;           // Lean limit
  float afrMax;           // Rich limit
  float afrTarget;        // Stoichiometric target (14.7 for gasoline)
  
  bool updated;
};

HUDData hudData = {
  .oilPressure = 45.0f,
  .oilPressureMin = 15.0f,
  .oilPressureMax = 80.0f,
  .afr = 14.7f,
  .afrMin = 10.0f,
  .afrMax = 18.0f,
  .afrTarget = 14.7f,
  .updated = true
};

// ============================================================================
// CANBUS DATA STRUCTURE (for future transmission)
// ============================================================================

// CAN message IDs for gauge cluster
#define CAN_ID_OIL_AFR    0x300   // Oil pressure + AFR data

// Data format for CAN transmission (8 bytes):
// Byte 0-1: Oil pressure (uint16, PSI * 10)
// Byte 2-3: AFR (uint16, value * 100)
// Byte 4:   Oil pressure status (0=OK, 1=LOW, 2=HIGH)
// Byte 5:   AFR status (0=OK, 1=LEAN, 2=RICH)
// Byte 6-7: Reserved

struct CANOilAFRMessage {
  uint16_t oilPressure_x10;
  uint16_t afr_x100;
  uint8_t  oilStatus;
  uint8_t  afrStatus;
  uint16_t reserved;
};

// ============================================================================
// UART PROTOCOL FOR RECEIVING DATA
// ============================================================================

#define UART_BUFFER_SIZE 64
char uartBuffer[UART_BUFFER_SIZE];
int bufferIndex = 0;

void processUARTInput() {
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (bufferIndex > 0) {
        uartBuffer[bufferIndex] = '\0';
        parseHUDMessage(uartBuffer);
        bufferIndex = 0;
      }
    } else if (bufferIndex < UART_BUFFER_SIZE - 1) {
      uartBuffer[bufferIndex++] = c;
    }
  }
}

void parseHUDMessage(const char* msg) {
  // Format: HUD:<oil_psi>:<afr>
  // Example: HUD:45.0:14.7
  
  char id[8];
  float oilPsi, afr;
  
  if (sscanf(msg, "%7[^:]:%f:%f", id, &oilPsi, &afr) == 3) {
    if (strcmp(id, "HUD") == 0) {
      hudData.oilPressure = constrain(oilPsi, 0.0f, 100.0f);
      hudData.afr = constrain(afr, 5.0f, 25.0f);
      hudData.updated = true;
      
      Serial.printf("Received: Oil=%.1f PSI, AFR=%.2f\n", 
                    hudData.oilPressure, hudData.afr);
    }
  }
}

// ============================================================================
// STATUS DETERMINATION
// ============================================================================

GaugeStatus getOilPressureStatus(float psi) {
  if (psi < hudData.oilPressureMin) return STATUS_WARNING;  // Too low - danger!
  if (psi > hudData.oilPressureMax) return STATUS_CAUTION;  // Too high - caution
  return STATUS_OK;
}

GaugeStatus getAFRStatus(float afr) {
  float deviation = fabsf(afr - hudData.afrTarget);
  if (deviation > 2.0f) return STATUS_WARNING;  // Far from stoich
  if (deviation > 1.0f) return STATUS_CAUTION;  // Slightly off
  return STATUS_OK;
}

uint16_t statusToColor(GaugeStatus status) {
  switch (status) {
    case STATUS_WARNING: return COL_WARNING;
    case STATUS_CAUTION: return COL_CAUTION;
    default: return COL_GOOD;
  }
}

// ============================================================================
// HUD RENDERING
// ============================================================================

void drawOilPressureIcon(TFT_eSprite &spr, int x, int y, uint16_t color) {
  // Simplified oil can icon (scaled down for small display)
  int w = 16, h = 12;
  
  // Oil can body
  spr.fillEllipse(x + w/2, y + h/2 + 2, w/2 - 2, h/3, color);
  
  // Spout
  spr.drawLine(x + 2, y + h/2, x - 2, y + 2, color);
  spr.drawLine(x - 2, y + 2, x - 4, y + 4, color);
  
  // Drop
  spr.fillCircle(x - 5, y + 8, 2, color);
}

void drawAFRIcon(TFT_eSprite &spr, int x, int y, uint16_t color) {
  // Lambda symbol (simplified)
  int h = 12;
  
  // Draw a simple "A/F" indicator
  spr.setTextColor(color, COL_BG);
  spr.setTextSize(1);
  spr.setTextDatum(TL_DATUM);
  spr.drawString("A/F", x, y);
}

void drawHorizontalBar(TFT_eSprite &spr, int x, int y, int w, int h, 
                       float value, float minVal, float maxVal, 
                       float targetVal, uint16_t barColor) {
  // Background bar outline
  spr.drawRect(x, y, w, h, COL_LABEL);
  
  // Calculate fill width
  float normalized = (value - minVal) / (maxVal - minVal);
  normalized = constrain(normalized, 0.0f, 1.0f);
  int fillW = (int)(normalized * (w - 2));
  
  // Fill bar
  if (fillW > 0) {
    spr.fillRect(x + 1, y + 1, fillW, h - 2, barColor);
  }
  
  // Target marker (if applicable)
  if (targetVal >= minVal && targetVal <= maxVal) {
    float targetNorm = (targetVal - minVal) / (maxVal - minVal);
    int targetX = x + 1 + (int)(targetNorm * (w - 2));
    spr.drawLine(targetX, y, targetX, y + h - 1, COL_TEXT);
  }
}

void renderHUD() {
  hud.fillSprite(COL_BG);
  
  // Layout for 128x128 square display
  // Top half: Oil Pressure (0-60)
  // Bottom half: AFR (64-128)
  
  // ========================================
  // TOP SECTION: Oil Pressure (y: 0-60)
  // ========================================
  
  GaugeStatus oilStatus = getOilPressureStatus(hudData.oilPressure);
  uint16_t oilColor = statusToColor(oilStatus);
  
  // Label
  hud.setTextColor(COL_LABEL, COL_BG);
  hud.setTextDatum(TL_DATUM);
  hud.setTextSize(1);
  hud.drawString("OIL PSI", 4, 4);
  
  // Value (large) - centered
  hud.setTextColor(oilColor, COL_BG);
  hud.setTextDatum(MC_DATUM);
  hud.setTextSize(4);
  
  char oilBuf[8];
  snprintf(oilBuf, sizeof(oilBuf), "%.0f", hudData.oilPressure);
  hud.drawString(oilBuf, SCREEN_W / 2, 32);
  
  // Progress bar
  drawHorizontalBar(hud, 4, 52, SCREEN_W - 8, 8,
                    hudData.oilPressure, 0, 100, -1, oilColor);
  
  // ========================================
  // DIVIDER
  // ========================================
  hud.drawLine(4, 64, SCREEN_W - 4, 64, COL_DIVIDER);
  
  // ========================================
  // BOTTOM SECTION: AFR (y: 64-128)
  // ========================================
  
  GaugeStatus afrStatus = getAFRStatus(hudData.afr);
  uint16_t afrColor = statusToColor(afrStatus);
  
  // Label
  hud.setTextColor(COL_LABEL, COL_BG);
  hud.setTextDatum(TL_DATUM);
  hud.setTextSize(1);
  hud.drawString("AFR", 4, 68);
  
  // Value (large) - centered
  hud.setTextColor(afrColor, COL_BG);
  hud.setTextDatum(MC_DATUM);
  hud.setTextSize(4);
  
  char afrBuf[8];
  snprintf(afrBuf, sizeof(afrBuf), "%.1f", hudData.afr);
  hud.drawString(afrBuf, SCREEN_W / 2, 96);
  
  // Progress bar with target marker at 14.7 (stoichiometric)
  // Rich on left, Lean on right
  hud.setTextColor(COL_LABEL, COL_BG);
  hud.setTextSize(1);
  hud.setTextDatum(TL_DATUM);
  hud.drawString("R", 4, 118);
  hud.setTextDatum(TR_DATUM);
  hud.drawString("L", SCREEN_W - 4, 118);
  
  drawHorizontalBar(hud, 14, 116, SCREEN_W - 28, 8,
                    hudData.afr, hudData.afrMin, hudData.afrMax, 
                    hudData.afrTarget, afrColor);
  
  // Push to display
  hud.pushSprite(0, 0);
}

// ============================================================================
// CAN BUS TRANSMISSION (placeholder for future implementation)
// ============================================================================

void prepareCANMessage(CANOilAFRMessage* msg) {
  msg->oilPressure_x10 = (uint16_t)(hudData.oilPressure * 10);
  msg->afr_x100 = (uint16_t)(hudData.afr * 100);
  
  // Set status bytes
  GaugeStatus oilStatus = getOilPressureStatus(hudData.oilPressure);
  msg->oilStatus = (oilStatus == STATUS_WARNING) ? 1 : 
                   (oilStatus == STATUS_CAUTION) ? 2 : 0;
  
  GaugeStatus afrStatus = getAFRStatus(hudData.afr);
  msg->afrStatus = (afrStatus == STATUS_WARNING) ? 1 : 
                   (afrStatus == STATUS_CAUTION) ? 2 : 0;
  
  msg->reserved = 0;
}

// Placeholder - implement with actual CAN library
void transmitCANData() {
  CANOilAFRMessage msg;
  prepareCANMessage(&msg);
  
  // TODO: Implement CAN bus transmission
  // canbus.send(CAN_ID_OIL_AFR, (uint8_t*)&msg, sizeof(msg));
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  
  // Initialize display
  tft.init();
  tft.setRotation(1);  // Landscape mode for 160x80
  tft.fillScreen(COL_BG);
  
  // Create sprite for double buffering
  hud.setColorDepth(16);
  hud.createSprite(SCREEN_W, SCREEN_H);
  hud.fillSprite(COL_BG);
  
  // Initial render
  renderHUD();
  
  Serial.println("HUD Display Ready");
  Serial.println("Displays: Oil Pressure (PSI) and AFR");
  Serial.println("");
  Serial.println("Commands:");
  Serial.println("  HUD:<oil_psi>:<afr>");
  Serial.println("  Example: HUD:45.0:14.7");
  Serial.println("");
  Serial.println("Data will be transmitted via CAN to ESP32 gauges.");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  static unsigned long lastUpdate = 0;
  static unsigned long lastCANTransmit = 0;
  static unsigned long lastDemoUpdate = 0;
  
  // Process incoming UART data
  processUARTInput();
  
  // Demo mode - simulate data if no UART input
  #ifdef DEMO_MODE
  if (millis() - lastDemoUpdate > 50) {
    lastDemoUpdate = millis();
    
    // Simulate oil pressure oscillation
    static float oilDir = 0.5f;
    hudData.oilPressure += oilDir;
    if (hudData.oilPressure > 70) oilDir = -0.5f;
    if (hudData.oilPressure < 20) oilDir = 0.5f;
    
    // Simulate AFR oscillation
    static float afrDir = 0.02f;
    hudData.afr += afrDir;
    if (hudData.afr > 16.0f) afrDir = -0.02f;
    if (hudData.afr < 12.0f) afrDir = 0.02f;
    
    hudData.updated = true;
  }
  #endif
  
  // Update display when data changes (rate limited)
  if (hudData.updated && (millis() - lastUpdate > 33)) {  // ~30 FPS max
    renderHUD();
    hudData.updated = false;
    lastUpdate = millis();
  }
  
  // Transmit CAN data at regular intervals (10Hz)
  if (millis() - lastCANTransmit > 100) {
    transmitCANData();
    lastCANTransmit = millis();
  }
}
