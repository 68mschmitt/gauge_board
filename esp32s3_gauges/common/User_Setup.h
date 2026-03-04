// User_Setup.h for ESP32-S3 Single-Display Gauge
// Copy this file to your TFT_eSPI library folder
//
// Target: ESP32-S3 + GC9A01 240x240 display (one display per MCU)

#define USER_SETUP_ID 401

// Display driver
#define GC9A01_DRIVER

// Display dimensions
#define TFT_WIDTH  240
#define TFT_HEIGHT 240

// ============================================================================
// SPI Pin Configuration (ESP32-S3)
// Using FSPI (SPI2) - the default and fastest option
// ============================================================================

#define TFT_MOSI 11  // SPI MOSI (data out)
#define TFT_SCLK 12  // SPI CLK
#define TFT_CS   10  // Chip select
#define TFT_DC   9   // Data/Command
#define TFT_RST  14  // Reset

// Optional: backlight control
// #define TFT_BL   13
// #define TFT_BACKLIGHT_ON HIGH

// ============================================================================
// SPI Configuration - Maximum performance for single display
// ============================================================================

// 80MHz SPI - ESP32-S3 can handle this with short wires
#define SPI_FREQUENCY       80000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY 2500000

// Enable DMA for non-blocking transfers
#define USE_HSPI_PORT  // Use FSPI (SPI2) which is the default on S3

// ============================================================================
// Font Configuration
// ============================================================================

#define LOAD_GLCD    // Original Adafruit 8 pixel font
#define LOAD_FONT2   // Small 16 pixel high font
#define LOAD_FONT4   // Medium 26 pixel high font
#define LOAD_FONT6   // Large 48 pixel (digits only)
#define LOAD_FONT7   // 7-segment 48 pixel font
#define LOAD_GFXFF   // FreeFonts

#define SMOOTH_FONT
