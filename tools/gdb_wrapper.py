#!/usr/bin/env python3
"""
GDB Wrapper with Script Execution Capabilities

This wrapper provides robust process management for GDB with script execution
capabilities for automated debugging. It's designed specifically for debugging
the ICP201XX SPI barometer on the HRON-Chickadee-RC3 board.

Features:
- Automatic connection to running OpenOCD instance
- Script execution for automated debugging
- Interactive mode support
- Configurable timeouts
- Proper cleanup on exit or error
"""

import argparse
import os
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path


class GDBWrapper:
    """Wrapper for GDB with script execution and process management."""

    def __init__(
        self, target="localhost:3333", openocd_pid=None, log_file=None, debug=False
    ):
        """
        Initialize GDB wrapper.

        Args:
            target: GDB target in format host:port
            openocd_pid: PID of running OpenOCD process
            log_file: Path to log file for output
            debug: Enable debug output
        """
        self.target = target
        self.openocd_pid = openocd_pid
        self.log_file = log_file
        self.debug = debug
        self.process = None
        self.start_time = None
        self.max_runtime = 300  # seconds (5 minutes)

        # Parse target
        try:
            self.host, self.port = target.split(":")
            self.port = int(self.port)
        except ValueError:
            raise ValueError(f"Invalid target format: {target}. Expected host:port")

    def _log(self, message):
        """Log message to console and optionally to file."""
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
        msg = f"[{timestamp}] {message}"
        print(msg)

        if self.log_file:
            with open(self.log_file, "a") as f:
                f.write(msg + "\n")

    def _wait_for_openocd(self, timeout=30):
        """Wait for OpenOCD to be ready."""
        self._log(f"Waiting for OpenOCD at {self.host}:{self.port}")

        start_time = time.time()
        while time.time() - start_time < timeout:
            try:
                sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                sock.settimeout(1)
                result = sock.connect_ex((self.host, self.port))
                sock.close()

                if result == 0:
                    self._log(f"OpenOCD is ready at {self.host}:{self.port}")
                    return True

                time.sleep(1)

            except Exception as e:
                self._log(f"Error checking OpenOCD connection: {e}")
                time.sleep(1)

        self._log(f"Timeout waiting for OpenOCD at {self.host}:{self.port}")
        return False

    def start(self):
        """Start GDB process."""
        if self.process and self.process.poll() is None:
            self._log("GDB is already running")
            return False

        # Wait for OpenOCD if specified
        if not self._wait_for_openocd():
            return False

        cmd = [
            "gdb-multiarch",
            "--ex",
            f"target remote {self.target}",
            "--ex",
            "set pagination off",
            "--ex",
            "set print pretty on",
        ]

        self._log(f"Starting GDB: {' '.join(cmd)}")
        self.start_time = time.time()

        try:
            self.process = subprocess.Popen(
                cmd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                universal_newlines=True,
                bufsize=1,
            )

            self._log(f"GDB started with PID {self.process.pid}")
            return True

        except Exception as e:
            self._log(f"Failed to start GDB: {e}")
            return False

    def stop(self):
        """Stop GDB process."""
        if not self.process:
            self._log("GDB process not found")
            return False

        if self.process.poll() is not None:
            self._log("GDB process already terminated")
            return True

        try:
            self._log("Stopping GDB...")
            self.process.terminate()

            # Wait for graceful shutdown
            try:
                self.process.wait(timeout=5)
                self._log("GDB terminated gracefully")
            except subprocess.TimeoutExpired:
                self._log("GDB didn't terminate gracefully, killing...")
                self.process.kill()
                self.process.wait()
                self._log("GDB killed")

            return True

        except Exception as e:
            self._log(f"Error stopping GDB: {e}")
            return False

    def is_running(self):
        """Check if GDB process is running."""
        return self.process and self.process.poll() is None

    def send_command(self, command, wait_for_prompt=True, timeout=10):
        """
        Send command to GDB and optionally wait for prompt.

        Args:
            command: Command to send
            wait_for_prompt: Wait for GDB prompt after command
            timeout: Timeout for waiting for prompt

        Returns:
            Tuple of (success, output)
        """
        if not self.is_running():
            self._log("GDB is not running")
            return False, ""

        try:
            self._log(f"GDB> {command}")

            # Send command
            self.process.stdin.write(command + "\n")
            self.process.stdin.flush()

            if not wait_for_prompt:
                return True, ""

            # Read output until we see a prompt
            output = ""
            start_time = time.time()

            while time.time() - start_time < timeout:
                try:
                    line = self.process.stdout.readline()
                    if not line:
                        break

                    output += line
                    self._log(f"GDB: {line.strip()}")

                    # Check for prompt
                    if "(gdb)" in line:
                        return True, output

                except Exception as e:
                    self._log(f"Error reading GDB output: {e}")
                    break

            self._log(f"Timeout waiting for GDB prompt")
            return False, output

        except Exception as e:
            self._log(f"Error sending GDB command: {e}")
            return False, ""

    def execute_script(self, script_file, timeout=60):
        """
        Execute a GDB script file.

        Args:
            script_file: Path to script file
            timeout: Maximum time to execute script

        Returns:
            True if successful, False otherwise
        """
        if not Path(script_file).exists():
            self._log(f"Script file not found: {script_file}")
            return False

        self._log(f"Executing GDB script: {script_file}")

        try:
            with open(script_file, "r") as f:
                script_content = f.read()

            # Split script into commands
            commands = [
                cmd.strip()
                for cmd in script_content.split("\n")
                if cmd.strip() and not cmd.strip().startswith("#")
            ]

            for command in commands:
                success, _ = self.send_command(command, timeout=timeout / len(commands))
                if not success:
                    self._log(f"Failed to execute command: {command}")
                    return False

            self._log("Script execution completed successfully")
            return True

        except Exception as e:
            self._log(f"Error executing script: {e}")
            return False

    def interactive(self):
        """Enter interactive mode."""
        if not self.is_running():
            self._log("GDB is not running")
            return False

        self._log("Entering interactive mode. Type 'quit' to exit.")

        try:
            # Forward GDB output to console
            while self.is_running():
                try:
                    line = self.process.stdout.readline()
                    if not line:
                        break

                    print(line.strip())

                except Exception as e:
                    self._log(f"Error in interactive mode: {e}")
                    break

        except KeyboardInterrupt:
            self._log("Interrupted by user")

        return True

    def __enter__(self):
        """Context manager entry."""
        self.start()
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit."""
        self.stop()


def main():
    """Main entry point for the GDB wrapper."""
    parser = argparse.ArgumentParser(description="GDB Wrapper with Script Execution")
    parser.add_argument(
        "--target",
        "-t",
        default="localhost:3333",
        help="GDB target in format host:port",
    )
    parser.add_argument(
        "--openocd-pid", "-p", type=int, help="PID of running OpenOCD process"
    )
    parser.add_argument("--log", "-l", help="Log file for output")
    parser.add_argument(
        "--debug", "-d", action="store_true", help="Enable debug output"
    )
    parser.add_argument("--script", "-s", help="Script file to execute")
    parser.add_argument(
        "--timeout",
        type=int,
        default=60,
        help="Timeout for script execution in seconds",
    )
    parser.add_argument(
        "--interactive",
        "-i",
        action="store_true",
        help="Enter interactive mode after commands",
    )

    args = parser.parse_args()

    try:
        with GDBWrapper(
            target=args.target,
            openocd_pid=args.openocd_pid,
            log_file=args.log,
            debug=args.debug,
        ) as gdb:
            success = True

            # Execute script if specified
            if args.script:
                success = gdb.execute_script(args.script, timeout=args.timeout)
                if not success:
                    return 1

            # Enter interactive mode if requested
            if args.interactive:
                gdb.interactive()

        return 0 if success else 1

    except KeyboardInterrupt:
        print("\nInterrupted by user")
        return 1
    except Exception as e:
        print(f"Error: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
