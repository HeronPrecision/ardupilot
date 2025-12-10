#!/usr/bin/env python3
"""
Manual Barometer Testing Script for ICP201XX

This script provides direct testing of the ICP201XX barometer detection
through GDB commands. It's designed to help debug chip ID reading issues.
"""

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path


def run_command(cmd, timeout=30):
    """Run a command with timeout and return output."""
    try:
        result = subprocess.run(
            cmd,
            shell=True,
            check=True,
            text=True,
            timeout=timeout,
            capture_output=True,
        )
        return result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        print(f"Command timed out: {cmd}")
        return "", "Timeout"
    except subprocess.CalledProcessError as e:
        print(f"Command failed: {cmd}")
        return e.stdout, e.stderr


def start_openocd(config_file):
    """Start OpenOCD and return PID."""
    print(f"Starting OpenOCD with config: {config_file}")
    cmd = f"openocd -f {config_file} > /tmp/openocd.log 2>&1 & echo $!"
    stdout, stderr = run_command(cmd)

    if stderr:
        print(f"Error starting OpenOCD: {stderr}")
        return None

    try:
        pid = int(stdout.strip())
        print(f"OpenOCD started with PID {pid}")
        time.sleep(5)  # Give OpenOCD time to start
        return pid
    except ValueError:
        print(f"Failed to parse PID from: {stdout}")
        return None


def stop_openocd(pid):
    """Stop OpenOCD process."""
    if pid:
        print(f"Stopping OpenOCD (PID {pid})")
        run_command(f"kill {pid} 2>/dev/null")
        run_command("pkill -f openocd 2>/dev/null")


def check_barometer_detection():
    """Check if barometer is detected through GDB commands."""
    print("\n=== Testing ICP201XX Barometer Detection ===")

    # Create GDB script for testing
    script_content = """
# Halt the target
monitor halt

# Check if we can read memory
monitor mdw 0x20000000 10

# Reset the target
monitor reset init

# Wait a moment for initialization
monitor sleep 1000

# Try to find the barometer object
# Look for the AP_Baro_ICP201XX instance
info variables *_icp201xx*

# Try to examine the AP_Baro_ICP201XX class
print sizeof(AP_Baro_ICP201XX)

# Look for SPI device instance
info variables *dev*

# Check the device ID register address
# This is where the chip ID would be stored after reading
print/x &AP_Baro_ICP201XX::_dev

# Continue execution
monitor resume
"""

    script_file = "/tmp/test_baro.gdb"
    with open(script_file, "w") as f:
        f.write(script_content)

    print(f"Created GDB script: {script_file}")

    # Run GDB with the script
    print("\nRunning GDB with barometer detection script...")
    cmd = f"gdb-multiarch -ex 'target remote localhost:3333' -ex 'source {script_file}' -ex 'quit' -batch"
    stdout, stderr = run_command(cmd, timeout=60)

    print("\n=== GDB Output ===")
    print(stdout)
    if stderr:
        print("\n=== GDB Errors ===")
        print(stderr)

    # Clean up
    os.remove(script_file)


def check_memory_dump():
    """Check memory regions for barometer data."""
    print("\n=== Memory Dump for Barometer ===")

    # Create GDB script for memory examination
    script_content = """
# Halt the target
monitor halt

# Check stack region for any barometer objects
monitor mdw 0x20000000 100

# Look for potential barometer structures
# Search for the expected chip ID value (0x63 or 0x73)
find 0x20000000, 0x20020000, 0x63
find 0x20000000, 0x20020000, 0x73

# Check for SPI transaction buffers
monitor mdw 0x20000800 50

# Continue execution
monitor resume
"""

    script_file = "/tmp/memory_dump.gdb"
    with open(script_file, "w") as f:
        f.write(script_content)

    print(f"Created memory dump script: {script_file}")

    # Run GDB with the script
    print("\nRunning memory dump script...")
    cmd = f"gdb-multiarch -ex 'target remote localhost:3333' -ex 'source {script_file}' -ex 'quit' -batch"
    stdout, stderr = run_command(cmd, timeout=60)

    print("\n=== Memory Dump Results ===")
    print(stdout)
    if stderr:
        print("\n=== Memory Dump Errors ===")
        print(stderr)

    # Clean up
    os.remove(script_file)


def check_tty_output():
    """Check TTY output for barometer detection messages."""
    print("\n=== Checking TTY Output ===")

    # Find the correct TTY device
    find_tty_cmd = "ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | head -5"
    stdout, stderr = run_command(find_tty_cmd)

    if not stdout:
        print("No USB serial devices found")
        return

    print("Found USB serial devices:")
    print(stdout)

    # Try to read from each device
    devices = [line.split()[-1] for line in stdout.split("\n") if line.strip()]

    for device in devices:
        if not device.startswith("/dev/"):
            continue

        print(f"\nReading from {device} (5 seconds):")
        cmd = f"timeout 5s cat {device} 2>/dev/null || true"
        stdout, stderr = run_command(cmd)

        if stdout:
            print(stdout)

        # Check for barometer messages
        if "ICP201XX" in stdout or "baro" in stdout.lower():
            print(f"\n*** Found barometer output on {device} ***")


def main():
    """Main entry point for the manual test script."""
    parser = argparse.ArgumentParser(
        description="Manual testing script for ICP201XX barometer"
    )
    parser.add_argument(
        "--config",
        "-c",
        default="/home/user/ardupilot/hron_chickadee_swd.cfg",
        help="OpenOCD configuration file",
    )
    parser.add_argument(
        "--no-openocd",
        action="store_true",
        help="Don't start OpenOCD (use existing instance)",
    )
    parser.add_argument(
        "--tty",
        action="store_true",
        help="Check TTY output for barometer messages",
    )

    args = parser.parse_args()

    # Verify config file exists
    if not Path(args.config).exists():
        print(f"OpenOCD config file not found: {args.config}")
        return 1

    openocd_pid = None

    try:
        # Start OpenOCD if requested
        if not args.no_openocd:
            openocd_pid = start_openocd(args.config)
            if not openocd_pid:
                print("Failed to start OpenOCD")
                return 1

        # Run barometer detection tests
        check_barometer_detection()

        # Check memory for barometer data
        check_memory_dump()

        # Check TTY output if requested
        if args.tty:
            check_tty_output()

        print("\n=== Test Complete ===")
        return 0

    except KeyboardInterrupt:
        print("\nInterrupted by user")
        return 1
    except Exception as e:
        print(f"Error: {e}")
        return 1
    finally:
        # Clean up OpenOCD if we started it
        if openocd_pid:
            stop_openocd(openocd_pid)


if __name__ == "__main__":
    sys.exit(main())
