#!/bin/bash
# Build, flash, and test the Water Temperature Gauge
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT=${1:-/dev/ttyACM1}
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc"

echo "=== Building Water Temperature Gauge ==="
arduino-cli compile --fqbn "$FQBN" --clean "$SCRIPT_DIR"

echo ""
echo "=== Flashing to $PORT ==="
arduino-cli upload --fqbn "$FQBN" --port "$PORT" "$SCRIPT_DIR"

echo ""
echo "=== Waiting for device to reboot ==="
sleep 2

# Wait for the specified port to come back
for i in {1..10}; do
  if [ -e "$PORT" ]; then
    break
  fi
  echo "Waiting for $PORT..."
  sleep 1
done

if [ ! -e "$PORT" ]; then
  echo "Error: $PORT not found after reboot"
  exit 1
fi
echo "Using port: $PORT"

echo ""
echo "=== Running Demo ==="
echo "Protocol: WATER:<temp_min>:<temp_max>:<temp_actual>:<oil_min>:<oil_max>:<oil_actual>"
echo "Example:  WATER:100:250:185:150:300:210"
echo ""
echo "Press Ctrl+C to stop"

while true; do
  for i in $(seq 0 2 100); do
    waterTemp=$((100 + i * 150 / 100))
    oilTemp=$((150 + i * 150 / 100))
    echo "WATER:100:250:$waterTemp:150:300:$oilTemp" > "$PORT"
    sleep 0.02
  done
  
  for i in $(seq 100 -2 0); do
    waterTemp=$((100 + i * 150 / 100))
    oilTemp=$((150 + i * 150 / 100))
    echo "WATER:100:250:$waterTemp:150:300:$oilTemp" > "$PORT"
    sleep 0.02
  done
done
