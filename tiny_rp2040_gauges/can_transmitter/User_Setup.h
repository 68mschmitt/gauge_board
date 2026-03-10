// User_Setup.h for CAN Transmitter Display
// Copy this file to your TFT_eSPI library folder (or use User_Setup_Select.h)
//
// Target: Pimoroni Tiny RP2040 + ST7735S 128x128 RGB TFT Display (1.44")
//         + MCP2515 CAN Bus Module
//
// SPI bus is shared between display and MCP2515:
//   - Display: GPIO 2 (CLK), GPIO 3 (MOSI), GPIO 7 (CS)
//   - MCP2515: GPIO 2 (CLK), GPIO 3 (MOSI), GPIO 4 (MISO), GPIO 28 (CS)

#define USER_SETUP_ID 402

// ============================================================================
// Display Driver - ST7735S 128x128 (1.44" square)
// ============================================================================

#define ST7735_DRIVER

// This is the specific variant for 128x128 displays (green tab / black tab)
#define TFT_WIDTH  128
#define TFT_HEIGHT 128

// Color order - try BGR if colors look wrong
#define TFT_RGB_ORDER TFT_BGR

// ST7735 128x128 typically needs these offsets
#define CGRAM_OFFSET
#define TFT_OFFSET_X 2
#define TFT_OFFSET_Y 3

// Some 128x128 displays are actually 128x160 with offset
// Uncomment if display is shifted:
// #define ST7735_GREENTAB128

// ============================================================================
// SPI Pin Configuration (Pimoroni Tiny RP2040)
// ============================================================================

// SPI bus pins
#define TFT_MOSI 3   // SPI TX - Data out to display
#define TFT_SCLK 2   // SPI CLK - Clock

// Control pins
#define TFT_DC   0   // Data/Command select
#define TFT_RST  1   // Reset (active low)
#define TFT_CS   7   // Chip Select

// Backlight control (optional - set to -1 if not used)
#define TFT_BL   6   // Backlight control pin
#define TFT_BACKLIGHT_ON HIGH

// ============================================================================
// SPI Configuration
// ============================================================================

// Do NOT enable PIO SPI for better compatibility
// #define RP2040_PIO_SPI

// SPI frequency - lower for reliability, ST7735 max is typically 15-20MHz
#define SPI_FREQUENCY       16000000
#define SPI_READ_FREQUENCY  8000000

// Enable DMA for faster transfers
#define RP2040_DMA

// ============================================================================
// Font Configuration
// ============================================================================

#define LOAD_GLCD    // Original Adafruit 8 pixel font
#define LOAD_FONT2   // Small 16 pixel high font
#define LOAD_FONT4   // Medium 26 pixel high font
#define LOAD_FONT6   // Large 48 pixel (digits only)
#define LOAD_FONT7   // 7-segment 48 pixel font
#define LOAD_FONT8   // Large 75 pixel font
#define LOAD_GFXFF   // FreeFonts

#define SMOOTH_FONT
