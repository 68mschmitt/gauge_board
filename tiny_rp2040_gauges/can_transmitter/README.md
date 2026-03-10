# CAN Transmitter - Hardware Context

## Overview

This project implements a CAN bus transmitter using a Pimoroni Tiny RP2040 microcontroller, displaying transmitted values on a 128x128 ST7735S TFT display while sending simulated gauge data over CAN bus via an MCP2515 module.

## Hardware Components

| Component | Model | Notes |
|-----------|-------|-------|
| MCU | Pimoroni Tiny RP2040 | RP2040-based, USB-C |
| Display | ST7735S 128x128 | 1.44" square TFT, SPI interface |
| CAN Module | MCP2515 + TJA1050 | HiLetgo module, 8MHz crystal, 5V logic |

## Pin Connections

### Display (ST7735S)

| Display Pin | RP2040 GPIO | Function |
|-------------|-------------|----------|
| VCC | 3.3V | Power |
| GND | GND | Ground |
| SCK | GPIO 2 | SPI Clock (shared) |
| SDA/MOSI | GPIO 3 | SPI Data Out (shared) |
| DC | GPIO 0 | Data/Command select |
| RST | GPIO 1 | Reset (active low) |
| CS | GPIO 7 | Chip Select |
| BL | GPIO 6 | Backlight (HIGH = on) |

### CAN Module (MCP2515)

| MCP2515 Pin | RP2040 GPIO | Function |
|-------------|-------------|----------|
| VCC | 5V (VBUS) | Power (module needs 5V) |
| GND | GND | Ground |
| SCK | GPIO 2 | SPI Clock (shared with display) |
| SI (MOSI) | GPIO 3 | SPI Data In (shared with display) |
| SO (MISO) | GPIO 4 | SPI Data Out (CAN only) |
| CS | GPIO 28 | Chip Select |
| INT | GPIO 26 | Interrupt (optional) |

### Wiring Diagram

```
                    Pimoroni Tiny RP2040
                    ┌─────────────────────┐
                    │         USB-C       │
                    │    ┌───────────┐    │
                    │    │           │    │
              3.3V ─┤    │   RP2040  │    ├─ 5V (VBUS) ──► MCP2515 VCC
               GND ─┤    │           │    ├─ GND ────────► MCP2515 GND, Display GND
                    │    └───────────┘    │
           GPIO 0 ─┤                      ├─ GPIO 28 ───► MCP2515 CS
  (Display DC)     │                      │
           GPIO 1 ─┤                      ├─ GPIO 27
  (Display RST)    │                      │
           GPIO 2 ─┤──────────────────────┼──────────────► SCK (Display + MCP2515)
  (SPI CLK)        │                      │
           GPIO 3 ─┤──────────────────────┼──────────────► MOSI (Display + MCP2515 SI)
  (SPI MOSI)       │                      │
           GPIO 4 ─┤                      ├─ GPIO 26 ───► MCP2515 INT (optional)
  (SPI MISO)  ▲    │                      │
              │    │                      │
              │    └──────────────────────┘
              │
              └────────────────────────────────────────► MCP2515 SO

  Display CS ◄───── GPIO 7
  Display BL ◄───── GPIO 6
  Display VCC ◄──── 3.3V
```

## SPI Bus Configuration

Both the display and CAN module share the SPI bus (GPIO 2, 3) but use separate chip select pins:
- **Display CS**: GPIO 7
- **CAN CS**: GPIO 28

The RP2040 Arduino core defaults SPI to GPIO 16-19. This code explicitly reconfigures the Arduino SPI class to use GPIO 2-4:

```cpp
SPI.setRX(4);   // MISO on GPIO 4
SPI.setTX(3);   // MOSI on GPIO 3
SPI.setSCK(2);  // SCK on GPIO 2
```

Note: TFT_eSPI uses the Pico SDK's raw SPI functions (spi0), while mcp_can uses Arduino's SPI class. Both work together because they use proper CS pin management.

## CAN Bus Configuration

| Parameter | Value |
|-----------|-------|
| Baud Rate | 500 kbps |
| Crystal | 8 MHz |
| Mode | Loopback (testing) or Normal (real bus) |

### CAN Message IDs

