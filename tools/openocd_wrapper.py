#!/usr/bin/env python3
"""
OpenOCD Wrapper with Timeout Handling

This wrapper provides robust process management for OpenOCD with proper timeout
handling to prevent hanging. It's designed specifically for debugging the ICP201XX
SPI barometer on the HRON-Chickadee board.

Features:
- Automatic timeout handling (45s no-data, 5min max runtime)
- Comprehensive logging capabilities
- Context manager support for safe resource handling
- Proper cleanup on exit or error
"""

import argparse
import os
import signal
import subprocess
import sys
import time
from pathlib import Path


class OpenOCDWrapper:
    """Wrapper for OpenOCD with timeout handling and process management."""

    def __init__(self, config_file=None, log_file=None, debug=False):
        """
        Initialize OpenOCD wrapper.

        Args:
            config_file: Path to OpenOCD configuration file
            log_file: Path to log file for output
            debug: Enable debug output
        """
        self.config_file = config_file or "/home/user/ardupilot/hron_chickadee_swd.cfg"
        self.log_file = log_file
        self.debug = debug
        self.process = None
        self.last_output_time = None
        self.start_time = None
        self.max_no_data_time = 45  # seconds
        self.max_runtime = 300  # seconds (5 minutes)

        # Ensure config file exists
        if not Path(self.config_file).exists():
            raise FileNotFoundError(
                f"OpenOCD config file not found: {self.config_file}"
            )

    def _log(self, message):
        """Log message to console and optionally to file."""
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
        msg = f"[{timestamp}] {message}"
        print(msg)

        if self.log_file:
            with open(self.log_file, "a") as f:
                f.write(msg + "\n")

    def start(self):
        """Start OpenOCD process."""
        if self.process and self.process.poll() is None:
            self._log("OpenOCD is already running")
            return False

        cmd = ["openocd", "-f", self.config_file]

        self._log(f"Starting OpenOCD: {' '.join(cmd)}")
        self.start_time = time.time()
        self.last_output_time = time.time()

        try:
            self.process = subprocess.Popen(
                cmd,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                bufsize=1,
            )

            self._log(f"OpenOCD started with PID {self.process.pid}")
            return True

        except Exception as e:
            self._log(f"Failed to start OpenOCD: {e}")
            return False

    def stop(self):
        """Stop OpenOCD process."""
        if not self.process:
            self._log("OpenOCD process not found")
            return False

        if self.process.poll() is not None:
            self._log("OpenOCD process already terminated")
            return True

        try:
            self._log("Stopping OpenOCD...")
            self.process.terminate()

            # Wait for graceful shutdown
            try:
                self.process.wait(timeout=5)
                self._log("OpenOCD terminated gracefully")
            except subprocess.TimeoutExpired:
                self._log("OpenOCD didn't terminate gracefully, killing...")
                self.process.kill()
                self.process.wait()
                self._log("OpenOCD killed")

            return True

        except Exception as e:
            self._log(f"Error stopping OpenOCD: {e}")
            return False

    def is_running(self):
        """Check if OpenOCD process is running."""
        return self.process and self.process.poll() is None

    def monitor(self, timeout=None):
        """
        Monitor OpenOCD process with timeout handling.

        Args:
            timeout: Maximum time to monitor in seconds
                     (default: use max_runtime)

        Returns:
            True if process exits normally, False if timeout occurs
        """
        if not self.is_running():
            self._log("OpenOCD is not running")
            return False

        timeout = timeout or self.max_runtime
        self._log(f"Monitoring OpenOCD with {timeout}s timeout")

        while self.is_running():
            # Check if we've exceeded maximum runtime
            if time.time() - self.start_time > timeout:
                self._log(f"Maximum runtime of {timeout}s exceeded")
                self.stop()
                return False

            # Check for available output
            try:
                line = self.process.stdout.readline()
                if line:
                    self.last_output_time = time.time()
                    self._log(f"OpenOCD: {line.strip()}")

                    # Check for specific messages we care about
                    if "Error:" in line or "failed" in line.lower():
                        self._log(f"Detected error in OpenOCD output")
                else:
                    # Check if we haven't received data in too long
                    if time.time() - self.last_output_time > self.max_no_data_time:
                        self._log(f"No data received for {self.max_no_data_time}s")
                        self.stop()
                        return False

                # Small sleep to prevent CPU spinning
                time.sleep(0.1)

            except Exception as e:
                self._log(f"Error reading from OpenOCD: {e}")
                self.stop()
                return False

        # Process has terminated normally
        self._log(f"OpenOCD exited with code {self.process.returncode}")
        return True

    def __enter__(self):
        """Context manager entry."""
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.stop()


def main():
    """Main entry point for the OpenOCD wrapper."""
    parser = argparse.ArgumentParser(
        description="OpenOCD Wrapper with Timeout Handling"
    )
    parser.add_argument(
        "--config",
        "-c",
        default="/home/user/ardupilot/hron_chickadee_swd.cfg",
        help="OpenOCD configuration file",
    )
    parser.add_argument("--log", "-l", help="Log file for output")
    parser.add_argument(
        "--debug", "-d", action="store_true", help="Enable debug output"
    )
    parser.add_argument(
        "--timeout",
        "-t",
        type=int,
        default=300,
        help="Maximum runtime in seconds (default: 300)",
    )

    args = parser.parse_args()

    try:
        with OpenOCDWrapper(
            config_file=args.config, log_file=args.log, debug=args.debug
        ) as openocd:
            # Run with timeout monitoring
            success = openocd.monitor(timeout=args.timeout)

        return 0 if success else 1

    except KeyboardInterrupt:
        print("\nInterrupted by user")
        return 1
    except Exception as e:
        print(f"Error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
