/**
 * CAN Transmitter - Single 128x128 Display
 * 
 * Displays transmitted CAN data on ST7735S 128x128 display
 * Transmits simulated gauge values over CAN bus using MCP2515 module
 * 
 * Display Pins (ST7735S):
 *   GPIO0  = DC
 *   GPIO1  = RST
 *   GPIO2  = SPI CLK (shared with CAN)
 *   GPIO3  = SPI MOSI (shared with CAN)
 *   GPIO6  = Backlight
 *   GPIO7  = Display CS
 * 
 * CAN Module Pins (MCP2515):
 *   GPIO2  = SPI CLK (shared with display)
 *   GPIO3  = SPI MOSI (shared with display)
 *   GPIO4  = SPI MISO (CAN only)
 *   GPIO28 = CAN CS
 *   GPIO26 = CAN INT (optional)
 * 
 * CAN Message IDs (500kbps, 8MHz crystal):
 *   0x100 = Fuel Level (uint8, 0-100%)
 *   0x101 = Boost/Vacuum (int16, -50 to +30)
 *   0x102 = Oil Pressure (uint8, 0-100%)
 *   0x103 = AFR (uint16, value * 10)
 *   0x104 = Water Temp (uint8, 0-100%)
 *   0x105 = Oil Temp (int16, degrees F)
 * 
 * Hardware: Pimoroni Tiny RP2040 + MCP2515/TJA1050 CAN Module
 * 
 * Required Library: mcp_can by coryjfowler (Arduino Library Manager)
 */

#include <TFT_eSPI.h>
#include <SPI.h>
#include <mcp_can.h>

// ============================================================================
// PIN DEFINITIONS
// ============================================================================

// Display pins (directly controlled)
#define TFT_BL_PIN   6   // Backlight

// CAN bus pins
#define CAN_CS_PIN   28  // MCP2515 chip select
#define CAN_INT_PIN  26  // MCP2515 interrupt (optional)
#define CAN_MISO_PIN 4   // SPI MISO for CAN (GPIO4 = SPI0 RX)

// ============================================================================
// CAN MESSAGE IDS
// ============================================================================

#define CAN_ID_FUEL_LEVEL   0x100
#define CAN_ID_BOOST        0x101
#define CAN_ID_OIL_PRESSURE 0x102
#define CAN_ID_AFR          0x103
#define CAN_ID_WATER_TEMP   0x104
#define CAN_ID_OIL_TEMP     0x105

// ============================================================================
// CAN BUS INSTANCE
// ============================================================================

// Use SPI1 for CAN to avoid conflicts with display (which uses SPI/SPI0)
// Note: On RP2040, SPI1 default pins are: MISO=12, MOSI=11, SCK=10
// But we can remap them to any GPIO
MCP_CAN CAN(&SPI, CAN_CS_PIN);  // Use default SPI but with explicit CS

// CAN status tracking
bool canInitialized = false;
bool canTxActive = false;
uint32_t lastCanTxTime = 0;
uint32_t canTxInterval = 50;  // Transmit every 50ms (20 Hz)
uint32_t canTxCount = 0;
uint32_t canErrorCount = 0;

// ============================================================================
// DISPLAY INSTANCE
// ============================================================================

TFT_eSPI tft = TFT_eSPI();

// Display dimensions
static const int SCREEN_W = 128;
static const int SCREEN_H = 128;

// ============================================================================
// COLORS
// ============================================================================

static const uint16_t COL_BG       = TFT_BLACK;
static const uint16_t COL_TEXT     = TFT_WHITE;
static const uint16_t COL_LABEL    = TFT_DARKGREY;
static const uint16_t COL_VALUE    = TFT_CYAN;
static const uint16_t COL_HEADER   = TFT_YELLOW;
static const uint16_t COL_TX_OK    = TFT_GREEN;
static const uint16_t COL_TX_ERR   = TFT_RED;

// ============================================================================
// SIMULATED GAUGE VALUES
// ============================================================================

