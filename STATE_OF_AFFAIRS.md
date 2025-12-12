# ArduPilot HRON-Chickadee Project Status

## Current Status - CHIP ID READ WORKING ✓

**BREAKTHROUGH: SPI full-duplex fix successful - chip ID reads 0x63 correctly on hardware**

### Verified Working
- SPI Mode 3 (CPOL=1, CPHA=1) confirmed correct
- Full-duplex SPI transactions using `transfer_fullduplex()` - WORKING
- Chip ID read: **0x63** ✓ VERIFIED ON HARDWARE
- SPI transaction structure: `[0x3C, 0x0C, 0xFF] -> [0xFF, 0xFF, 0x63]`
- Data extraction from byte 2 (third byte) - CORRECT

### The Fix
Changed all SPI operations from `transfer(tx, len, rx, len)` to `transfer_fullduplex(buf, len)`:
- `dummy_reg()`: Uses single buffer for 3-byte transaction
- `read_reg()`: Uses single buffer, extracts data from `buf[2]`
- `write_reg()`: Uses single buffer for [CMD, REG, VAL] transaction

Hardware output confirms:
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

## Next Steps - Complete Driver Implementation

### Phase 1: Core Register Operations (IN PROGRESS)
1. ✓ Chip ID read working
2. ✓ Version register read
3. ✓ SPI full-duplex transactions
4. Implement complete initialization sequence from Betaflight:
   - Soft reset
   - Boot sequence with version detection (B2 vs non-B2)
   - OTP calibration data reading (4 blocks via OTP state machine)
   - Mode configuration

### Phase 2: FIFO-Based Continuous Reading
Following Betaflight's exact implementation:
1. Configure FIFO mode (pressure + temperature interleaved)
2. Set operation mode (Mode 1: 120Hz ODR for high-speed)
3. Enable continuous measurement mode
4. Read FIFO fill level
5. Read FIFO data in 6-byte chunks (3 bytes pressure + 3 bytes temp per sample)
6. Process raw data with OTP calibration coefficients

### Phase 3: Data Processing
1. Extract pressure and temperature from FIFO samples
2. Apply OTP calibration using polynomial calculations
3. Implement trend-weighted averaging (Betaflight feature)
4. Update ArduPilot pressure/temperature at configured rate

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
- `libraries/AP_HAL_ChibiOS/hwdef/HRON-Chickadee/hwdef.dat` - Hardware config
- `libraries/AP_HAL_ChibiOS/SPIDevice.cpp` - SPI implementation

## Test Scripts

**IMPORTANT: Minimize board flashing due to write cycle concerns**

- `./build.sh` - Builds firmware using Docker
- `./flash.sh` - Flashes to hardware via ST-LINK
- `./monitor.sh` - Resets and monitors serial output

Always use Docker for builds. If flash fails, check for stuck processes: `pkill -9 -f STM32`

## Implementation Guidelines

1. **Preserve I2C compatibility**: Keep I2C code paths intact, only modify SPI sections
2. **Follow Betaflight exactly**: Match timing, sequences, register values
3. **Use transfer_fullduplex()**: For all SPI transactions on this sensor
4. **Match register definitions**: Use same names/values as Betaflight where possible
5. **Preserve calibration logic**: Copy OTP reading and calculation methods
6. **Test incrementally**: But minimize flashing - implement fully before testing

## Notes

- Chip ID is NOT 0x3C (that's the read command)
- Chip ID is 0x63 (verified on hardware)
- Version can be 0x00 or 0xB2 (both valid)
- This sensor has unusual SPI protocol - data always in 3rd byte
- Betaflight implementation confirmed working on same hardware
- ArduPilot driver at commit 318dd27d96 (chip ID fix)
- Full-duplex SPI is MANDATORY for this sensor