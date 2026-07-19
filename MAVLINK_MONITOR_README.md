# MAVLink Sensor Monitor for HRON-Chickadee-RC3

This directory contains tools for monitoring sensor data from the HRON-Chickadee-RC3 target via MAVLink protocol.

## Overview

The MAVLink monitor queries real-time sensor data from the ArduPilot firmware running on HRON-Chickadee-RC3, including:

- **Attitude (Gyros)**: Roll, pitch, yaw angles and rotation rates
- **Altitude (Pressure)**: Relative and absolute altitude from barometric sensor
- **Heading (Compass)**: Magnetic heading in degrees

## Files

- `monitor.sh` - Main monitoring script with device reset and MAVLink data display
- `mavlink_monitor.py` - Python script that communicates with ArduPilot via MAVLink
- `monitor_raw.sh` - Legacy raw serial output monitor (non-MAVLink)

## Quick Start

### Basic Usage

```bash
./monitor.sh
```

This will:
1. Reset the HRON-Chickadee-RC3 device via SWD
2. Wait for MAVLink initialization
3. Display sensor data at 1 Hz until you press Ctrl+C

### Common Options

```bash
# Monitor for 30 seconds at 2 Hz update rate
./monitor.sh -d 30 -r 0.5

# Use different baud rate
./monitor.sh -b 115200

# Skip device reset (if already running)
./monitor.sh --no-reset

# Show help
./monitor.sh -h
```

## Monitor Output

The monitor displays data in a tabular format:

```
====================================================================================================
TIME             ROLL    PITCH      YAW   ROLLSPD  PITCHSPD    YAWSPD   ALT_REL   ALT_ABS    HDG
(sec)           (deg)    (deg)    (deg)   (deg/s)   (deg/s)   (deg/s)       (m)       (m)  (deg)
====================================================================================================
5.07              N/A      N/A      N/A       N/A       N/A       N/A       N/A       N/A    N/A
5.58           178.25    -9.38    87.50      0.02     -0.01      0.00     -0.03     -0.02     87
6.08           178.30    -9.38    87.19      0.01      0.01      0.00      0.01      0.00     87    -27.0    164.0    211.0
```

**Note**: The last three columns (MAG_X, MAG_Y, MAG_Z) show raw magnetometer readings in milligauss (mGa). These are the **direct hardware readings** from the compass/magnetometer sensor and are the best way to verify the magnetometer is functioning correctly.

### Debug Output

Every 5 seconds, the monitor displays detailed information about MAVLink communications:

```
====================================================================================================
DEBUG OUTPUT @ 10.93s
====================================================================================================

MAVLink Commands Sent (last 5 seconds):
----------------------------------------------------------------------------------------------------
  [  7.93s] REQUEST_MESSAGE: ATTITUDE (id=30)
  [  7.94s] REQUEST_MESSAGE: VFR_HUD (id=74)
  [  7.94s] REQUEST_MESSAGE: GLOBAL_POSITION_INT (id=33)
  [  9.93s] REQUEST_MESSAGE: ATTITUDE (id=30)
  [  9.94s] REQUEST_MESSAGE: VFR_HUD (id=74)
  [  9.94s] REQUEST_MESSAGE: GLOBAL_POSITION_INT (id=33)

MAVLink Messages Received (last 5 seconds):
----------------------------------------------------------------------------------------------------
  Message counts: {'ATTITUDE': 21, 'GLOBAL_POSITION_INT': 21, 'VFR_HUD': 21}

  Recent message details (last 3 of each type):

  ATTITUDE:
    [ 10.18s] {'roll': 3.111, 'pitch': -0.163, 'yaw': 1.539, 'rollspeed': 0.0009, ...}
    [ 10.43s] {'roll': 3.111, 'pitch': -0.163, 'yaw': 1.539, 'rollspeed': -0.0001, ...}
    [ 10.69s] {'roll': 3.111, 'pitch': -0.163, 'yaw': 1.539, 'rollspeed': 0.00001, ...}

  VFR_HUD:
    [ 10.18s] {'airspeed': 0.0, 'groundspeed': 0.004, 'heading': 88, 'alt': 0.06, ...}
    [ 10.43s] {'airspeed': 0.0, 'groundspeed': 0.004, 'heading': 88, 'alt': 0.06, ...}
    [ 10.69s] {'airspeed': 0.0, 'groundspeed': 0.003, 'heading': 88, 'alt': 0.06, ...}

  GLOBAL_POSITION_INT:
    [ 10.18s] {'lat': 0.0, 'lon': 0.0, 'alt': 0.06, 'relative_alt': -0.044, ...}
    [ 10.43s] {'lat': 0.0, 'lon': 0.0, 'alt': 0.06, 'relative_alt': -0.018, ...}
    [ 10.69s] {'lat': 0.0, 'lon': 0.0, 'alt': 0.06, 'relative_alt': -0.041, ...}
====================================================================================================
```

