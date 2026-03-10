/**
 * hud_protocol.h
 * 
 * Protocol for HUD display data transmission
 * 
 * This protocol handles:
 * 1. Receiving sensor data (oil pressure, AFR) via UART
 * 2. Preparing data for CAN bus transmission to ESP32 gauges
 * 
 * UART Input Format:
 *   HUD:<oil_psi>:<afr>
 *   Example: HUD:45.0:14.7
 * 
 * CAN Output Format:
 *   Message ID: 0x300
 *   Bytes 0-1: Oil pressure (uint16, PSI * 10)
 *   Bytes 2-3: AFR (uint16, value * 100)
 *   Bytes 4:   Oil status (0=OK, 1=LOW, 2=HIGH)
 *   Bytes 5:   AFR status (0=OK, 1=LEAN, 2=RICH)
 *   Bytes 6-7: Reserved
 * 
 * The ESP32 gauges will receive this CAN message and parse it using
 * the protocol defined in esp32s3_gauges/oil_pressure_gauge/gauge_protocol.h
 */

#ifndef HUD_PROTOCOL_H
#define HUD_PROTOCOL_H

#include <Arduino.h>

// ============================================================================
// CAN Message Definitions
// ============================================================================

// CAN message IDs
#define CAN_ID_OIL_AFR        0x300   // Oil pressure + AFR (this HUD)
#define CAN_ID_FUEL_BOOST     0x301   // Fuel level + boost (future HUD)
#define CAN_ID_WATER_OIL_TEMP 0x302   // Water temp + oil temp (future HUD)

// Status codes
#define STATUS_OK      0
#define STATUS_LOW     1
#define STATUS_HIGH    2
#define STATUS_LEAN    1
#define STATUS_RICH    2

// ============================================================================
// Data Structures
// ============================================================================

// Raw CAN message structure (8 bytes, matches CAN 2.0A standard)
struct CANOilAFRData {
  uint16_t oilPressure_x10;   // Oil pressure in PSI * 10 (0-1000 = 0-100 PSI)
  uint16_t afr_x100;          // AFR * 100 (1000-1800 = 10.0-18.0)
  uint8_t  oilStatus;         // 0=OK, 1=LOW, 2=HIGH
  uint8_t  afrStatus;         // 0=OK, 1=LEAN, 2=RICH
  uint8_t  reserved[2];       // Future use
};

// Decoded data for display/processing
struct HUDSensorData {
  // Oil Pressure
  float oilPressure;          // PSI (0-100)
  float oilPressureMinWarn;   // Low warning threshold
  float oilPressureMaxWarn;   // High warning threshold
  
  // AFR
  float afr;                  // Air-Fuel Ratio (10.0-18.0 typical)
  float afrMin;               // Minimum displayable
  float afrMax;               // Maximum displayable
  float afrTarget;            // Target (stoichiometric)
  
  // Timestamps
  uint32_t lastUpdate;        // millis() of last update
  bool valid;                 // Data validity flag
};

// ============================================================================
// HUD Protocol Class
// ============================================================================

class HUDProtocol {
public:
  HUDProtocol() : _bufferIndex(0) {
    resetData();
  }
  
  void begin() {
    Serial.begin(115200);
    _data.lastUpdate = millis();
  }
  
  // Process incoming serial data
  void update() {
    while (Serial.available()) {
      char c = Serial.read();
      
      if (c == '\n' || c == '\r') {
        if (_bufferIndex > 0) {
          _buffer[_bufferIndex] = '\0';
          parseMessage(_buffer);
          _bufferIndex = 0;
        }
      } else if (_bufferIndex < BUFFER_SIZE - 1) {
        _buffer[_bufferIndex++] = c;
      }
    }
  }
  
  // Check if new data available
  bool hasUpdate() {
    if (_dataUpdated) {
      _dataUpdated = false;
      return true;
    }
    return false;
  }
  
  // Getters
  float getOilPressure() const { return _data.oilPressure; }
  float getAFR() const { return _data.afr; }
  const HUDSensorData& getData() const { return _data; }
  
  // Setters (for direct sensor input or simulation)
  void setOilPressure(float psi) {
    _data.oilPressure = constrain(psi, 0.0f, 100.0f);
    _data.lastUpdate = millis();
    _data.valid = true;
    _dataUpdated = true;
  }
  
  void setAFR(float afr) {
    _data.afr = constrain(afr, 5.0f, 25.0f);
    _data.lastUpdate = millis();
    _data.valid = true;
    _dataUpdated = true;
  }
  
  // Prepare CAN message for transmission
  void prepareCANMessage(CANOilAFRData* msg) {
    msg->oilPressure_x10 = (uint16_t)(_data.oilPressure * 10);
    msg->afr_x100 = (uint16_t)(_data.afr * 100);
    
    // Determine oil status
    if (_data.oilPressure < _data.oilPressureMinWarn) {
      msg->oilStatus = STATUS_LOW;
    } else if (_data.oilPressure > _data.oilPressureMaxWarn) {
      msg->oilStatus = STATUS_HIGH;
    } else {
      msg->oilStatus = STATUS_OK;
    }
    
    // Determine AFR status
    if (_data.afr < _data.afrTarget - 2.0f) {
      msg->afrStatus = STATUS_RICH;
    } else if (_data.afr > _data.afrTarget + 2.0f) {
      msg->afrStatus = STATUS_LEAN;
    } else {
      msg->afrStatus = STATUS_OK;
    }
    
    msg->reserved[0] = 0;
    msg->reserved[1] = 0;
  }
  
  // Configure thresholds
  void setOilThresholds(float minWarn, float maxWarn) {
    _data.oilPressureMinWarn = minWarn;
    _data.oilPressureMaxWarn = maxWarn;
  }
  
  void setAFRConfig(float min, float max, float target) {
    _data.afrMin = min;
    _data.afrMax = max;
    _data.afrTarget = target;
  }
  
private:
  static const int BUFFER_SIZE = 64;
  char _buffer[BUFFER_SIZE];
  int _bufferIndex;
  HUDSensorData _data;
  bool _dataUpdated;
  
  void resetData() {
    _data.oilPressure = 0.0f;
    _data.oilPressureMinWarn = 15.0f;
    _data.oilPressureMaxWarn = 80.0f;
    _data.afr = 14.7f;
    _data.afrMin = 10.0f;
    _data.afrMax = 18.0f;
    _data.afrTarget = 14.7f;
    _data.lastUpdate = 0;
    _data.valid = false;
    _dataUpdated = false;
  }
  
  void parseMessage(const char* msg) {
    // Format: HUD:<oil_psi>:<afr>
    char id[8];
    float oilPsi, afr;
    
    if (sscanf(msg, "%7[^:]:%f:%f", id, &oilPsi, &afr) == 3) {
      if (strcmp(id, "HUD") == 0) {
        _data.oilPressure = constrain(oilPsi, 0.0f, 100.0f);
        _data.afr = constrain(afr, 5.0f, 25.0f);
        _data.lastUpdate = millis();
        _data.valid = true;
        _dataUpdated = true;
      }
    }
  }
};

#endif // HUD_PROTOCOL_H
