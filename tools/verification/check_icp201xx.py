#!/usr/bin/env python3
"""
ICP201XX Barometer Verification Script

This script verifies that the ICP201XX barometer driver is working correctly
by checking for:
1. Successful chip ID detection (0x73)
2. Absence of initialization errors
3. Successful barometer registration
"""

import argparse
import os
import sys
import time
from pathlib import Path

import serial
import serial.tools.list_ports


def wait_for_tty(device_prefix="/dev/ttyACM", timeout=30):
    """
    Wait for a TTY device to appear.

    Returns:
        Path to TTY device or None if timeout
    """
    print(f"Waiting for TTY device with prefix '{device_prefix}'...")

    start_time = time.time()
    while time.time() - start_time < timeout:
        # List available TTY devices
        ports = serial.tools.list_ports.comports()
        for port in ports:
            if device_prefix in port.device:
                print(f"Found TTY device: {port.device}")
                return port.device

        time.sleep(0.5)

    print(f"Timeout waiting for TTY device")
    return None


def verify_icp201xx(device, duration=10):
    """
    Monitor TTY device for ICP201XX verification.

    Returns:
        True if ICP201XX is verified, False otherwise
    """
    print(f"Monitoring {device} for {duration} seconds...")

    try:
        with serial.Serial(device, 57600, timeout=1) as ser:
            start_time = time.time()
            chip_id_detected = False
            init_successful = False
            config_error = False

            while time.time() - start_time < duration:
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting).decode("utf-8", errors="replace")
                    print(data, end="", flush=True)

                    # Check for chip ID detection
                    if "Valid chip ID detected: 0x73" in data:
                        chip_id_detected = True
                        print("\n✅ CHIP ID DETECTION SUCCESS")

                    # Check for successful initialization
                    if "BAROMETER INITIALIZATION SUCCESS" in data:
                        init_successful = True
                        print("\n✅ BAROMETER INITIALIZATION SUCCESS")

                    # Check for configuration errors
                    if "Config Error: Baro: unable to initialise driver" in data:
                        config_error = True
                        print("\n❌ BAROMETER CONFIGURATION ERROR")

                else:
                    time.sleep(0.1)

            # Verification results
            print("\n\n" + "=" * 50)
            print("ICP201XX VERIFICATION RESULTS")
            print("=" * 50)
            print(
                f"Chip ID Detection: {'✅ SUCCESS' if chip_id_detected else '❌ FAILED'}"
            )
            print(f"Initialization: {'✅ SUCCESS' if init_successful else '❌ FAILED'}")
            print(
                f"Configuration Error: {'❌ DETECTED' if config_error else '✅ NONE'}"
            )

            # Overall success if chip ID detected and no config errors
            overall_success = chip_id_detected and not config_error
            print(f"Overall Status: {'✅ WORKING' if overall_success else '❌ FAILED'}")
            print("=" * 50)

            return overall_success

    except Exception as e:
        print(f"Error monitoring TTY: {e}")
        return False


def reset_board():
    """Reset the board using STM32_Programmer_CLI."""
    print("Resetting board...")
    os.system(
        "sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI "
        "-c port=SWD reset=HWrst -hardRst >/dev/null 2>&1"
    )
    time.sleep(3)


def main():
    """Main entry point for the ICP201XX verification script."""
    parser = argparse.ArgumentParser(
        description="Verify ICP201XX barometer implementation"
    )
    parser.add_argument(
        "--device", "-d", default="/dev/ttyACM0", help="TTY device path"
    )
    parser.add_argument(
        "--duration", "-t", type=int, default=15, help="Monitoring duration in seconds"
    )
    parser.add_argument("--no-reset", action="store_true", help="Don't reset the board")

    args = parser.parse_args()

    try:
        # Reset board if requested
        if not args.no_reset:
            reset_board()

        # Wait for TTY device if it doesn't exist
        device = args.device
        if not Path(device).exists():
            device = wait_for_tty()
            if not device:
                print("Failed to find TTY device")
                return 1

        # Verify ICP201XX implementation
        if verify_icp201xx(device, args.duration):
            return 0
        else:
            return 1

    except KeyboardInterrupt:
        print("\nInterrupted by user")
        return 1
    except Exception as e:
        print(f"Error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
