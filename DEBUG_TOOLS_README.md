# HRON-Chickadee-RC3 Debug Tools

This document describes the debugging tools created for the HRON-Chickadee-RC3 flight controller to help with testing and development of the ICP201XX SPI barometer driver.

## Overview

The HRON-Chickadee-RC3 project required robust debugging tools that wouldn't hang and would properly manage OpenOCD and GDB processes. A suite of Python scripts was developed using uv Python to address these requirements.

## Tools Created

### 1. OpenOCD Wrapper (`tools/openocd_wrapper.py`)

A robust wrapper for OpenOCD with proper process management and logging.

**Features:**
- Automatic process cleanup on exit
- Support for logging OpenOCD output to file
- Context manager support for safe resource handling
- Configurable timeouts:
  - 45-second timeout when no data is received
  - 5-minute maximum runtime limit
- Proper signal handling for graceful shutdown

**Usage:**
```bash
uv run python tools/openocd_wrapper.py [--config CONFIG_FILE] [--log LOG_FILE] [--timeout STARTUP_TIME]
```

### 2. GDB Wrapper (`tools/gdb_wrapper.py`)

A wrapper for GDB with script execution capabilities.

**Features:**
- Automatic connection to running OpenOCD instance
- Script execution for automated debugging
- Interactive mode support
- Configurable timeouts:
  - 45-second timeout when no data is received
  - 5-minute maximum runtime limit
- Verification that OpenOCD is running before connecting
- Proper process cleanup

**Usage:**
```bash
uv run python tools/gdb_wrapper.py [--target HOST:PORT] [--script SCRIPT_FILE] [--openocd-pid PID]
```

### 3. Test Suite (`tools/test_debug.py`)

Comprehensive test suite to verify wrapper functionality.

**Test Cases:**
- OpenOCD startup and shutdown
- GDB connection and execution
- Context manager functionality
- Process cleanup verification
- Log file creation and content

**Usage:**
```bash
uv run python tools/test_debug.py
```

### 4. Simple Barometer Check (`tools/check_baro.py`)

Script to verify ICP201XX barometer detection.

**Features:**
- Automated detection of barometer driver
- Multiple fallback methods for checking detection
- Timeout handling to prevent hanging
- Comprehensive logging
- Support for both direct and interactive GDB methods

**Usage:**
```bash
uv run python tools/check_baro.py
```

### 5. Manual GDB Test (`tools/simple_gdb/manual_test.py`)

Simplified script for manual GDB connection and testing.

**Features:**
- Direct OpenOCD and GDB management
- Simple script execution
- Clear output formatting
- Timeout protection

**Usage:**
```bash
uv run python tools/simple_gdb/manual_test.py
```

## Implementation Details

### Timeout Handling

All tools implement a two-tier timeout system:
1. **No-data Timeout (default: 45 seconds)** - If no data is received for this duration, the tool assumes it's stuck and exits gracefully.
2. **Maximum Runtime Timeout (default: 300 seconds/5 minutes)** - Hard limit on how long the tool can run.

### Process Management

The wrappers ensure proper process lifecycle management:
- Processes are started with proper error checking
- Signals are handled to allow clean shutdown
- Child processes are properly terminated
- Resources are cleaned up on exit

### Logging

Comprehensive logging is implemented to aid debugging:
- OpenOCD output can be directed to a log file
- GDB commands and responses are captured
- Error conditions are clearly reported
- Log files are automatically cleaned up unless specified otherwise

## Common Issues and Solutions

### "Remote connection closed" Error

This error occurs when:
1. OpenOCD fails to initialize properly
2. Target device is not connected
3. SWD connection is unstable

**Solutions:**
- Check physical SWD connections
- Verify target power (3.27V is expected)
- Ensure no other processes are using the SWD interface
- Try resetting the target device

### "No data received" Timeout

This occurs when:
1. GDB commands are not executing
2. OpenOCD is not responding
3. Target is not properly halted

**Solutions:**
- Verify OpenOCD configuration
- Check if target supports the operations requested
- Try manual intervention with interactive GDB

## Future Enhancements

1. **Auto-recovery**: Implement automatic retry logic for transient failures
2. **Barometer-specific commands**: Add specialized commands for ICP201XX testing
3. **GUI interface**: Create a simple GUI for easier debugging
4. **Real-time monitoring**: Add continuous monitoring capabilities

## Integration with ICP201XX Development

These tools were specifically created to help with the ICP201XX SPI barometer driver development for the HRON-Chickadee-RC3 board. The driver implementation requires:

1. **SPI Configuration**: The ICP201XX uses SPI4 with specific chip select (PE3)
2. **Command Structure**: Special command bytes (0x33 for write, 0x3C for read)
3. **Dummy Reads**: Required after each transaction for proper operation
4. **Retry Logic**: Multiple attempts for chip ID reading

The debugging tools provide a clean environment to verify these requirements and test the implementation without manual intervention or hanging processes.