This debug output helps verify:
- MAVLink commands are being sent correctly
- Messages are being received from the autopilot
- Data streams are functioning properly
- Message rates and timing

### Column Descriptions

- **TIME**: Elapsed time since monitoring started (seconds)
- **ROLL**: Roll angle in degrees (-180 to +180)
- **PITCH**: Pitch angle in degrees (-90 to +90)
- **YAW**: Yaw angle in degrees (0 to 360)
- **ROLLSPD**: Roll rate in degrees per second
- **PITCHSPD**: Pitch rate in degrees per second
- **YAWSPD**: Yaw rate in degrees per second
- **ALT_REL**: Relative altitude in meters (from home position)
- **ALT_ABS**: Absolute altitude in meters (MSL)
- **HDG**: Magnetic heading in degrees (0 to 360) - **fused from EKF**
- **MAG_X**: Raw magnetometer X-axis reading in milligauss (mGa)
- **MAG_Y**: Raw magnetometer Y-axis reading in milligauss (mGa)
- **MAG_Z**: Raw magnetometer Z-axis reading in milligauss (mGa)

### Understanding Magnetometer vs Heading

**Important distinction for compass testing:**

- **HDG (Heading)**: This comes from the VFR_HUD message and represents a *fused* value calculated by the EKF (Extended Kalman Filter). It blends data from GPS, magnetometer, gyros, and other sensors. While it uses magnetometer data, it's not a direct test of the magnetometer hardware.

- **MAG_X, MAG_Y, MAG_Z**: These come from RAW_IMU/SCALED_IMU messages and represent **direct magnetometer hardware readings**. These are the raw magnetic field measurements in three axes and are the **best way to verify the magnetometer/compass is producing valid data**.

