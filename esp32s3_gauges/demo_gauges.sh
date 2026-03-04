#!/bin/bash
# Demo script for all three gauges
# Sends test data via USB serial to animate the gauges

PORT=${1:-/dev/ttyACM1}

echo "Starting gauge demo on $PORT"
echo "Press Ctrl+C to stop"

while true; do
  for i in $(seq 0 2 100); do
    # Fuel: 0-100%, boost range -20 to 20
    boost=$((i * 40 / 100 - 20))
    echo "FUEL:$i:-20:20:$boost" > $PORT
    
    # Oil: pressure 0-80 psi, AFR 10-18
    pressure=$((i * 80 / 100))
    afr=$(echo "scale=1; 10 + $i * 8 / 100" | bc)
    echo "OIL:0:80:$pressure:10:18:$afr" > $PORT
    
    # Water: temp 100-250°F, oil temp 150-300°F
    waterTemp=$((100 + i * 150 / 100))
    oilTemp=$((150 + i * 150 / 100))
    echo "WATER:100:250:$waterTemp:150:300:$oilTemp" > $PORT
    
    sleep 0.02
  done
  
  for i in $(seq 100 -2 0); do
    boost=$((i * 40 / 100 - 20))
    echo "FUEL:$i:-20:20:$boost" > $PORT
    pressure=$((i * 80 / 100))
    afr=$(echo "scale=1; 10 + $i * 8 / 100" | bc)
    echo "OIL:0:80:$pressure:10:18:$afr" > $PORT
    waterTemp=$((100 + i * 150 / 100))
    oilTemp=$((150 + i * 150 / 100))
    echo "WATER:100:250:$waterTemp:150:300:$oilTemp" > $PORT
    sleep 0.02
  done
done
