# HRON-Chickadee SWD Debugging Tips & Commands

## Overview
This guide covers how to use the SWD debugging setup for the HRON-Chickadee flight controller. The SWD debugging system is fully operational and allows for low-level debugging of both the bootloader and main firmware.

## 🛠️ Required Tools

### Install Dependencies
```bash
# Install netcat for OpenOCD communication
sudo apt install netcat-openbsd

# Install multi-architecture GDB for ARM debugging
sudo apt install gdb-multiarch

# Install Python (should already be available)
python3 --version
```

## 🚀 Quick Start SWD Debugging

### Method 1: Python Debugger (Recommended)
```bash
# Start OpenOCD
openocd -f /home/user/ardupilot/hron_chickadee_swd.cfg &
OPENOCD_PID=$!

# Wait for connection
sleep 5

# Run the Python debugger
python3 /home/user/ardupilot/debug_gdb.py

# Clean up
kill $OPENOCD_PID
```

### Method 2: Manual GDB Session
```bash
# Start OpenOCD in background
openocd -f /home/user/ardupilot/hron_chickadee_swd.cfg &

# Start GDB
gdb-multiarch

# In GDB, connect to target:
(gdb) target remote localhost:3333
(gdb) monitor halt
(gdb) info registers pc
(gdb) x/16x 0x08000000
```

### Method 3: OpenOCD Command Line
```bash
# Start OpenOCD with immediate commands
openocd -f /home/user/ardupilot/hron_chickadee_swd.cfg \
  -c "init" \
  -c "halt" \
  -c "reg pc" \
  -c "mdw 0x08000000 16" \
  -c "shutdown"
```

## 📋 Common SWD Commands

### GDB Commands
```gdb
# Connect to target
target remote localhost:3333

# Halt the device
monitor halt

# Reset the device
monitor reset

# Read registers
info registers
info registers pc sp lr

# Read memory
x/16x 0x08000000    # Read 16 words from flash start
x/32x 0x20000000    # Read 32 words from RAM start
x/4i 0x08000004     # Disassemble 4 instructions

# Set breakpoints
break *0x08000200    # Set breakpoint at address
break main          # Set breakpoint at function

# Continue execution
continue
stepi               # Step one instruction
nexti               # Next instruction (skip calls)

# Monitor commands
monitor flash banks
monitor init
```

### OpenOCD Telnet Commands (Port 4444)
```
# Connect via telnet
telnet localhost 4444

# Basic commands
halt                    # Halt the target
resume                  # Resume execution
reset                   # Reset the target

# Memory operations
mdw 0x08000000 16      # Read memory words
mww 0x20000000 0x12345678 # Write memory word
mhb 0x20000000 0x12     # Write memory half-word
mbb 0x20000000 0x34     # Write memory byte

# Register operations
reg pc                   # Read program counter
reg pc 0x08001000       # Set program counter

# Flash operations
flash write_bank erase firmware.bin 0x08000000
flash info

# Debug control
bp 0x08001000 hw        # Set hardware breakpoint
rbp 0x08001000 hw       # Remove breakpoint
```

## 🔍 Debugging Workflows

### 1. Bootloader Analysis
```bash
# Start debugging session
openocd -f hron_chickadee_swd.cfg &
sleep 5
python3 debug_gdb.py

# Check bootloader vector table
(gdb) x/32x 0x08000000

# Verify reset vector
(gdb) x/i 0x08000004

# Read initial stack pointer
(gdb) x/x 0x08000000
```

### 2. Main Firmware Debugging
```bash
# Flash firmware, then debug:
openocd -f hron_chickadee_swd.cfg &
gdb-multiarch your_firmware.elf
(gdb) target remote localhost:3333
(gdb) load your_firmware.elf
(gdb) break main
(gdb) continue
```

