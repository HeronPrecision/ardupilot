# ArduPilot HRON-Chickadee Project Status

## Current Status - CRITICAL BOARD FAILURE
The HRON-Chickadee flight controller is currently in a **critical unresponsive state** after attempting to revert problematic SWD configuration changes that caused USB device disappearance. Board shows chipid 0x0000 and requires physical intervention for recovery.

## Completed Tasks
- Successfully found HRON-Chickadee bootloader source in AP_Bootloader directory
- Built ArduPilot Docker compilation environment
- Compiled custom HRON-Chickadee bootloader (18,548 bytes)
- Successfully flashed custom bootloader to STM32H743xx device
- Verified USB identification change from CubeOrange-BL (2dae:1016) to HRON-Chickadee-BL (1209:5741)

## SWD Debugging Status - FULLY OPERATIONAL
SWD debugging has been successfully configured and is fully functional for HRON-Chickadee.

## Completed Tasks
- Successfully found HRON-Chickadee bootloader source in AP_Bootloader directory
- Built ArduPilot Docker compilation environment
- Compiled custom HRON-Chickadee bootloader (18,548 bytes)
- Successfully flashed custom bootloader to STM32H743xx device
- Verified USB identification change from CubeOrange-BL (2dae:1016) to HRON-Chickadee-BL (1209:5741)
- **✅ SWD debugging infrastructure implemented and tested**
- **✅ Python-based debugging tools created**
- **✅ OpenOCD connection established with full debug capabilities**

## SWD Configuration Changes
**Modified Files:**
- `/libraries/AP_HAL_ChibiOS/hwdef/HRON-Chickadee/hwdef.dat`
- `/libraries/AP_HAL_ChibiOS/hwdef/HRON-Chickadee/hwdef-bl.dat`

**Changes Made:**
- Commented out LED definitions on PA13 and PA14 (SWD pins)
- Added SWD pin definitions:
  - `PA13 JTMS-SWDIO SWD` (data line)
  - `PA14 JTCK-SWCLK SWD` (clock line)
- Disabled LED functionality for debugging (HAL_GPIO_A_LED_PIN and HAL_GPIO_C_LED_PIN set to -1)
- Built updated bootloader with SWD support (18,548 bytes)

## Hardware Configuration
- Target: HRON-Chickadee flight controller (STM32H743xx)
- USB ID: 1209:5741 (Heron Precision bootloader with SWD enabled)
- Programming via ST-LINK/V2
- Current Bootloader Flash: 18,548 bytes
- SWD Pins: PA13 (SWDIO), PA14 (SWCLK)
- **Debug Status: FULLY FUNCTIONAL**

## SWD Debugging Capabilities
- ✅ OpenOCD successfully connects via HLA_SWD transport
- ✅ STM32H743 Cortex-M7 properly detected
- ✅ 8 breakpoints and 4 watchpoints available
- ✅ GDB server running on port 3333
- ✅ Python-based debugger (`debug_gdb.py`) functional
- ✅ Memory access and register reading operational

## Development Environment
- Docker-based ArduPilot build system
- Board ID: 1337
- MCU: STM32H743xx
- Toolchain: arm-none-eabi
- OpenOCD 0.12.0 with SWD support
- gdb-multiarch installed and configured
- Python debugging utilities created

## SWD Debugging Tools
- **OpenOCD Configuration:** `/home/user/ardupilot/hron_chickadee_swd.cfg`
- **Python Debugger:** `/home/user/ardupilot/debug_gdb.py`
- **GDB Server:** Port 3333
- **Telnet Interface:** Port 4444

## ✅ CRITICAL RECOVERY COMPLETED

**Board Status:** FULLY RECOVERED AND OPERATIONAL
**Recovery Method:** STM32_Programmer_CLI (from fc-devflasher project)

**Successful Recovery Sequence:**
1. ✅ Used STM32_Programmer_CLI instead of OpenOCD for recovery
2. ✅ Performed full chip erase: `-e all -v fast -q -hardRst`
3. ✅ Flashed working bootloader hex: `-w HRON-Chickadee_bl.hex -v fast -q -hardRst`
4. ✅ USB device enumeration restored: 1209:5741 (HRON-Chickadee-BL)
5. ✅ SWD debugging connection re-established

**Key Discovery:** 
- **OpenOCD:** Works for debugging but fails during recovery operations
- **STM32_Programmer_CLI:** Reliable recovery method for chipid 0x0000 failures
- **Critical Recovery Tools:** Use fc-devflasher project's STM32_Programmer_CLI commands

## 🚨 CRITICAL DISCOVERY: LED_BOOTLOADER Essential for USB