// Fuel gauge
float fuelLevel = 0.5f;      // 0.0..1.0 (displayed as 0-100%)
int   boostValue = -50;      // Vacuum/boost reading (-50 to 30)

// Oil pressure gauge
float oilPressure = 0.5f;    // 0.0..1.0 (displayed as 0-100%)
float afrValue = 14.0f;      // AFR reading (0.0 to 20.0)

// Water temperature gauge
float waterTemp = 0.5f;      // 0.0..1.0 (displayed as 0-100%)
int   oilTempValue = 50;     // Oil temp in F (50 to 250)

// ============================================================================
// CAN CONFIGURATION
// ============================================================================

// Set to true for standalone testing (no external CAN bus needed)
// Set to false when connected to a real CAN network
#define CAN_LOOPBACK_MODE true

// ============================================================================
// CAN BUS FUNCTIONS
// ============================================================================

bool initCAN() {
  Serial.println("Configuring CAN module...");
  
  // Configure CAN CS pin - make sure it's HIGH (deselected)
  pinMode(CAN_CS_PIN, OUTPUT);
  digitalWrite(CAN_CS_PIN, HIGH);
  
  // Configure interrupt pin (optional)
  pinMode(CAN_INT_PIN, INPUT_PULLUP);
  
  // CRITICAL: Arduino SPI class on RP2040 defaults to GPIO 16-19, not GPIO 2-4!
  // We MUST reconfigure the Arduino SPI class to use our pins
  // These calls must happen before SPI.begin()
  SPI.end();  // End any existing SPI session
  SPI.setRX(CAN_MISO_PIN);  // GPIO 4
  SPI.setTX(3);              // GPIO 3 (same as TFT_MOSI)
  SPI.setSCK(2);             // GPIO 2 (same as TFT_SCLK)
  SPI.begin();
  
  Serial.printf("Arduino SPI reconfigured: RX=GPIO%d, TX=GPIO3, SCK=GPIO2\n", CAN_MISO_PIN);
  
  // Small delay to ensure SPI is stable
  delay(50);
  
  Serial.printf("CAN CS pin: GPIO%d\n", CAN_CS_PIN);
  Serial.printf("Attempting CAN init at 500kbps with 8MHz crystal...\n");
  
  // Initialize MCP2515 at 500kbps with 8MHz crystal
  // MCP_ANY = accept any message type (standard or extended)
  byte result = CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
  Serial.printf("CAN.begin() returned: 0x%02X (CAN_OK=0x%02X, CAN_FAILINIT=0x%02X)\n", 
                result, CAN_OK, CAN_FAILINIT);
  
  if (result == CAN_OK) {
    #if CAN_LOOPBACK_MODE
      // Loopback mode - for testing without external CAN bus
      // Module transmits and receives its own messages internally
      Serial.println("Setting CAN to LOOPBACK mode (standalone testing)...");
      CAN.setMode(MCP_LOOPBACK);
    #else
      // Normal mode - requires external CAN bus with termination and another device
      Serial.println("Setting CAN to NORMAL mode (external CAN bus required)...");
      CAN.setMode(MCP_NORMAL);
    #endif
    
    Serial.println("CAN initialization successful!");
    return true;
  }
  
  Serial.println("CAN initialization FAILED!");
  Serial.println("Possible causes:");
  Serial.println("  - Check MISO wiring (MCP2515 SO -> GPIO4)");
  Serial.println("  - Check power (5V to VCC)");
  Serial.println("  - Check crystal (8MHz expected)");
  return false;
}

