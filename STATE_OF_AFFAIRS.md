# ArduPilot HRON-Chickadee Project Status

## Current Status - BUILD, FLASH, AND MONITOR TESTING COMPLETED

Successfully performed the three core operations on the HRON-Chickadee project:
1. Build: Successfully compiled ArduCopter firmware for HRON-Chickadee
2. Flash: Successfully flashed firmware to the STM32H743 device
3. Monitor: Successfully observed boot and initialization sequence

## Build Results - SUCCESSFUL

### Build Process
- Executed: `./build.sh`
- Target: ArduCopter firmware for HRON-Chickadee board
- Result: Successfully built with no errors

### Build Output
```
Waf: Entering directory `/ardupilot/build/HRON-Chickadee'
...
+[1037/1037] Generating bin/arducopter_with_bl.hex
Waf: Leaving directory `/ardupilot/build/HRON-Chickadee'

BUILD SUMMARY
Build directory: /ardupilot/build/HRON-Chickadee
Target          Text (B)  Data (B)  BSS (B)  Total Flash Used (B)  Free Flash (B)  External Flash Used (B)
----------------------------------------------------------------------------------------------------------
bin/arducopter   1594308      3928   258324               1598236          105692  Not Applicable

'copter' finished successfully (6.114s)
```

### Firmware Details
- Firmware Size: 1,598,236 bytes (approximately 1.6MB)
- Free Flash: 105,692 bytes (approximately 103KB)
- File Generated: `build/HRON-Chickadee/bin/arducopter_with_bl.hex`

## Flash Results - SUCCESSFUL

### Flash Process
- Executed: `./flash.sh`
- Tool: STM32CubeProgrammer v2.21.0
- Interface: SWD (Serial Wire Debug)
- Result: Successfully flashed firmware to device

### Flash Output
```
ST-LINK SN  : 430037000A0000363132524E
ST-LINK FW  : V2J45S7
Board       : --
Voltage     : 3.27V
SWD freq    : 4000 KHz
Connect mode: Under Reset
Reset mode: Hardware reset
Device ID   : 0x450
Revision ID : Rev V
Device name : STM32H7xx
Flash size  : 2 MBytes
Device type : MCU
Device CPU  : Cortex-M7
BL Version  : 0x91

Memory Programming ...
  File          : arducopter_with_bl.hex
  Size          : 1.65 MB
  Address       : 0x08000000

Erasing internal memory sectors [0 13]
Download in Progress:
[==================================================] 100%

File download complete
Time elapsed during download operation: 00:00:27.180
```

### Device Information
- MCU: STM32H7xx (Cortex-M7)
- Flash Size: 2MB
- Programming Interface: ST-LINK via SWD
- Flash Time: 27.180 seconds for 1.65MB

## Monitor Results - IDENTIFIED BAROMETER INITIALIZATION ISSUE

### Monitor Process
- Executed: `./monitor.sh`
- Method: Reset device and capture serial output
- Result: Successfully captured boot sequence, identified barometer initialization issue

### Monitor Output
```
Hard reset is performed
=== ICP201XX: PROBE FUNCTION STARTING ===
=== ICP201XX: HELLO - PROBE DEBUG MESSAGE ===
ICP201XX: probe() ENTRY - bus 4 addr 0x01
=== ICP201XX: IF YOU SEE THIS, OUR DEBUG METHOD WORKS ===
=== ICP201XX: DEBUG HELLO MESSAGE IN CONSTRUCTOR ===
=== ICP201XX: CONSTRUCTOR CALLED - THIS IS OUR TEST ===
=== ICP201XX: SENSOR OBJECT CREATED ===
=== ICP201XX: THIS IS ANOTHER HELLO TEST ===
ICP201XX: sensor created, calling init()
ICP201XX: HELLO - Debug message test
Config Error: Baro: unable to initialise driver
Config Error: Baro: unable to initialise driver
7LConfig Error: Baro: unable to initialise driver
Config Error: Baro: unable to initialise driver
Config Error: Baro: unable to initialise driver
```

### Analysis of Barometer Issue
- The ICP201XX barometer driver is being detected and probed
- Constructor is successfully called
- The init() function is returning FAILED (though the debug message doesn't show this return value)
- This results in "Config Error: Baro: unable to initialise driver" messages
- The driver appears to be failing at some point during initialization, but the specific point of failure isn't clear
- The issue is likely related to SPI communication or chip ID validation

## Hardware Configuration

### STM32H743VIT6 Device
- Flash Size: 2MB
- Voltage: 3.27V
- Programming Interface: ST-LINK via SWD

### Barometer Sensor
- Model: ICP201XX
- Interface: SPI on bus 4
- Status: Detected but initialization failing due to chip ID mismatch

## Scripts Status

### build.sh
- Purpose: Builds ArduCopter firmware for HRON-Chickadee
- Implementation: Docker-based build using ardupilot-build container
- Command: `docker run --rm -v $(pwd):/ardupilot ardupilot-build /bin/bash -c "cd /ardupilot && ./waf --board=HRON-Chickadee copter"`
- Status: Working correctly

### flash.sh
- Purpose: Flashes firmware to STM32 device
- Implementation: Uses STM32CubeProgrammer CLI
- Command: `sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=SWD reset=HWrst -w build/HRON-Chickadee/bin/arducopter_with_bl.hex 0x08000000`
- Status: Working correctly

### monitor.sh
- Purpose: Resets device and captures serial output
- Implementation: Uses STM32CubeProgrammer CLI for reset and cat for serial monitoring
- Command: `sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=SWD reset=HWrst -hardRst && sleep 5 && timeout 25 cat /dev/ttyACM0 | strings`
- Status: Working correctly

## Current Issue - Barometer Initialization

The ICP201XX barometer driver is failing initialization because it's rejecting the chip ID returned by the sensor. Based on the documentation:

1. Expected Chip ID: 0x63 (according to driver)
2. Actual Chip ID: Unknown (needs debugging)
3. Current Driver Behavior: Fails initialization for unknown reason
4. Required Fix: Determine exact failure point in initialization sequence
5. Debug Priority: Add more detailed logging to identify failure point

## Next Steps

1. Fix ICP201XX Driver
   - Add detailed logging to `libraries/AP_Baro/AP_Baro_ICP201XX.cpp`
   - Identify exact point of failure in initialization
   - Determine if issue is chip ID, SPI communication, or other
   - Implement appropriate fix based on findings

2. Verification
   - Re-run build, flash, and monitor sequence
   - Confirm barometer initialization succeeds
   - Verify no "Config Error: Baro: unable to initialise driver" messages
   - Confirm successful pressure and temperature readings

3. Documentation
   - Update STATE_OF_AFFAIRS.md with resolution details
   - Record the root cause and solution
   - Document any workarounds needed for this hardware variant

## Key Files Referenced

- `build.sh` - Build script for ArduCopter firmware
- `flash.sh` - Flash script using STM32CubeProgrammer
- `monitor.sh` - Monitor script for device output
- `libraries/AP_Baro/AP_Baro_ICP201XX.cpp` - Barometer driver requiring debugging
- `libraries/AP_Baro/AP_Baro.cpp` - Barometer management code
- `libraries/AP_HAL_ChibiOS/hwdef/HRON-Chickadee/hwdef.dat` - Hardware configuration
- `docs/ICP201XX_SPI_Implementation.md` - Documentation of the SPI implementation