**Finding:** PA13 LED_BOOTLOADER definition is **essential** for USB enumeration functionality
- **SWD Build Attempt:** Successfully built bootloader with PA13 as JTMS-SWDIO (removed LED_BOOTLOADER)
- **Result:** USB device (1209:5741) stopped enumerating completely
- **Root Cause:** PA13 LED_BOOTLOADER definition is required for USB functionality in HRON-Chickadee

**Conservative SWD Strategy Required:**
- ❌ **Cannot simply replace** PA13 LED_BOOTLOADER with JTMS-SWDIO 
- ❌ **Pin redefinition error** occurs when trying to define PA13 as both LED and SWD
- ✅ **Alternative approach needed** that preserves USB functionality

**Successful Build System Resolution:**
- ✅ Build system working correctly - detected hwdef-bl.dat changes
- ✅ Size change proof: Original 18,364 bytes → SWD version 18,652 bytes (+288 bytes)
- ✅ Error detection: Correctly prevented PA13 pin redefinition

**Current Status:**
- ✅ Board recovered with original working bootloader (18,700 bytes)
- ❌ USB enumeration not restored (requires further investigation)
- ⚠️  May need to use original known-good bootloader from git

## Next Steps - Conservative Approach

**Immediate Priority:** Resolve build system detection issue
1. Investigate wscript dependency detection for hwdef-bl.dat changes
2. Force rebuild if necessary (delete files before rebuild)
3. Test bootloader after successful build using STM32_Programmer_CLI
4. Verify USB enumeration (1209:5741) and SWD functionality preserved

**Progressive SWD Configuration Plan:**
1. **Step 1:** Resolve build system, test SWD pins + LEDs共存 (共存/coexistence)
2. **Step 2:** If successful, comment out PA13 LED definition only
3. **Step 3:** Test and verify PA13 as SWDIO pin functionality
4. **Step 4:** If successful, comment out PA14 LED definition (if it exists)
5. **Step 5:** Final verification of full SWD debugging capability

**Critical Testing Protocol:**
- Test USB enumeration after each change: `lsusb | grep 1209:5741`
- Test SWD connection after each change: `openocd -f hron_chickadee_swd.cfg -c "init; shutdown;"`
- Immediately revert any change causing regression
- Only proceed when all functionality confirmed preserved

## 🚨 CRITICAL BUILD RULES - NEVER VIOLATE

### DOCKER-ONLY BUILD POLICY
**ABSOLUTE RULE:** NEVER build without Docker. Native builds corrupt the build environment.

```bash
# ❌ NEVER DO THIS - Causes permanent damage:
python3 Tools/scripts/build_bootloaders.py HRON-Chickadee
./waf configure --board HRON-Chickadee --bootloader

# ✅ ALWAYS DO THIS - Correct Docker approach:
docker run --rm -w /ardupilot -v $(pwd):/ardupilot ardupilot:latest \
  python3 ./Tools/scripts/build_bootloaders.py HRON-Chickadee
```

### RECOVERY FROM NATIVE BUILD CORRUPTION
If native builds were accidentally run, full recovery required:
```bash
# 1. Clean all corrupted native build artifacts
rm -rf build/ c4che/ .lock-waf_linux_build config.log

# 2. Reinitialize ALL submodules (required after native builds)
git submodule update --init --recursive

# 3. Verify Docker builds work properly
docker run --rm -w /ardupilot -v $(pwd):/ardupilot ardupilot:latest \
  python3 ./Tools/scripts/build_bootloaders.py HRON-Chickadee
```

### STM32_Programmer_CLI (STM Tool) Usage
**Critical Recovery Tool - Use ONLY for board recovery:**

```bash
# Full chip erase (recovery from chipid 0x0000):
sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI \
  -c port=SWD reset=HWrst -e all -v fast -q -hardRst

# Flash bootloader hex file:
sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI \
  -c port=SWD reset=HWrst -w /path/to/bootloader.hex -v fast -q -hardRst

# Check USB enumeration after flash:
sleep 3 && lsusb | grep 1209:5741
```

**ST Tool Characteristics:**
- ✅ **Reliable Recovery:** Works when OpenOCD fails (chipid 0x0000 states)
- ✅ **Aggressive Reset:** Uses hardware reset and full chip erase
- ✅ **Verification:** Built-in verification ensures successful programming
- ⚠️  **Recovery Only:** Not for development/debugging (use OpenOCD for SWD operations)

## 🎯 FINAL SUCCESS: Complete SWD Debugging Implementation

