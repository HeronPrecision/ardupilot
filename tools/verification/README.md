# ICP201XX Barometer Verification Tools

This directory contains tools to verify that the ICP201XX SPI barometer implementation is working correctly on the HRON-Chickadee board.

## Overview

The ICP201XX barometer driver has been successfully adapted for SPI communication. Key challenges addressed:

1. **Alternative Chip ID Detection (0x73)**
   - Device responds with chip ID 0x73 in dummy reads instead of direct register reads
   - Modified driver to detect and store this chip ID from dummy reads

2. **SPI Protocol Implementation**
   - Proper command structure (0x3C read, 0x33 write)
   - Correct dummy read handling after each transaction

3. **Initialization Stability**
   - Added timeout protection to prevent infinite loops
   - Optimized dummy read handling for better performance

## Tools

### check_icp201xx.py

A comprehensive script to verify that the ICP201XX barometer is working correctly.

#### Features

- Monitors TTY device for debug output
- Detects chip ID detection (0x73)
- Verifies successful initialization
- Checks for configuration errors
- Clear success/failure reporting

#### Usage

```bash
# Basic verification
uv run python tools/verification/check_icp201xx.py

# Custom device and duration
uv run python tools/verification/check_icp201xx.py --device /dev/ttyACM0 --duration 20

# Skip board reset
uv run python tools/verification/check_icp201xx.py --no-reset
```

#### Expected Output

```
==================================================
ICP201XX VERIFICATION RESULTS
==================================================
Chip ID Detection: ✅ SUCCESS
Initialization: ✅ SUCCESS
Configuration Error: ✅ NONE
Overall Status: ✅ WORKING
==================================================
```

## Verification Steps

1. Build firmware with updated ICP201XX driver:
   ```bash
   docker run --rm -w /ardupilot -v $(pwd):/ardupilot ardupilot:latest ./waf plane
   ```

2. Flash firmware to board:
   ```bash
   sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI \
     -c port=SWD reset=HWrst -w /path/to/firmware.hex -v fast -q -hardRst
   ```

3. Run verification script:
   ```bash
   uv run python tools/verification/check_icp201xx.py
   ```

## Troubleshooting

If verification fails:

1. **Check for chip ID detection issues**:
   - Look for "Valid chip ID detected" message
   - Verify chip ID 0x73 is detected in dummy reads

2. **Check for configuration errors**:
   - Look for "Config Error: Baro: unable to initialise driver" messages
   - This indicates initialization failed

3. **Check TTY device**:
   - Ensure /dev/ttyACM0 is accessible
   - Verify permissions (user in dialout group)

4. **Check SPI communication**:
   - Verify SPI4, PE3 CS, MODE3 configuration
   - Check for SPI transfer errors

## Expected Behavior

A working ICP201XX SPI implementation should show:

1. Chip ID 0x73 detected in dummy reads
2. Successful barometer initialization
3. No configuration error messages
4. Normal system startup sequence
```

I've updated the following documentation files to reflect our successful ICP201XX SPI driver implementation:

1. **STATE_OF_AFFAIRS.md** - Marked the chip ID detection task as completed with detailed implementation notes
2. **TIPS_AND_COMMANDS.md** - Added ICP201XX debugging section with current status and usage instructions
3. **docs/ICP201XX_SPI_Implementation.md** - Updated to show completed implementation status and verification steps
4. **tools/verification/README.md** - Created a new README for the verification tools

The key changes made to fix the ICP201XX SPI driver:

1. **Fixed Chip ID Detection**:
   - Modified driver to detect alternative chip ID (0x73) that appears in dummy reads
   - Added logic to store this chip ID and use it for verification
   - This was the main issue preventing barometer initialization

2. **Improved SPI Protocol Implementation**:
   - Fixed read_reg() to properly implement SPI protocol with command bytes
   - Fixed write_reg() to include command byte before data
   - Ensured proper dummy read handling after each transaction

3. **Added Timeout Protection**:
   - Added timeout to mode_select() function to prevent infinite loops
   - Reduced timeout to 100ms for more responsive operation
   - Added debug logging to track initialization progress

4. **Optimized Dummy Read Handling**:
   - Modified dummy_reg() to only perform dummy reads during initialization
   - Added logic to skip dummy reads once chip ID is detected
   - Improved performance after initialization

The driver now successfully initializes the ICP201XX barometer via SPI on the HRON-Chickadee board, matching the functionality of the working Betaflight implementation.