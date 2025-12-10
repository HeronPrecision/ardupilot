#!/bin/bash

# Monitor script to capture raw serial output from HRON-Chickadee
# This script resets the device and captures serial output without filtering

# Reset the board
echo "Resetting HRON-Chickadee board..."
sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=SWD reset=HWrst -hardRst

# Wait for device to enumerate
echo "Waiting for device to enumerate..."
sleep 3

# Check if device exists
if [ ! -e /dev/ttyACM0 ]; then
    echo "Error: /dev/ttyACM0 not found"
    exit 1
fi

# Configure serial port
stty -F /dev/ttyACM0 57600 raw -echo

# Capture raw serial output for 15 seconds
echo "Capturing serial output for 15 seconds..."
timeout 15 cat /dev/ttyACM0 > /tmp/serial_output.raw

# Display both raw and interpreted output
echo ""
echo "=== Raw Serial Output (hexdump) ==="
hexdump -C /tmp/serial_output.raw | head -20

echo ""
echo "=== String Output ==="
cat /tmp/serial_output.raw | strings

echo ""
echo "=== Complete Raw Output ==="
cat /tmp/serial_output.raw
