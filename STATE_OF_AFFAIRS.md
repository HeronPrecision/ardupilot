# ArduPilot HRON-Chickadee Project Status

## Current Status - SPI TRANSFER METHOD IDENTIFIED AS ROOT CAUSE

Discovered fundamental issue with ArduPilot's SPI transfer implementation for ICP201XX chip ID reading.

### Critical Finding - SPI Transfer API Mismatch

**Betaflight (WORKING):**
- Sends: `[0x3C, 0x0C, 0xFF]` via full-duplex SPI
- Receives: `[0xFF, 0xFF, 0x63]`
- Extracts chip ID from byte 2: **0x63** ✓ CORRECT

**ArduPilot (NOT WORKING):**
- Calls: `dev->transfer(chip_id_tx, 3, chip_id_rx, 3)`
- This creates a 6-byte transaction instead of 3-byte full-duplex
- ArduPilot's transfer() combines TX and RX into single buffer when lengths match but buffers differ
- Receives: `[0x03, 0x00, 0x00]` - wrong data due to incorrect transaction structure
- Chip ID read fails with 0x03 instead of 0x63

**Root Cause:**
ArduPilot's `SPIDevice::transfer(send, send_len, recv, recv_len)` when `send_len == recv_len` but `send != recv`:
1. Creates buffer of size `send_len + recv_len` (6 bytes in this case)
2. Copies send data to first half: `buf[0:3] = [0x3C, 0x0C, 0xFF]`
3. Zeros second half: `buf[3:6] = [0x00, 0x00, 0x00]`
4. Does 6-byte SPI transfer instead of 3-byte
5. Copies `buf[3:6]` to recv buffer
6. This is NOT what the ICP201XX expects

**Solution:**
Must use `dev->transfer_fullduplex(buf, 3)` instead, which performs true 3-byte full-duplex SPI transaction.

### Code Location for Fix

File: `ardupilot/libraries/AP_HAL_ChibiOS/SPIDevice.cpp`

The problematic code path (lines 289-310):
```cpp
bool SPIDevice::transfer(const uint8_t *send, uint32_t send_len,
                         uint8_t *recv, uint32_t recv_len)
{
    // ... when send_len == recv_len and send != recv:
    uint8_t buf[send_len+recv_len];  // Creates 6-byte buffer!
    if (send_len > 0) {
        memcpy(buf, send, send_len);
    }
    if (recv_len > 0) {
        memset(&buf[send_len], 0, recv_len);
    }
    bool ret = do_transfer(buf, buf, send_len+recv_len);  // 6-byte transfer
    if (ret && recv_len > 0) {
        memcpy(recv, &buf[send_len], recv_len);  // Copies wrong data
    }
    return ret;
}
```

The correct approach (lines 314-324):
```cpp
bool SPIDevice::transfer_fullduplex(const uint8_t *send, uint8_t *recv, uint32_t len)
{
    uint8_t buf[len];
    memcpy(buf, send, len);
    bool ret = do_transfer(buf, buf, len);  // Proper full-duplex
    if (ret) {
        memcpy(recv, buf, len);
    }
    return ret;
}
```

## Hardware Configuration

### STM32H743VIT6 Device
- Flash Size: 2MB
- Voltage: 3.27V
- Programming Interface: ST-LINK via SWD

### Barometer Sensor
- Model: ICP201XX
- Interface: SPI Mode 3 (CPOL=1, CPHA=1) on bus 4
- Expected Chip ID: 0x63
- Current Status: Reading 0x03 due to incorrect SPI transfer method
- SPI Frequency: 6 MHz max

## Betaflight Reference Implementation

Betaflight's working implementation (verified on hardware):
- SPI Mode 3 (CPOL=1, CPHA=1) - confirmed working
- Full-duplex SPI transactions via `spiSequence()` with `busSegment_t` structures
- Read command: 0x3C
- Write command: 0x33 (note: different from datasheet's 0x34)
- Dummy read after each register access (except EMPTY register 0x00)
- 3-byte transactions: [CMD, REG, DATA/DUMMY]
- **Data appears in third byte of response** - this is the key

Debug output from Betaflight shows:
```
icp201xxReadReg: reg=0x0C len=1 tx=[0x3C,0x0C,0xFF...] rx=[0xFF,0xFF,0x63...] data=0x63
```

## Next Steps

1. **Fix all SPI transactions in AP_Baro_ICP201XX.cpp**
   - Replace `dev->transfer(tx, 3, rx, 3)` with `dev->transfer_fullduplex(buf, 3)`
   - Ensure single buffer is used for TX/RX
   - Extract data from buf[2] (third byte)
   - Apply to chip ID read, register reads, and register writes

2. **Test chip ID read first**
   - Modify only the chip ID reading code initially
   - Build, flash, monitor
   - Verify chip ID now reads 0x63 instead of 0x03
   - Only proceed once this works

3. **Complete remaining driver implementation**
   - Implement FIFO-based continuous reading mode (like Betaflight)
   - Add OTP calibration data reading
   - Implement proper pressure/temperature calculations from FIFO data
   - Add boot sequence based on version detection (B2 vs non-B2)

4. **Compare SPI abstractions thoroughly**
   - Betaflight uses busSegment_t + spiSequence()
   - ArduPilot uses transfer() and transfer_fullduplex()
   - Understand exact byte-level behavior of each
   - Ensure ArduPilot driver mirrors Betaflight's actual SPI transactions

## Key Files

- `libraries/AP_Baro/AP_Baro_ICP201XX.cpp` - Driver needing SPI fixes (current version uses incorrect transfer API)
- `libraries/AP_Baro/AP_Baro_ICP201XX.h` - Driver header
- `libraries/AP_HAL_ChibiOS/SPIDevice.cpp` - SPI transfer implementation (lines 289-324)
- `betaflight/src/main/drivers/barometer/barometer_icp201xx.c` - Reference implementation
- `libraries/AP_HAL_ChibiOS/hwdef/HRON-Chickadee/hwdef.dat` - Hardware config (SPI Mode 3)

## Test Scripts

All testing uses these three scripts only:
- `./build.sh` - Builds firmware using Docker
- `./flash.sh` - Flashes to hardware via ST-LINK
- `./monitor.sh` - Resets and monitors serial output

**Important:** Never run `./waf` directly - always use Docker via build.sh. If flash fails, check for stuck STM32 programmer processes: `pkill -9 -f STM32`

## Important Notes

- **Never assume success without hardware verification**
- The chip ID is definitely NOT 0x3C (that's the read command)
- The chip ID is positively NOT 0x03 (current incorrect reading from bad SPI)
- Expected chip ID is 0x63 (verified in Betaflight output)
- SPI Mode 3 is confirmed correct (matches Betaflight)
- The ICP201XX has a "strange SPI transaction method" - data always in 3rd byte of response
- **Always use full-duplex SPI (transfer_fullduplex) for this sensor**
- Betaflight confirmed working with exact same hardware
- Current ArduPilot code at commit c5d1d465313dc9492020f53a90a03abbb6c731bb

## Recommendation

Make minimal changes to test the SPI fix theory:
1. Change ONLY the chip ID read to use transfer_fullduplex
2. Build and test
3. If successful (reads 0x63), proceed with fixing remaining SPI calls
4. Add heavy debug logging to compare byte-for-byte with Betaflight
5. Use Betaflight's usbCdcPrintf for reference implementation debugging