### 3. Reset and Startup Analysis
```bash
# Monitor reset sequence
gdb-multiarch
(gdb) target remote localhost:3333
(gdb) monitor reset
(gdb) monitor halt
(gdb) info registers pc
(gdb) x/10i 0x08000000
```

## 📊 Memory Map Reference

### STM32H743 Memory Regions
- **Flash Memory:** 0x08000000 - 0x08200000 (2MB)
- **SRAM D1:** 0x24000000 - 0x24080000 (512KB)
- **SRAM D2:** 0x30000000 - 0x30040000 (256KB)
- **SRAM D3:** 0x38000000 - 0x38010000 (64KB)
- **ITCM RAM:** 0x00000000 - 0x00040000 (256KB)
- **Backup SRAM:** 0x38800000 - 0x38810000 (4KB)

### Vector Table Layout (0x08000000)
```
0x08000000: Initial Stack Pointer
0x08000004: Reset Vector (Reset_Handler)
0x08000008: NMI_Handler
0x0800000C: HardFault_Handler
...
```

## ⚡ Troubleshooting

### Connection Issues
```bash
# Check ST-LINK connection
st-info --probe

# Reset device
st-info --reset

# Check OpenOCD processes
ps aux | grep openocd

# Kill stuck OpenOCD processes
pkill -f openocd
```

### Debug Register Errors
The debug register error (`read_memory: read at 0x5c001004 failed`) is normal for STM32H7 and can be ignored. The connection is still functional.

### Permission Issues
```bash
# Add user to dialout group for USB access
sudo usermod -a -G dialout $USER
# Logout and login again
```

## 🛠️ Advanced Debugging

### 1. Real-Time Variable Watching
```gdb
# Define variable
(gdb) set variable my_counter = 0

# Watch variable
(gdb) watch my_counter
(gdb) continue
```

### 2. Breakpoint Conditions
```gdb
# Conditional breakpoint
(gdb) break main if argc > 1

# Hit counter
(gdb) break *0x08001000
(gdb) ignore 1 10  # Ignore first 10 hits
```

### 3. Memory Write Watching
```gdb
# Watch memory writes
(gdb) watch *0x20000000
```

### 4. Register Manipulation
```gdb
# Modify registers
(gdb) set $pc = 0x08000100
(gdb) set $sp = 0x20000800
```

## 🔧 Custom Configuration

### OpenOCD Configuration
Edit `/home/user/ardupilot/hron_chickadee_swd.cfg`:

```tcl
# Adjust adapter speed for stability
adapter speed 1000

# Enable debug during low power
stm32h7x dbgmcu_enable 0

# Custom reset sequence
reset_config srst_only srst_nogate connect_assert_srst
```

### Python Debugger Customization
Edit `/home/user/ardupilot/debug_gdb.py` to add custom commands or modify the connection logic.

## 📝 Tips & Best Practices

1. **Always halt before reading memory:** `monitor halt`
2. **Use appropriate adapter speed:** Start with 1000kHz
3. **Check device status:** `monitor target list`
4. **Verify target voltage:** Should be ~3.3V
5. **Watch for debug register errors:** Normal for STM32H7
6. **Use hardware breakpoints:** Limited to 8 breakpoints
7. **Monitor stack usage:** Check stack pointer for overflow
8. **Backup original firmware:** Before flashing new builds

## 🚨 BUILD ENVIRONMENT CRITICAL RULES

### DOCKER-ONLY BUILD POLICY - NEVER VIOLATE
**ABSOLUTE RULE:** ALWAYS build using Docker. Native builds permanently corrupt the build environment.

```bash
# ✅ CORRECT - Docker-based build (ALWAYS USE THIS):
docker run --rm -w /ardupilot -v $(pwd):/ardupilot ardupilot:latest \
  python3 ./Tools/scripts/build_bootloaders.py HRON-Chickadee

# ❌ FORBIDDEN - Native build (NEVER DO THIS):
python3 Tools/scripts/build_bootloaders.py HRON-Chickadee
./waf configure --board HRON-Chickadee --bootloader
```

