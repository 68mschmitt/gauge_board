/**
 * gauge_protocol.h
 * 
 * UART Protocol for receiving gauge data from external source
 * 
 * Protocol: Simple ASCII line-based for easy debugging
 * 
 * Formats:
 *   FUEL:<percent_0_100>:<boost_min>:<boost_max>:<boost_actual>
 *   OIL:<press_min>:<press_max>:<press_actual>:<afr_min>:<afr_max>:<afr_actual>
 *   WATER:<temp_min>:<temp_max>:<temp_actual>:<oil_min>:<oil_max>:<oil_actual>
 * 
 * Examples:
 *   FUEL:75:-20:20:5           (75% full, boost range -20 to 20, currently 5)
 *   OIL:0:80:45:10:18:14.7     (pressure 0-80 psi at 45, AFR 10-18 at 14.7)
 *   WATER:100:250:185:150:300:210  (water 100-250°F at 185, oil 150-300 at 210)
 * 
 * Broadcast mode: Send all three in sequence, each gauge ignores non-matching IDs
 */

#ifndef GAUGE_PROTOCOL_H
#define GAUGE_PROTOCOL_H

#include <Arduino.h>

// UART Configuration
#define GAUGE_UART_BAUD     115200
#define GAUGE_UART_RX_PIN   18      // Configurable per board
#define GAUGE_UART_TX_PIN   17      // Optional, for debugging

// Gauge identifiers
#define GAUGE_ID_FUEL   "FUEL"
#define GAUGE_ID_OIL    "OIL"
#define GAUGE_ID_WATER  "WATER"

// Message buffer
#define UART_BUFFER_SIZE 128

// Parsed gauge data
struct GaugeData {
  float needleValue;    // 0.0 to 1.0 (calculated from input)
  float digitalValue;   // Displayed number (calculated from input)
  bool updated;         // Set true when new data received
};

class GaugeProtocol {
public:
  GaugeProtocol(const char* gaugeId) : _gaugeId(gaugeId), _bufferIndex(0) {
    _data.needleValue = 0.5f;
    _data.digitalValue = 0.0f;
    _data.updated = false;
  }
  
  void begin() {
    #ifndef USE_USB_SERIAL
    Serial1.begin(GAUGE_UART_BAUD, SERIAL_8N1, GAUGE_UART_RX_PIN, GAUGE_UART_TX_PIN);
    #endif
  }
  
  void update() {
    #ifdef USE_USB_SERIAL
    Stream& input = Serial;
    #else
    Stream& input = Serial1;
    #endif
    
    while (input.available()) {
      char c = input.read();
      
      if (c == '\n' || c == '\r') {
        if (_bufferIndex > 0) {
          _buffer[_bufferIndex] = '\0';
          parseMessage(_buffer);
          _bufferIndex = 0;
        }
      } else if (_bufferIndex < UART_BUFFER_SIZE - 1) {
        _buffer[_bufferIndex++] = c;
      }
    }
  }
  
  bool hasUpdate() {
    if (_data.updated) {
      _data.updated = false;
      return true;
    }
    return false;
  }
  
  float getNeedle() { return _data.needleValue; }
  float getDigital() { return _data.digitalValue; }
  
private:
  const char* _gaugeId;
  char _buffer[UART_BUFFER_SIZE];
  int _bufferIndex;
  GaugeData _data;
  
  // Convert actual value to 0-1 needle position given min/max range
  float valueToNeedle(float val, float minVal, float maxVal) {
    if (maxVal <= minVal) return 0.5f;
    float normalized = (val - minVal) / (maxVal - minVal);
    return constrain(normalized, 0.0f, 1.0f);
  }
  
  void parseMessage(const char* msg) {
    char id[16];
    
    // Try to extract the gauge ID first
    if (sscanf(msg, "%15[^:]", id) != 1) return;
    if (strcmp(id, _gaugeId) != 0) return;
    
    // Skip past "ID:"
    const char* data = strchr(msg, ':');
    if (!data) return;
    data++;
    
    if (strcmp(_gaugeId, GAUGE_ID_FUEL) == 0) {
      // FUEL:<percent>:<boost_min>:<boost_max>:<boost_actual>
      float percent, boostMin, boostMax, boostActual;
      if (sscanf(data, "%f:%f:%f:%f", &percent, &boostMin, &boostMax, &boostActual) == 4) {
        _data.needleValue = constrain(percent / 100.0f, 0.0f, 1.0f);
        _data.digitalValue = boostActual;
        _data.updated = true;
      }
    }
    else if (strcmp(_gaugeId, GAUGE_ID_OIL) == 0) {
      // OIL:<press_min>:<press_max>:<press_actual>:<afr_min>:<afr_max>:<afr_actual>
      float pressMin, pressMax, pressActual, afrMin, afrMax, afrActual;
      if (sscanf(data, "%f:%f:%f:%f:%f:%f", &pressMin, &pressMax, &pressActual, &afrMin, &afrMax, &afrActual) == 6) {
        _data.needleValue = valueToNeedle(pressActual, pressMin, pressMax);
        _data.digitalValue = afrActual;
        _data.updated = true;
      }
    }
    else if (strcmp(_gaugeId, GAUGE_ID_WATER) == 0) {
      // WATER:<temp_min>:<temp_max>:<temp_actual>:<oil_min>:<oil_max>:<oil_actual>
      float tempMin, tempMax, tempActual, oilMin, oilMax, oilActual;
      if (sscanf(data, "%f:%f:%f:%f:%f:%f", &tempMin, &tempMax, &tempActual, &oilMin, &oilMax, &oilActual) == 6) {
        _data.needleValue = valueToNeedle(tempActual, tempMin, tempMax);
        _data.digitalValue = oilActual;
        _data.updated = true;
      }
    }
  }
};

#endif // GAUGE_PROTOCOL_H
