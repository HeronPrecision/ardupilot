#!/usr/bin/env python3
"""
TTY Monitor Script for ICP201XX Debug Output

This script monitors the TTY device for debug output from the ICP201XX barometer.
It waits for the TTY device to appear after reset and then captures the output
for a specified time period.
"""

import argparse
import os
import signal
import sys
import time
from pathlib import Path

import serial

try:
    import serial.tools.list_ports
except ImportError:
    import serial.tools.list_tools as list_ports


def wait_for_tty(device_prefix="/dev/ttyACM", timeout=10):
    """
    Wait for a TTY device to appear.

    Args:
        device_prefix: Prefix of the TTY device to wait for
        timeout: Maximum time to wait in seconds

    Returns:
        Path to the TTY device or None if timeout
    """
    print(f"Waiting for TTY device with prefix '{device_prefix}'...")

    start_time = time.time()
    while time.time() - start_time < timeout:
        # List available TTY devices
        try:
            ports = serial.tools.list_ports.comports()
        except AttributeError:
            ports = list_ports.list_ports()
        for port in ports:
            if device_prefix in port.device:
                print(f"Found TTY device: {port.device}")
                return port.device

        time.sleep(0.1)  # Faster polling

    print(f"Timeout waiting for TTY device")
    return None


def monitor_tty(device, baudrate=57600, duration=10, output_file=None):
    """
    Monitor TTY device for debug output.

    Args:
        device: TTY device path
        baudrate: Serial baud rate
        duration: How long to monitor in seconds
        output_file: Optional file to save output

    Returns:
        True if successful, False otherwise
    """
    try:
        print(f"Opening {device} at {baudrate} baud...")

        with serial.Serial(device, baudrate, timeout=1) as ser:
            print(f"Monitoring {device} for {duration} seconds...")

            # Buffer to accumulate data for text extraction
            buffer = bytearray()
            start_time = time.time()

            while time.time() - start_time < duration:
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting)
                    buffer.extend(data)

                    # Try to extract text from buffer
                    try:
                        text = data.decode("utf-8", errors="replace")
                        print(text, end="", flush=True)

                        # Check for ICP201XX debug messages
                        if "ICP201XX:" in text:
                            print("\n*** Found ICP201XX debug output ***", flush=True)
                    except:
                        # If text decode fails, just continue accumulating buffer
                        pass

                    # Prevent buffer from growing too large
                    if len(buffer) > 1000:
                        buffer = buffer[-1000:]
                else:
                    time.sleep(0.1)

            # Try to decode the entire buffer at the end
            try:
                full_text = buffer.decode("utf-8", errors="replace")

                if output_file:
                    with open(output_file, "w") as f:
                        f.write(full_text)
                    print(f"\nOutput saved to {output_file}")

                return True
            except:
                # Save raw data if text decode fails
                if output_file:
                    with open(output_file, "wb") as f:
                        f.write(buffer)
                    print(f"\nRaw binary output saved to {output_file}")

                return False

    except Exception as e:
        print(f"Error monitoring TTY: {e}")
        return False


def reset_board():
    """Reset the board using STM32_Programmer_CLI."""
    try:
        print("Resetting board...")
        os.system(
            "sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI "
            "-c port=SWD reset=HWrst -hardRst >/dev/null 2>&1"
        )
        time.sleep(1)
        return True
    except Exception as e:
        print(f"Error resetting board: {e}")
        return False


def main():
    """Main entry point for the TTY monitor script."""
    parser = argparse.ArgumentParser(
        description="Monitor TTY output for ICP201XX debug"
    )
    parser.add_argument(
        "--device", "-d", default="/dev/ttyACM0", help="TTY device path"
    )
    parser.add_argument(
        "--baudrate", "-b", type=int, default=57600, help="Serial baud rate"
    )
    parser.add_argument(
        "--duration", "-t", type=int, default=15, help="Duration to monitor in seconds"
    )
    parser.add_argument(
        "--wait", "-w", type=int, default=10, help="Time to wait for device in seconds"
    )
    parser.add_argument("--no-reset", action="store_true", help="Don't reset the board")
    parser.add_argument("--output", "-o", help="Save output to file")

    args = parser.parse_args()

    try:
        # Reset board if requested
        if not args.no_reset:
            if not reset_board():
                return 1

        # Wait for TTY device if it doesn't exist
        device = args.device
        if not Path(device).exists():
            print(f"Device {device} not found, waiting for it to appear...")
            device = wait_for_tty("/dev/ttyACM", args.wait)
            if not device:
                return 1

        # Monitor TTY device
        print(
            f"Starting monitor with baudrate={args.baudrate}, duration={args.duration}s"
        )
        if monitor_tty(device, args.baudrate, args.duration, args.output):
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