**For magnetometer testing**, look for:
- Non-zero values that change when you rotate the device
- Values typically in the range of -500 to +500 mGa (Earth's magnetic field is ~250-650 mGa)
- Smooth, continuous readings without large jumps
- Values that make sense for your geographic location

## Python Script Usage

You can also run the Python monitor directly:

```bash
# Basic usage
uv run python3 mavlink_monitor.py /dev/ttyACM0

# With options
uv run python3 mavlink_monitor.py /dev/ttyACM0 -b 57600 -d 60 -r 0.5

# Via UDP (for SITL or network connections)
uv run python3 mavlink_monitor.py udp:localhost:14550
```

### Python Script Options

- `-b, --baud RATE`: Baud rate for serial connection (default: 57600)
- `-d, --duration SEC`: Monitoring duration in seconds (default: run until Ctrl+C)
- `-r, --rate SEC`: Update rate in seconds (default: 1.0)

## MAVLink Protocol Details

### Message Types Used

The monitor actively requests and processes the following MAVLink messages:

1. **HEARTBEAT** - Confirms connection and vehicle status
2. **ATTITUDE** - Roll, pitch, yaw angles and rotation rates
3. **VFR_HUD** - Altitude and heading data
3. **GLOBAL_POSITION_INT** - GPS position and relative altitude
4. **SYS_STATUS** - System health and status
5. **RAW_IMU** - Raw IMU data including magnetometer readings
6. **SCALED_IMU** - Scaled IMU data including magnetometer readings

### Data Request Strategy

The monitor uses three methods to ensure data reception:

1. **REQUEST_DATA_STREAM** - Requests legacy data stream groups
2. **MAV_CMD_SET_MESSAGE_INTERVAL** - Sets specific message intervals (MAVLink 2)
3. **MAV_CMD_REQUEST_MESSAGE** - Actively polls for specific messages

This multi-strategy approach ensures compatibility with various ArduPilot configurations.

### Debug Output (Every 5 Seconds)

The monitor automatically outputs detailed MAVLink communication information every 5 seconds, showing:

1. **Commands Sent** - All MAVLink commands transmitted in the last 5 seconds, including:
   - REQUEST_DATA_STREAM commands with stream ID and rate
   - SET_MESSAGE_INTERVAL commands with message ID and interval
   - REQUEST_MESSAGE commands for specific message types

2. **Messages Received** - Summary and details of received messages:
   - Message counts by type
   - Last 3 messages of each important type (ATTITUDE, VFR_HUD, GLOBAL_POSITION_INT)
   - Full parsed data from each message

This debug information is useful for:
- Verifying MAVLink communication is working
- Diagnosing message rate issues
- Understanding raw sensor values
- Debugging connection problems

## Connection Details

- **Default Port**: `/dev/ttyACM0` (USB CDC ACM device)
- **Default Baud Rate**: 57600 (standard ArduPilot default)
- **Protocol**: MAVLink 2.0
- **System ID**: 1 (ArduPilot default)
- **Component ID**: 0 (Autopilot component)

## Testing the Magnetometer/Compass

To specifically test if the magnetometer is working:

1. **Watch the MAG_X, MAG_Y, MAG_Z columns** - These should show non-zero values
2. **Rotate the device** - The magnetometer values should change smoothly as you rotate
3. **Check the debug output** - Look for RAW_IMU or SCALED_IMU messages with magnetometer data
4. **Compare HDG to magnetometer** - The heading should correlate with the magnetic field direction

**Example of good magnetometer data:**
```
MAG_X: -27.0 mGa
MAG_Y: 164.0 mGa
MAG_Z: 211.0 mGa
```

These values indicate the magnetometer is:
- Powered and communicating (non-zero values)
- Reading Earth's magnetic field (reasonable magnitude ~200-300 mGa total field)
- Providing data to the flight controller

**Note**: VFR_HUD's heading (HDG) is NOT the best test for the magnetometer because it's a fused/filtered value from multiple sensors via the EKF.

## Troubleshooting

### Device Not Found

If you see "No such file or directory: '/dev/ttyACM0'":

1. Check USB connection
2. Verify device enumeration: `ls -la /dev/ttyACM*`
3. Check permissions: `sudo chmod 666 /dev/ttyACM0`
4. Try resetting: `sudo /home/user/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=SWD reset=HWrst -hardRst`

### No Heartbeat Received

If the monitor connects but receives no heartbeat:

1. Verify ArduPilot firmware is properly flashed
2. Try different baud rates: 115200, 57600, 9600
3. Check if device is sending data: `timeout 3 cat /dev/ttyACM0 | od -A x -t x1z`
4. Ensure MAVLink is enabled in ArduPilot configuration

### Partial Data (N/A values)

If some columns show "N/A":

- The monitor is connected but specific message types aren't being received
- Wait a few seconds for data streams to initialize
- Some sensors may not be available or configured on the hardware

### Permission Denied

If you get permission errors:

```bash
sudo chmod 666 /dev/ttyACM0
# Or add your user to the dialout group (requires logout/login):
sudo usermod -a -G dialout $USER
```

## Development

### Dependencies

The Python monitor requires:
- `pymavlink` - MAVLink protocol implementation

These are managed via `uv` and defined in the project's virtual environment.

### Adding New Sensor Data

To add monitoring for additional sensors:

1. Find the MAVLink message type (e.g., `SCALED_IMU`, `RAW_IMU`)
2. Add message ID to `request_data_streams()` or `request_specific_messages()`
3. Handle the message in `process_messages()`
4. Extract data in `get_sensor_data()`
5. Update `print_header()` and `print_sensor_data()` for display

### Modifying Update Rates

Update rates are controlled by:
- Stream request rates (in `request_data_streams()`)
- Message interval settings (in microseconds)
- Display rate (command-line `-r` option)

## References

- [MAVLink Protocol Documentation](https://mavlink.io/)
- [ArduPilot MAVLink Commands](https://ardupilot.org/dev/docs/mavlink-commands.html)
- [pymavlink Library](https://github.com/ArduPilot/pymavlink)

## License

This monitoring tooling is part of the ArduPilot project and follows the same licensing terms.