// Transmit all gauge values over CAN bus
void transmitCANData() {
  byte txBuf[8];
  byte sendStatus;
  bool anyError = false;
  
  // --- CAN ID 0x100: Fuel Level (0-100%) ---
  txBuf[0] = (byte)(fuelLevel * 100.0f);
  sendStatus = CAN.sendMsgBuf(CAN_ID_FUEL_LEVEL, 0, 1, txBuf);
  if (sendStatus != CAN_OK) anyError = true;
  
  // --- CAN ID 0x101: Boost/Vacuum (int16) ---
  int16_t boostInt = (int16_t)boostValue;
  txBuf[0] = (boostInt >> 8) & 0xFF;  // High byte
  txBuf[1] = boostInt & 0xFF;          // Low byte
  sendStatus = CAN.sendMsgBuf(CAN_ID_BOOST, 0, 2, txBuf);
  if (sendStatus != CAN_OK) anyError = true;
  
  // --- CAN ID 0x102: Oil Pressure (0-100%) ---
  txBuf[0] = (byte)(oilPressure * 100.0f);
  sendStatus = CAN.sendMsgBuf(CAN_ID_OIL_PRESSURE, 0, 1, txBuf);
  if (sendStatus != CAN_OK) anyError = true;
  
  // --- CAN ID 0x103: AFR (value * 10, uint16) ---
  uint16_t afrInt = (uint16_t)(afrValue * 10.0f);
  txBuf[0] = (afrInt >> 8) & 0xFF;  // High byte
  txBuf[1] = afrInt & 0xFF;          // Low byte
  sendStatus = CAN.sendMsgBuf(CAN_ID_AFR, 0, 2, txBuf);
  if (sendStatus != CAN_OK) anyError = true;
  
  // --- CAN ID 0x104: Water Temp (0-100%) ---
  txBuf[0] = (byte)(waterTemp * 100.0f);
  sendStatus = CAN.sendMsgBuf(CAN_ID_WATER_TEMP, 0, 1, txBuf);
  if (sendStatus != CAN_OK) anyError = true;
  
  // --- CAN ID 0x105: Oil Temp (int16, degrees F) ---
  int16_t oilTempInt = (int16_t)oilTempValue;
  txBuf[0] = (oilTempInt >> 8) & 0xFF;  // High byte
  txBuf[1] = oilTempInt & 0xFF;          // Low byte
  sendStatus = CAN.sendMsgBuf(CAN_ID_OIL_TEMP, 0, 2, txBuf);
  if (sendStatus != CAN_OK) anyError = true;
  
  // Update status
  if (anyError) {
    canErrorCount++;
  } else {
    canTxCount++;
  }
  canTxActive = !anyError;
}

// ============================================================================
// DISPLAY FUNCTIONS
// ============================================================================

void drawHeader() {
  tft.setTextDatum(TC_DATUM);
  tft.setTextColor(COL_HEADER, COL_BG);
  tft.setTextSize(1);
  
  #if CAN_LOOPBACK_MODE
    tft.drawString("CAN LOOP", SCREEN_W / 2, 2);
  #else
    tft.drawString("CAN TX", SCREEN_W / 2, 2);
  #endif
  
  // Draw status indicator
  uint16_t statusColor = canInitialized ? (canTxActive ? COL_TX_OK : COL_TX_ERR) : COL_TX_ERR;
  tft.fillCircle(SCREEN_W - 10, 8, 5, statusColor);
  
  // Divider line
  tft.drawFastHLine(0, 14, SCREEN_W, COL_LABEL);
}

void drawDataRow(int y, const char* label, const char* value, uint16_t idColor) {
  // CAN ID indicator (small colored square)
  tft.fillRect(2, y + 2, 4, 10, idColor);
  
  // Label
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(COL_LABEL, COL_BG);
  tft.setTextSize(1);
  tft.drawString(label, 10, y);
  
  // Value (right-aligned)
  tft.setTextDatum(TR_DATUM);
  tft.setTextColor(COL_VALUE, COL_BG);
  tft.drawString(value, SCREEN_W - 4, y);
}