### Build Recovery After Native Build Mistake
If native builds were accidentally run, complete recovery required:

```bash
# 1. Clean corrupted artifacts
rm -rf build/ c4che/ .lock-waf_linux_build config.log

# 2. Reinitialize ALL submodules (critical step)
git submodule update --init --recursive

# 3. Verify Docker builds work
docker run --rm -w /ardupilot -v $(pwd):/ardupilot ardupilot:latest \
  python3 ./Tools/scripts/build_bootloaders.py HRON-Chickadee
```

## 🆘 CRITICAL RECOVERY PROCEDURES

### STM32_Programmer_CLI (STM Tool) Recovery
**Purpose:** Board recovery ONLY (not for development). Use when OpenOCD fails.

```bash
# Full chip erase (recovery from chipid 0x0000):
sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI \
  -c port=SWD reset=HWrst -e all -v fast -q -hardRst

# Flash bootloader hex file:
sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI \
  -c port=SWD reset=HWrst -w /path/to/bootloader.hex -v fast -q -hardRst

# Verify USB enumeration after recovery:
sleep 3 && lsusb | grep 1209:5741
```

**STM Tool Characteristics:**
- ✅ **Recovery Specialist:** Rescues boards from chipid 0x0000 failures
- ✅ **Hardware Reset:** Uses aggressive hardware reset + full erase  
- ✅ **Verified Programming:** Built-in verification ensures success
- ⚠️  **Recovery Only:** Use OpenOCD for debugging, STM tool for recovery

### Board Recovery from Critical Failure State
**Symptoms:** chipid 0x0000, no USB enumeration, OpenOCD connection failures
**Solution:** Use STM32_Programmer_CLI with aggressive reset approach:

```bash
# Full recovery sequence
# 1. Connect ST-LINK, ensure board power
# 2. Use STM32_Programmer_CLI (NOT OpenOCD for recovery)
# 3. Perform full chip erase and reflash bootloader
# 4. Verify USB device appears (1209:5741 for HRON-Chickadee)
# 5. Test SWD connection recovery
```

### USB Power Cycle Recovery
**Use when other recovery attempts have failed** - this command power cycles all USB devices on port 2:

```bash
# USB power cycle (last resort recovery method)
sudo uhubctl -f -l 2 -a cycle
```

**Usage Context:**
- After OpenOCD recovery attempts have failed
- When STM32_Programmer_CLI cannot establish connection
- Before attempting OpenOCD debug or STM Cube flashing again
- Use when board appears completely unresponsive to SWD

**Recovery Sequence with USB Power Cycle:**
1. Try standard OpenOCD connection
2. Attempt STM32_Programmer_CLI recovery
3. If both fail: `sudo uhubctl -f -l 2 -a cycle`
4. Wait 10-15 seconds for USB power restoration
5. Retry OpenOCD connection
6. Retry STM32_Programmer_CLI recovery if needed

## 🚨 CRITICAL LIMITATION: USB Upload Not Functional

### HRON-Chickadee USB Upload Status: NON-FUNCTIONAL

**Current State:** USB enumeration and bootloader upload methods are NOT working for HRON-Chickadee

**Root Cause: USB Devices Not Available in Docker Containers**
- **Fundamental Issue:** Docker containers cannot access host USB devices by default
- **Build Policy:** ALL ArduPilot builds MUST use Docker (per STATE_OF_AFFAIRS.md critical rules)
- **Result:** Upload functionality unavailable even if bootloader USB enumeration worked

**Limitations:**
- [X] **`./waf --upload`:** Will NOT work in Docker - no USB device access
- [X] **`python3 Tools/scripts/uploader.py`:** Will NOT work in Docker - requires USB enumeration
- [X] **Native Builds:** FORBIDDEN - permanently corrupt build environment
- [X] **Traditional ArduPilot upload methods:** NON-FUNCTIONAL due to Docker requirement

