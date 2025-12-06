# ST-LINK/V2 Recovery Procedure

## Problem: ST-LINK Connection Lost

**Symptoms:**
- `st-info --probe` returns "Found 0 stlink programmers"
- `STM32_Programmer_CLI` shows "ST-LINK error (DEV_CONNECT_ERR)"
- USB device (0483:3748) visible in lsusb but not responding to programming tools
- OpenOCD connection attempts hang or fail

**Root Cause:** Hanging OpenOCD Process

The primary cause was a hanging OpenOCD process that had locked the ST-LINK device, preventing other tools from accessing it.

## Recovery Steps - WORKING SOLUTION

### Step 1: Kill Hanging Processes
```bash
# Find and kill any hanging OpenOCD or STM32 processes
ps aux | grep -E "(openocd|stm32|stlink|gdb)" | grep -v grep

# Kill hanging processes (example PID from above command)
pkill -f openocd
```

### Step 2: USB Driver Refresh
```bash
# Refresh USB device rules and triggers
sudo udevadm control --reload-rules && sudo udevadm trigger

# Optional: Refresh USB storage drivers (helps sometimes)
sudo modprobe -r usb_storage && sudo modprobe usb_storage
```

### Step 3: Use STM32_Programmer_CLI First
The STM32_Programmer_CLI tool seems more robust at recovering the ST-LINK connection:
```bash
sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=SWD reset=HWrst
```

**Expected successful output:**
```
ST-LINK SN  : Unexpected_SN_Format
ST-LINK FW  : V2J45S7
Board       : --
Voltage     : 3.27V
SWD freq    : 4000 KHz
Connect mode: Under Reset
Reset mode  : Hardware reset
Device ID   : 0x450
Revision ID : Rev V
Device name : STM32H7xx
Flash size  : 2 MBytes
Device type : MCU
Device CPU  : Cortex-M7
BL Version  : 0x91
```

### Step 4: Verify ST-LINK Recovery
```bash
# Test basic ST-LINK functionality
st-info --probe
```

**Expected successful output:**
```
Found 1 stlink programmers
  version:    V2J45S7
  serial:     430037000A0000363132524E
  flash:      2097152 (pagesize: 131072)
  sram:       131072
  chipid:     0x0450
  descr:      H74x/H75x
```

## What Did NOT Work

### USB Power Cycling Alone
```bash
# These methods did NOT solve the hanging process issue:
sudo uhubctl -f -l 2 -a cycle -d 10     # 10-second delay
sudo uhubctl -f -l 2 -a off && sleep 15 && sudo uhubctl -f -l 2 -a on  # Extended power cycle
```

USB power cycling was ineffective because the hanging OpenOCD process remained running.

### Module Reloads
```bash
# These did NOT solve the core issue:
sudo modprobe -r stlink && sudo modprobe stlink  # Module not loaded anyway
```

## Key Lessons

1. **Process Cleanup is Critical:** Always check for and kill hanging OpenOCD processes first
2. **STM32_Programmer_CLI is More Resilient:** It can often establish connections when other tools cannot
3. **USB Power Cycling Without Process Cleanup is Useless:** The hanging process prevents recovery regardless of USB state
4. **udevadm Trigger Helps:** Refreshing USB device rules after process cleanup improves reliability

## Preventive Measures

### Clean Development Workflow
1. Always kill OpenOCD processes before starting new debug sessions:
   ```bash
   pkill -f openocd
   ```

2. Check for hanging processes before starting development:
   ```bash
   ps aux | grep -E "(openocd|stm32|stlink|gdb)" | grep -v grep
   ```

3. Use proper OpenOCD shutdown:
   ```bash
   # Instead of Ctrl+C, use proper shutdown
   echo "shutdown" | nc localhost 4444
   # or kill the process cleanly
   pkill -f openocd
   ```

### Connection Recovery Sequence (Standard)
If ST-LINK becomes unresponsive:
```bash
# 1. Kill processes
pkill -f openocd

# 2. Refresh USB subsystem  
sudo udevadm control --reload-rules && sudo udevadm trigger

# 3. Test with STM32 tool first
sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=SWD reset=HWrst

# 4. Verify with st-info
st-info --probe
```

## Hardware Context
- **ST-LINK/V2**: USB ID 0483:3748
- **Target Device**: STM32H743 (chipid 0x0450)
- **Target Board**: HRON-Chickadee flight controller
- **Connection**: SWD (Serial Wire Debug)

## Summary
The ST-LINK recovery was achieved by:
1. **Killing hanging OpenOCD process** (primary solution)
2. **Refreshing USB device rules** (secondary support)  
3. **Using STM32_Programmer_CLI** for initial connection test
4. **Verifying with st-info** to confirm full recovery

The root cause was software (hanging process) rather than hardware, which explains why USB power cycling alone was ineffective.