void drawDisplay() {
  char buf[16];
  int y = 18;
  int rowHeight = 16;
  
  // Clear data area only (preserve header)
  tft.fillRect(0, 16, SCREEN_W, SCREEN_H - 32, COL_BG);
  
  // Draw header with status
  drawHeader();
  
  // Row 1: Fuel Level (0x100)
  snprintf(buf, sizeof(buf), "%d%%", (int)(fuelLevel * 100));
  drawDataRow(y, "Fuel", buf, TFT_BLUE);
  y += rowHeight;
  
  // Row 2: Boost (0x101)
  snprintf(buf, sizeof(buf), "%d", boostValue);
  drawDataRow(y, "Boost", buf, TFT_PURPLE);
  y += rowHeight;
  
  // Row 3: Oil Pressure (0x102)
  snprintf(buf, sizeof(buf), "%d%%", (int)(oilPressure * 100));
  drawDataRow(y, "Oil P", buf, TFT_ORANGE);
  y += rowHeight;
  
  // Row 4: AFR (0x103)
  snprintf(buf, sizeof(buf), "%.1f", afrValue);
  drawDataRow(y, "AFR", buf, TFT_MAGENTA);
  y += rowHeight;
  
  // Row 5: Water Temp (0x104)
  snprintf(buf, sizeof(buf), "%d%%", (int)(waterTemp * 100));
  drawDataRow(y, "Water", buf, TFT_CYAN);
  y += rowHeight;
  
  // Row 6: Oil Temp (0x105)
  snprintf(buf, sizeof(buf), "%dF", oilTempValue);
  drawDataRow(y, "OilT", buf, TFT_RED);
  
  // Footer: TX count and errors
  tft.drawFastHLine(0, SCREEN_H - 16, SCREEN_W, COL_LABEL);
  
  tft.setTextDatum(BL_DATUM);
  tft.setTextColor(COL_TX_OK, COL_BG);
  tft.setTextSize(1);
  snprintf(buf, sizeof(buf), "TX:%lu", canTxCount);
  tft.drawString(buf, 4, SCREEN_H - 2);
  
  tft.setTextDatum(BR_DATUM);
  tft.setTextColor(canErrorCount > 0 ? COL_TX_ERR : COL_LABEL, COL_BG);
  snprintf(buf, sizeof(buf), "ERR:%lu", canErrorCount);
  tft.drawString(buf, SCREEN_W - 4, SCREEN_H - 2);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);  // Give time for serial to connect
  Serial.println("\n\n========================================");
  Serial.println("=== CAN Transmitter Starting ===");
  Serial.println("========================================");
  
  // Configure all CS pins HIGH (deselected) FIRST
  pinMode(CAN_CS_PIN, OUTPUT);
  digitalWrite(CAN_CS_PIN, HIGH);
  pinMode(7, OUTPUT);  // Display CS
  digitalWrite(7, HIGH);
  
  // Configure backlight
  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);
  
  Serial.println("Pin configuration:");
  Serial.printf("  Display: CS=GPIO7, DC=GPIO0, RST=GPIO1, BL=GPIO%d\n", TFT_BL_PIN);
  Serial.printf("  CAN: CS=GPIO%d, INT=GPIO%d, MISO=GPIO%d\n", CAN_CS_PIN, CAN_INT_PIN, CAN_MISO_PIN);
  Serial.println("  Shared SPI: SCK=GPIO2, MOSI=GPIO3");
  
  // Initialize display (uses TFT_eSPI which has its own SPI setup)
  Serial.println("\nInitializing display...");
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(COL_BG);
  Serial.println("Display initialized.");
  
  // Show init message
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextSize(1);
  tft.drawString("Init CAN...", SCREEN_W / 2, SCREEN_H / 2);
  
  // Small delay to let display settle
  delay(100);
  
  // Initialize CAN bus (will reconfigure Arduino SPI for our pins)
  Serial.println("\nInitializing CAN bus...");
  canInitialized = initCAN();
  
  if (canInitialized) {
    Serial.println("==> CAN bus ready!");
    tft.fillScreen(COL_BG);
    tft.setTextColor(COL_TX_OK, COL_BG);
    tft.drawString("CAN OK!", SCREEN_W / 2, SCREEN_H / 2);
  } else {
    Serial.println("==> CAN bus FAILED!");
    tft.fillScreen(COL_BG);
    tft.setTextColor(COL_TX_ERR, COL_BG);
    tft.drawString("CAN FAIL!", SCREEN_W / 2, SCREEN_H / 2);
    tft.drawString("Check wiring", SCREEN_W / 2, SCREEN_H / 2 + 16);
  }
  
  delay(1000);
  tft.fillScreen(COL_BG);
  
  Serial.println("Setup complete. Starting main loop...");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

