// User_Setup.h for Multi-Display Gauge Cluster
// Copy this file to your TFT_eSPI library folder (or use User_Setup_Select.h)
//
// Target: Pimoroni Tiny RP2040 + 3x GC9A01 240x240 displays
// IMPORTANT: Do NOT enable RP2040_PIO_SPI - CS switching won't work with PIO

#define USER_SETUP_ID 400

// Display driver
#define GC9A01_DRIVER

// Display dimensions
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// ============================================================================
// SPI Pin Configuration (Pimoroni Tiny RP2040)
// ============================================================================

// SPI bus pins (shared by all displays)
#define TFT_MOSI 3   // SPI TX - Data out to displays
#define TFT_SCLK 2   // SPI CLK - Clock

// Control pins (directly controlled, no SPI)
#define TFT_DC   0   // Data/Command select
#define TFT_RST  1   // Reset (active low, directly connected to all displays)

// IMPORTANT: Set TFT_CS to -1 to disable library CS control
// We control CS pins manually for multi-display support
#define TFT_CS  -1

// CS pins defined in main sketch (avoiding GPIO4 which is SPI0 MISO):
//   GPIO5 = Fuel Gauge
//   GPIO6 = Oil Pressure Gauge  
//   GPIO7 = Water Temperature Gauge

// ============================================================================
// SPI Configuration
// ============================================================================

// Do NOT enable PIO SPI - it doesn't support manual CS switching
// #define RP2040_PIO_SPI

// SPI frequency (40MHz is reliable, 62.5MHz may work)
#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000

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
