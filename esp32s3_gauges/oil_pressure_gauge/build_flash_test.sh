#!/bin/bash
# Build, flash, and test the Oil Pressure Gauge
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT=${1:-/dev/ttyACM1}
FQBN="esp32:esp32:esp32s3:CDCOnBoot=cdc"

echo "=== Building Oil Pressure Gauge ==="
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
echo "Protocol: OIL:<press_min>:<press_max>:<press_actual>:<afr_min>:<afr_max>:<afr_actual>"
echo "Example:  OIL:0:80:45:10:18:14.7"
echo ""
echo "Press Ctrl+C to stop"

while true; do
  for i in $(seq 0 2 100); do
    pressure=$((i * 80 / 100))
    afr=$(echo "scale=1; 10 + $i * 8 / 100" | bc)
    echo "OIL:0:80:$pressure:10:18:$afr" > "$PORT"
    sleep 0.02
  done
  
  for i in $(seq 100 -2 0); do
    pressure=$((i * 80 / 100))
    afr=$(echo "scale=1; 10 + $i * 8 / 100" | bc)
    echo "OIL:0:80:$pressure:10:18:$afr" > "$PORT"
    sleep 0.02
  done
done