static uint32_t lastFrameTime = 0;
static uint32_t lastDisplayUpdate = 0;
static uint32_t displayUpdateInterval = 100;  // Update display every 100ms

void loop() {
  uint32_t now = millis();
  uint32_t deltaMs = now - lastFrameTime;
  lastFrameTime = now;
  
  // Scale animation by time delta for consistent speed
  float deltaScale = deltaMs / 40.0f;
  
  // ---- Update simulated values ----
  
  // Fuel gauge - slow sweep
  static float fuelDir = 1.0f;
  fuelLevel += 0.002f * fuelDir * deltaScale;
  if (fuelLevel >= 1.0f) { fuelLevel = 1.0f; fuelDir = -1.0f; }
  if (fuelLevel <= 0.0f) { fuelLevel = 0.0f; fuelDir = 1.0f; }
  
  // Boost - faster sweep
  static float boostDir = 1.0f;
  boostValue += (int)(0.5f * boostDir * deltaScale);
  if (boostValue > 30) { boostValue = 30; boostDir = -1.0f; }
  if (boostValue < -50) { boostValue = -50; boostDir = 1.0f; }
  
  // Oil pressure - slow sweep  
  static float oilDir = 1.0f;
  oilPressure += 0.003f * oilDir * deltaScale;
  if (oilPressure >= 1.0f) { oilPressure = 1.0f; oilDir = -1.0f; }
  if (oilPressure <= 0.0f) { oilPressure = 0.0f; oilDir = 1.0f; }
  
  // AFR - medium sweep
  static float afrDir = 1.0f;
  afrValue += 0.05f * afrDir * deltaScale;
  if (afrValue > 20.0f) { afrValue = 20.0f; afrDir = -1.0f; }
  if (afrValue < 10.0f) { afrValue = 10.0f; afrDir = 1.0f; }
  
  // Water temp - slow sweep
  static float waterDir = 1.0f;
  waterTemp += 0.0015f * waterDir * deltaScale;
  if (waterTemp >= 1.0f) { waterTemp = 1.0f; waterDir = -1.0f; }
  if (waterTemp <= 0.0f) { waterTemp = 0.0f; waterDir = 1.0f; }
  
  // Oil temp - medium sweep
  static float oilTempDir = 1.0f;
  oilTempValue += (int)(1.0f * oilTempDir * deltaScale);
  if (oilTempValue > 250) { oilTempValue = 250; oilTempDir = -1.0f; }
  if (oilTempValue < 50) { oilTempValue = 50; oilTempDir = 1.0f; }
  
  // ---- CAN TRANSMISSION ----
  if (canInitialized && (now - lastCanTxTime >= canTxInterval)) {
    transmitCANData();
    lastCanTxTime = now;
  }
  
  // ---- DISPLAY UPDATE ----
  if (now - lastDisplayUpdate >= displayUpdateInterval) {
    drawDisplay();
    lastDisplayUpdate = now;
  }
  
  // Debug output every 5 seconds
  static uint32_t lastDebugTime = 0;
  if (now - lastDebugTime >= 5000) {
    Serial.printf("CAN TX: %lu msgs, Errors: %lu\n", canTxCount, canErrorCount);
    Serial.printf("Values: Fuel=%d%% Boost=%d Oil=%d%% AFR=%.1f Water=%d%% OilT=%dF\n",
                  (int)(fuelLevel * 100), boostValue,
                  (int)(oilPressure * 100), afrValue,
                  (int)(waterTemp * 100), oilTempValue);
    lastDebugTime = now;
  }
}