| CAN ID | Data | Bytes | Description |
|--------|------|-------|-------------|
| 0x100 | uint8 (0-100) | 1 | Fuel level % |
| 0x101 | int16 (big-endian) | 2 | Boost/vacuum (-50 to +30) |
| 0x102 | uint8 (0-100) | 1 | Oil pressure % |
| 0x103 | uint16 (value × 10) | 2 | AFR (10.0-20.0) |
| 0x104 | uint8 (0-100) | 1 | Water temperature % |
| 0x105 | int16 (big-endian) | 2 | Oil temperature (°F) |

### Transmission Rate

Messages are transmitted at 20 Hz (every 50ms).

## Operating Modes

### Loopback Mode (Default)

For standalone testing without an external CAN bus. The MCP2515 internally receives its own transmitted messages.

```cpp
#define CAN_LOOPBACK_MODE true
```

### Normal Mode

For connection to a real CAN bus (vehicle OBD-II, another CAN device, etc.). Requires:
- Proper CAN bus termination (120Ω resistors at each end)
- At least one other device to ACK frames

```cpp
#define CAN_LOOPBACK_MODE false
```

## Dependencies

### Arduino Libraries

| Library | Version | Purpose |
|---------|---------|---------|
| TFT_eSPI | 2.5.43+ | Display driver |
| mcp_can | 1.5.1 | MCP2515 CAN controller |

### Board Support

- **Board Package**: Raspberry Pi Pico/RP2040 (rp2040:rp2040)
- **Board**: `rp2040:rp2040:rpipico`

## TFT_eSPI Configuration

The `User_Setup.h` file must be copied to the TFT_eSPI library folder. Key settings:

```cpp
#define ST7735_DRIVER
#define TFT_WIDTH  128
#define TFT_HEIGHT 128
#define TFT_RGB_ORDER TFT_BGR
#define TFT_MOSI 3
#define TFT_SCLK 2
#define TFT_DC   0
#define TFT_RST  1
#define TFT_CS   7
#define TFT_BL   6
#define SPI_FREQUENCY 16000000
```

## Build & Flash

```bash
# Compile
arduino-cli compile --fqbn rp2040:rp2040:rpipico can_transmitter.ino

# Flash (MCU must be in bootloader mode - hold BOOTSEL + reset)
cp build/can_transmitter.ino.uf2 /run/media/$USER/RPI-RP2/
```

## Serial Debug Output

115200 baud. Example output:

```
========================================
=== CAN Transmitter Starting ===
========================================
Pin configuration:
  Display: CS=GPIO7, DC=GPIO0, RST=GPIO1, BL=GPIO6
  CAN: CS=GPIO28, INT=GPIO26, MISO=GPIO4
  Shared SPI: SCK=GPIO2, MOSI=GPIO3

Initializing display...
Display initialized.

Initializing CAN bus...
Arduino SPI reconfigured: RX=GPIO4, TX=GPIO3, SCK=GPIO2
CAN CS pin: GPIO28
Attempting CAN init at 500kbps with 8MHz crystal...
CAN.begin() returned: 0x00 (CAN_OK=0x00, CAN_FAILINIT=0x01)
Setting CAN to LOOPBACK mode (standalone testing)...
CAN initialization successful!
==> CAN bus ready!

CAN TX: 100 msgs, Errors: 0
Values: Fuel=50% Boost=-11 Oil=49% AFR=15.9 Water=0% OilT=130F
```

## Display Layout

```
┌────────────────────────┐
│  CAN LOOP          [●] │  ← Header with status dot (green=OK, red=error)
├────────────────────────┤
│ ■ Fuel          XX%    │
│ ■ Boost         -XX    │
│ ■ Oil P         XX%    │
│ ■ AFR           XX.X   │
│ ■ Water         XX%    │
│ ■ OilT          XXXF   │
├────────────────────────┤
│ TX:XXXX      ERR:XXXX  │  ← Footer with counters
└────────────────────────┘
```

## Troubleshooting

### "CAN FAIL!" on startup

1. **Check wiring**: Especially MISO (SO → GPIO 4)
2. **Check power**: MCP2515 module needs 5V on VCC
3. **Check crystal**: Code assumes 8MHz (most common on cheap modules)

### TX count stays at 0, errors incrementing

- Normal mode requires an external CAN bus with another device to ACK frames
- Use loopback mode for standalone testing

### Display shows garbage or wrong colors

- Check `TFT_RGB_ORDER` setting in User_Setup.h (try `TFT_RGB` vs `TFT_BGR`)
- Check display offsets (`TFT_OFFSET_X`, `TFT_OFFSET_Y`)