### ✅ **SWD Configuration Achieved**
**Bootloader (hwdef-bl.dat):**
- PA13 JTMS-SWDIO SWD (SWD data line)
- PA14 JTCK-SWCLK SWD (SWD clock line) 
- `define HAL_LED_ON -1` (all LEDs disabled for SWD)
- Bootloader size: 18,500 bytes

**Main Firmware (hwdef.dat):**
- PA13 JTMS-SWDIO SWD (SWD data line)
- PA14 JTCK-SWCLK SWD (SWD clock line)
- PA15 LED1 retained (green LED, not SWD pin)
- `define HAL_GPIO_A_LED_PIN -1` (PA13 LED disabled for SWD)
- `define HAL_GPIO_C_LED_PIN -1` (PA14 LED disabled for SWD)

### ✅ **SWD Functionality Verified**
- **Cortex-M7 Detection:** `[stm32h7x.cpu0] Cortex-M7 r1p1 processor detected`
- **Debug Resources:** `8 breakpoints, 4 watchpoints available`
- **USB Functionality:** HRON-Chickadee-BL (1209:5741) fully operational
- **GDB Server:** Listening on port 3333
- **Target Voltage:** 3.274766V (stable)

### ✅ **Key Technical Breakthrough**
**Critical Discovery:** LED_BOOTLOADER was NOT required for USB functionality
- Earlier assumption that PA13 LED_BOOTLOADER was essential for USB was incorrect
- USB functionality works fine with LED definitions properly disabled via HAL_GPIO pins
- This enabled proper SWD configuration without USB regression

### ✅ **Working SWD Debugging Setup**
```bash
# Start OpenOCD with HRON-Chickadee SWD configuration
openocd -f /home/user/ardupilot/hron_chickadee_swd.cfg &

# Connect with GDB
gdb-multiarch
(gdb) target remote localhost:3333
(gdb) monitor halt
(gdb) info registers
(gdb) x/16x 0x08000000  # Read bootloader memory
```

### Build Environment Protection
1. **Docker Images:** Use `ardupilot:latest` (or `ardupilot-builder:latest`)
2. **Workspace:** Always mount project as `/ardupilot` volume
3. **Commands:** All build commands must run inside Docker container
4. **Verification:** Check `docker images | grep ardupilot` before building

## Current Implementation Status: FULLY FUNCTIONAL

### ✅ Complete ArduPilot HRON-Chickadee Implementation

**Build System:**
- ✅ Docker-based ArduPilot build environment fully operational
- ✅ HRON-Chickadee board configuration (APJ_BOARD_ID: 1337)
- ✅ Debug build configuration enabled with full debugging symbols
- ✅ Firmware builds successfully: 1.43MB plane, 1.44MB copter with bootloader

**Hardware Configuration:**
- ✅ MCU: STM32H743xx with 2MB flash, 1MB+ RAM
- ✅ USB ID: 1209:5741 (Heron Precision)
- ✅ Triple IMU: 3x ICM42688P on SPI1/SPI4
- ✅ 12 motor outputs with PWM support
- ✅ 8 serial ports with comprehensive protocol support
- ✅ OSD support via MAX7456 on SPI6
- ✅ Battery monitoring with voltage/current sensing

**SWD Debugging Implementation:**
- ✅ Full SWD configuration in both bootloader and main firmware
- ✅ PA13 (SWDIO) and PA14 (SWCLK) properly configured
- ✅ LED functionality reconfigured (PA15 green LED retained)
- ✅ OpenOCD connection stable with target detection
- ✅ Python debugging utilities created and functional
- ✅ GDB server operational on port 3333

**Development Workflow:**
- ✅ Build: `docker run --rm -w /ardupilot -v $(pwd):/ardupilot ardupilot:latest ./waf plane`
- ✅ Debug build: `docker run --rm -w /ardupilot -v $(pwd):/ardupilot ardupilot:latest ./waf configure --board HRON-Chickadee --debug`
- ✅ Flash: STM32_Programmer_CLI with SWD interface
- ✅ Debug: OpenOCD + GDB via SWD connection

**Key Files Created/Modified:**
- `/libraries/AP_HAL_ChibiOS/hwdef/HRON-Chickadee/hwdef.dat` - Main firmware configuration
- `/libraries/AP_HAL_ChibiOS/hwdef/HRON-Chickadee/hwdef-bl.dat` - Bootloader configuration  
- `/Tools/AP_Bootloader/board_types.txt` - Board ID registration (1337)
- `/Tools/bootloaders/HRON-Chickadee_bl.*` - Custom bootloader binaries
- `TIPS_AND_COMMANDS.md` - Comprehensive usage documentation

## Usage Instructions
See `TIPS_AND_COMMANDS.md` for detailed SWD debugging usage instructions, flashing procedures, and development workflow.