**Technical Root Cause:**
```
Docker Build Environment:
├── [OK] Firmware compilation (works)
├── [OK] Build system (works) 
├── [X] USB device access (blocked by Docker isolation)
└── [X] Upload capability (requires USB access)

Native Build Environment:
├── [OK] USB device access (would work)
├── [X] Build system (corrupted if used)
└── [X] Upload capability (build corruption prevents firmware generation)
```

**Working Alternatives:**
- [OK] **SWD Debugging:** Fully operational via OpenOCD and ST-LINK/V2
- [OK] **STM32_Programmer_CLI:** Native host tool for flashing firmware  
- [OK] **Direct firmware flashing:** Using hex files with STM32_Programmer_CLI

**Mandatory Development Workflow:**
1. **Build firmware:** `docker run --rm -w /ardupilot -v $(pwd):/ardupilot ardupilot:latest ./waf plane`
2. **Exit Docker:** Return to host environment for USB operations
3. **Flash firmware:** `sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=SWD reset=HWrst -w /home/user/ardupilot/build/HRON-Chickadee/bin/arduplane_with_bl.hex -v fast -q -hardRst`
4. **Debug via SWD:** `openocd -f /home/user/ardupilot/hron_chickadee_swd.cfg`

**Critical Design Constraint:**
- This is NOT a bug - it's a consequence of the mandatory Docker-only build policy
- The isolation that protects the build environment also prevents USB access
- Alternative flashing methods (SWD/JTAG) are the intended development workflow

### Conservative Incremental Changes Strategy
**Critical Rule:** Make only one change at a time and test immediately:

```bash
# 1. Make single change to hwdef-bl.dat
# 2. Build bootloader: docker run --rm -w /ardupilot -v $(pwd):/ardupilot ardupilot:latest python3 ./Tools/AP_Bootloader/wscript configure build  
# 3. Flash using STM32_Programmer_CLI (reliable method)
# 4. Test USB enumeration: lsusb | grep 1209:5741
# 5. Test SWD connection: openocd -f hron_chickadee_swd.cfg -c "init; shutdown;"
# 6. If any regression: REVERT the change immediately
# 7. Only proceed to next change if all functionality preserved
```

### Docker Build Commands for HRON-Chickadee

```bash
# Build bootloader using Docker (when build system working)
docker run --rm -w /ardupilot -v $(pwd):/ardupilot ardupilot:latest python3 ./Tools/AP_Bootloader/wscript configure build

# Available Docker images
docker images | grep ardupilot
# ardupilot:latest, ardupilot-builder:latest, ardupilot-dev:latest
```

### Expected Bootloader File Sizes
- **Original HRON-Chickadee bootloader:** ~18,364 bytes (.bin)
- **With SWD changes:** Should be ~18,548 bytes (.bin)
- **If files unchanged after build:** Build system not detecting hwdef changes

## 🚨 Safety Notes

- Ensure proper power supply during debugging
- Don't modify critical system registers
- Test on development boards first
- Keep backup of working firmware
- Monitor device temperature during extended sessions
- **CRITICAL:** Use STM32_Programmer_CLI for recovery, not OpenOCD
- **ALWAYS:** Test USB enumeration after each change: `lsusb | grep 1209:5741`
- **NEVER:** Make multiple changes without intermediate testing

## 📚 Reference Materials

- [OpenOCD Documentation](http://openocd.org/doc/)
- [STM32H743 Reference Manual](https://www.st.com/resource/en/reference_manual/dm00171833-stm32h743-stm32h753-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf)
- [ARM Cortex-M7 Debugging](https://developer.arm.com/documentation/100969/0000)

## 🤝 Support

For issues with the HRON-Chickadee SWD debugging:
1. Check the OpenOCD output for error messages
2. Verify ST-LINK connection and drivers
3. Ensure proper power to the target device
4. Review the hardware configuration files