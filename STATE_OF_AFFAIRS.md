# ArduPilot HRON-Chickadee-RC3 Project Status

## Current Status - FIFO SAFEGUARDS INTEGRATED ⚠️ HW VALIDATION PENDING

**FIFO overflow protections and averaging updates merged; awaiting hardware regression verification**

### Completed Implementation
- ✓ SPI Mode 3 (CPOL=1, CPHA=1) with full-duplex transactions
- ✓ Chip ID read: **0x63** verified on hardware
- ✓ Boot sequence with OTP calibration (non-B2 variant)
- ✓ Soft reset with proper command (0x80) and timing
- ✓ Mode configuration using Read-Modify-Write approach
- ✓ Mode 1 operation (120Hz ODR) for high-speed sampling
- ✓ FIR filter warmup (14 sample discard)
- ✓ 25ms timer interval (collect ~3 samples per read)
- ✓ MODE_SELECT latch delay (200µs unconditional)
- ✓ All timing matches Betaflight exactly
- ✓ FIFO read path now caps transfers, adds trend averaging, and guards against overflow

### SPI Transaction Fix
Changed all SPI operations from `transfer(tx, len, rx, len)` to `transfer_fullduplex(buf, len)`:
- `dummy_reg()`: Single buffer for 3-byte transaction
- `read_reg()`: Single buffer, extracts data from `buf[2]`
- `write_reg()`: Single buffer for [CMD, REG, VAL] transaction

Verified working on hardware:
```
ICP201XX read_reg: reg=0x0C len=1 tx=[0x3C,0x0C,0xFF] rx=[0xFF,0xFF,0x63] data=0x63
```

## Hardware Configuration

### STM32H743VIT6 Device
- Flash Size: 2MB
- Voltage: 3.27V
- Programming Interface: ST-LINK via SWD
- **IMPORTANT: Board approaching lifetime write cycle limit - minimize flashing**

### Barometer Sensor
- Model: ICP201XX (TDK InvenSense)
- Interface: SPI Mode 3 on bus 4
- Chip ID: 0x63 ✓ CONFIRMED
- SPI Frequency: 6 MHz max
- Transaction format: 3-byte full-duplex [CMD, REG, DATA]

## Next Steps - Hardware Validation

### Current State
All driver functionality implemented and committed:
- Commit 318dd27d96: SPI full-duplex fix (chip ID working)
- Commit ad5cb6cea0: Complete driver implementation

### Remaining Tasks
1. **Hardware testing** (minimize flashing due to write cycle concerns)
   - Verify boot sequence executes correctly
   - Confirm FIFO data collection and discard logic working under load
   - Validate pressure/temperature readings with new averaging
   - Check that Mode 1 (120Hz) operation remains stable

2. **Toolchain verification**
   - Ensure the crash analysis workflow has access to `arm-none-eabi-gdb`
   - Re-run `crash_debugger.py` end-to-end once toolchain is present

3. **Monitor for follow-up improvements**
   - Evaluate whether non-blocking FIFO reads are still needed
   - Revisit trend weighting parameters after real-flight data review

## Critical Implementation Details from Betaflight

### Timing Requirements
- **Startup delay**: 100ms after power-on (Betaflight increased from 10ms)
- **MODE_SELECT latch delay**: 200µs UNCONDITIONAL after MODE_SELECT writes
- **Read interval**: 25ms (collect ~3 samples at 120Hz)
- **Conversion interval**: 8333µs (120Hz ODR)

### Boot Sequence (Version-Dependent)
```
if (version == 0xB2):
    # B2 variant boot sequence
    1. Read OTP data (4 blocks)
    2. Write block 0 word 0 to MR register
    3. Write block 0 word 1 to MRA register  
    4. Write block 0 word 2 to MRB register
else:
    # Non-B2 variant
    1. Read OTP data (4 blocks)
    # No MR/MRA/MRB writes needed
```

