# MAVLink Monitor Improvements for HRON-Chickadee-RC3

## Overview

Enhanced the monitor.sh script and created a comprehensive MAVLink monitoring system for the HRON-Chickadee-RC3 ArduPilot target. The system provides real-time sensor data monitoring with active MAVLink querying and support for both detailed and quiet output modes.

## Key Improvements

### 1. Complete MAVLink Monitor Implementation

**Created `mavlink_monitor.py`:**
- Actively queries sensor data via MAVLink protocol
- Monitors attitude (gyros), altitude (pressure), and heading (compass)
- Uses multiple MAVLink request strategies for reliability:
  - REQUEST_DATA_STREAM (legacy method)
  - SET_MESSAGE_INTERVAL (MAVLink 2.0 method)
  - REQUEST_MESSAGE (active polling)

**Enhanced `monitor.sh`:**
- Reset device via SWD
- Handle serial port permissions automatically
- Support command-line options
- Colored output and error handling
- Default 60-second duration to ensure proper exit

### 2. Direct Magnetometer Testing

Added direct magnetometer/compass testing via:
- RAW_IMU and SCALED_IMU message requests
- Display of raw magnetometer readings (MAG_X, MAG_Y, MAG_Z) in milligauss
- **This is the best way to verify compass hardware is working** (vs. VFR_HUD's fused heading)

### 3. Concise Output with Debug Information

**Normal Mode:**
- Tabular display of sensor data
- Detailed debug output every 5 seconds showing:
  - MAVLink commands sent
  - Message types and counts received
  - Raw magnetometer values for troubleshooting

**Quiet Mode (-q):**
- Sensor data only
- No debug output or connection messages
- Ideal for automated testing or data logging

### 4. Enhanced Monitoring Capabilities

**Real-time Sensor Data:**
```
TIME        ROLL   PITCH    YAW  ROLLSPD PITCHSPD  YAWSPD ALT_REL ALT_ABS  HDG  MAG_X  MAG_Y  MAG_Z
(sec)      (deg)   (deg)  (deg)  (deg/s)  (deg/s) (deg/s)     (m)     (m) (deg) (mGa)  (mGa)  (mGa)
6.94      178.26   -9.34  87.98    -0.04     0.01   -0.02    0.60    0.06   87  -27.0  164.0  211.0
```

**Magnetometer Testing:**
- Raw values show if magnetometer is powered and reading Earth's magnetic field
- Values typically 250-650 mGa for Earth's field
- Values change smoothly when device is rotated

### 5. Command-Line Options

**monitor.sh Options:**
- `-p, --port PORT`: Serial port (default: /dev/ttyACM0)
- `-b, --baud RATE`: Baud rate (default: 57600)
- `-d, --duration SEC`: Run for seconds (default: 60)
- `-r, --rate SEC`: Update rate (default: 1.0)
- `-q, --quiet`: Suppress debug output
- `--no-reset`: Skip device reset

## Usage Examples

```bash
# Basic monitoring with debug output (default 60s)
./monitor.sh

# Quick 30-second test with faster updates
./monitor.sh -d 30 -r 0.5

# Quiet mode for data logging only
./monitor.sh -d 120 --no-reset -q > sensor_data.log

# Monitor with different baud rate
./monitor.sh -b 115200 -d 45

# Skip reset for already running device
./monitor.sh --no-reset -d 10 -r 0.2
```

## Files Created/Modified

1. **`mavlink_monitor.py`** - Python MAVLink monitoring script (new)
2. **`monitor.sh`** - Enhanced bash wrapper (updated)
3. **`MAVLINK_MONITOR_README.md`** - Comprehensive documentation (new)
4. **`MAVLINK_MONITOR_FINAL_SUMMARY.md`** - This summary (new)

## Troubleshooting Features

The monitor includes comprehensive error handling and troubleshooting information:
- Serial port configuration and permissions
- Connection timeout detection
- Detailed MAVLink communication logging
- Message type tracking and diagnostics
- Magnetometer-specific testing guidance

## Integration with ArduPilot

The monitor integrates seamlessly with ArduPilot's MAVLink implementation:
- Compatible with ArduPilot's standard message set
- Uses ArduPilot's default connection parameters
- Supports all ArduPilot vehicle types
- Works with standard ArduPilot sensor configuration

This enhanced monitoring system provides comprehensive sensor testing capabilities for the HRON-Chickadee-RC3 target while maintaining simplicity and usability.