/**
 * gauge_protocol.h
 * 
 * UART Protocol for receiving gauge data from external source
 * 
 * Protocol: Simple ASCII line-based for easy debugging
 * Format: <GAUGE_ID>:<NEEDLE_VALUE>:<DIGITAL_VALUE>\n
 * 
 * Examples:
 *   FUEL:0.75:-15         (Fuel at 75%, boost at -15)
 *   OIL:0.50:14.7         (Oil pressure at 50%, AFR at 14.7)
 *   WATER:0.60:210        (Water temp at 60%, oil temp at 210°F)
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
#define UART_BUFFER_SIZE 64

// Parsed gauge data
struct GaugeData {
  float needleValue;    // 0.0 to 1.0
  float digitalValue;   // Displayed number (int or float depending on gauge)
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
    Serial1.begin(GAUGE_UART_BAUD, SERIAL_8N1, GAUGE_UART_RX_PIN, GAUGE_UART_TX_PIN);
  }
  
  // Call this in loop() to process incoming UART data
  void update() {
    while (Serial1.available()) {
      char c = Serial1.read();
      
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
  
  // Get current gauge data
  GaugeData& getData() {
    return _data;
  }
  
  // Check if data was updated since last check
  bool hasUpdate() {
    if (_data.updated) {
      _data.updated = false;
      return true;
    }
    return false;
  }
  
  // Get needle value (0.0 to 1.0)
  float getNeedle() { return _data.needleValue; }
  
  // Get digital readout value
  float getDigital() { return _data.digitalValue; }
  
private:
  const char* _gaugeId;
  char _buffer[UART_BUFFER_SIZE];
  int _bufferIndex;
  GaugeData _data;
  
  void parseMessage(const char* msg) {
    // Format: GAUGE_ID:NEEDLE:DIGITAL
    char id[16];
    float needle, digital;
    
    if (sscanf(msg, "%15[^:]:%f:%f", id, &needle, &digital) == 3) {
      // Check if this message is for us
      if (strcmp(id, _gaugeId) == 0) {
        _data.needleValue = constrain(needle, 0.0f, 1.0f);
        _data.digitalValue = digital;
        _data.updated = true;
      }
    }
  }
};

#endif // GAUGE_PROTOCOL_H