### OTP Reading State Machine
Must follow exact sequence:
1. Write 0x00 to OTP_CONFIG1 (enable OTP)
2. For each address 0-3:
   - Write address to OTP_ADDR
   - Write 0x10 (READ command) to OTP_CMD
   - Poll OTP_STATUS until bit 0 == 1 (ready)
   - Read data from OTP_RDATA
3. Store 4 bytes of OTP data for calibration

### Mode Configuration
```
MODE_SELECT register bits:
- Bit 0-1: FIFO readout mode (0 = pressure+temp interleaved)
- Bit 2: Power mode (0 = normal, 1 = active) - USE NORMAL
- Bit 3: Measurement mode (1 = continuous)
- Bit 4: Forced trigger (0 = standby)
- Bit 5-7: Operation mode (1 = Mode 1 = 120Hz ODR)

Value: 0x20 | 0x08 | 0x00 = 0x28
(Mode 1 | continuous | normal power | pres+temp FIFO)
```

### FIFO Reading
- Read FIFO_FILL register to get sample count
- Each sample = 6 bytes (3 pressure + 3 temperature)
- Must read in multiples of 6 bytes
- Data format: 20-bit values in 3 bytes (MSB first)

### Register Access Pattern
Every register read/write MUST be followed by dummy read of REG_EMPTY (0x00), 
EXCEPT when reading REG_EMPTY itself.

### SPI Commands
- Read: 0x3C (single byte) or 0x3D (multi-byte) - Betaflight uses 0x3C for all
- Write: 0x33 (Betaflight uses this, not datasheet's 0x34)

## Key Files

- `libraries/AP_Baro/AP_Baro_ICP201XX.cpp` - Driver (NEEDS COMPLETION)
- `libraries/AP_Baro/AP_Baro_ICP201XX.h` - Driver header
- `betaflight/src/main/drivers/barometer/barometer_icp201xx.c` - Reference (WORKING)
- `libraries/AP_HAL_ChibiOS/hwdef/HRON-Chickadee-RC3/hwdef.dat` - Hardware config
- `libraries/AP_HAL_ChibiOS/SPIDevice.cpp` - SPI implementation

## Test Scripts

**IMPORTANT: Minimize board flashing due to write cycle concerns**

- `./build.sh` - Builds firmware using Docker
- `./flash.sh` - Flashes to hardware via ST-LINK
- `./monitor.sh` - Resets the device, requests barometer MAVLink streams, and prints raw plus scaled pressure/temperature output

Always use Docker for builds. If flash fails, check for stuck processes: `pkill -9 -f STM32`

## Implementation Summary

The ArduPilot driver now matches Betaflight's implementation:

1. **SPI transactions**: All use `transfer_fullduplex()` for proper 3-byte full-duplex
2. **Timing**: MODE_SELECT_LATCH_US (200µs) applied unconditionally after mode writes
3. **Boot sequence**: Handles B2 variant detection, OTP calibration for non-B2
4. **Configuration**: Mode 1 (120Hz ODR), normal power, continuous measurement
5. **FIR warmup**: Waits for and discards 14 initial samples
6. **Timer**: 25ms interval for optimal sample collection at 120Hz
7. **I2C compatibility**: Fully preserved, all SPI changes are isolated
8. **Ground telemetry**: `monitor.sh` and `mavlink_monitor.py` now request SCALED_PRESSURE and RAW_PRESSURE MAVLink streams to expose raw ICP201XX pressure and temperature data for validation
9. **Serial readiness**: `mavlink_monitor.py` now waits up to 10 seconds for `/dev/ttyACM0` (or the specified device) to enumerate before configuring the port, preventing lost data immediately after reset

## Notes

- Chip ID: 0x63 (verified on hardware, NOT 0x3C which is the read command)
- Version: 0x00 or 0xB2 (both valid, boot sequence differs)
- SPI protocol: Data always appears in 3rd byte of response
- Full-duplex SPI is MANDATORY for this sensor
- Betaflight implementation confirmed working on same hardware
- ArduPilot commits:
  - 318dd27d96: SPI full-duplex fix (chip ID working)
  - ad5cb6cea0: Complete driver implementation
- Board has limited write cycles remaining - test carefully