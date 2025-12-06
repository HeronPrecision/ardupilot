#!/usr/bin/env python3
"""
GDB-based SWD Debugger for HRON-Chickadee using OpenOCD
Connects to OpenOCD GDB interface and reads device state
"""

import re
import subprocess
import sys
import time


class GDBSWDDebugger:
    def __init__(self, gdb_path="gdb-multiarch", host="localhost", port=3333):
        self.gdb_path = gdb_path
        self.host = host
        self.port = port
        self.gdb_process = None

    def connect(self):
        """Connect to OpenOCD via GDB"""
        try:
            # Start GDB and connect to OpenOCD
            self.gdb_process = subprocess.Popen(
                [self.gdb_path],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=0,
            )

            # Connect to target
            self.send_command(f"target remote {self.host}:{self.port}")

            # Read initial connection response
            time.sleep(1)
            output = self.read_output()

            if "Connected" in output or "Remote" in output:
                print(f"✅ Connected to OpenOCD GDB server at {host}:{port}")
                return True
            else:
                print(f"❌ Failed to connect to GDB server")
                print(f"Response: {output}")
                return False

        except Exception as e:
            print(f"❌ Error connecting to GDB: {e}")
            return False

    def send_command(self, command):
        """Send command to GDB and return response"""
        if not self.gdb_process:
            return "❌ Not connected to GDB"

        try:
            # Send command
            self.gdb_process.stdin.write(command + "\n")
            self.gdb_process.stdin.flush()

            # Wait for response
            time.sleep(0.5)

            # Read output
            output = self.read_output()
            return output.strip()

        except Exception as e:
            return f"❌ Error executing command '{command}': {e}"

    def read_output(self):
        """Read output from GDB"""
        try:
            if self.gdb_process and self.gdb_process.stdout:
                # Non-blocking read
                import select

                ready, _, _ = select.select([self.gdb_process.stdout], [], [], 0.1)
                if ready:
                    return self.gdb_process.stdout.read(4096)
            return ""
        except:
            return ""

    def read_registers(self):
        """Read key CPU registers"""
        print("\n🔍 Reading CPU Registers:")
        print("-" * 40)

        # Program Counter
        pc_response = self.send_command("info registers pc")
        pc_match = re.search(r"pc\s+(0x[0-9a-fA-F]+)", pc_response)
        if pc_match:
            print(f"Program Counter: {pc_match.group(1)}")

        # Stack Pointer
        sp_response = self.send_command("info registers sp")
        sp_match = re.search(r"sp\s+(0x[0-9a-fA-F]+)", sp_response)
        if sp_match:
            print(f"Stack Pointer: {sp_match.group(1)}")

        # Current Status Register
        xpsr_response = self.send_command("info registers xpsr")
        xpsr_match = re.search(r"xpsr\s+(0x[0-9a-fA-F]+)", xpsr_response)
        if xpsr_match:
            print(f"XPSR: {xpsr_match.group(1)}")

    def read_memory(self, address, words=16):
        """Read memory from specified address"""
        print(f"\n📖 Reading {words} words from 0x{address:08X}:")
        print("-" * 50)

        cmd = f"examine {hex(address)} {words}"
        response = self.send_command(cmd)

        # Format the memory dump
        lines = response.split("\n")
        for line in lines:
            if "0x" in line:
                print(line)

        return response

    def check_bootloader(self):
        """Check what bootloader/firmware is running"""
        print("\n🚀 Bootloader Analysis:")
        print("-" * 30)

        # Read vector table
        self.read_memory(0x08000000, 32)

        # Read initial stack pointer (first word in vector table)
        print(f"\nInitial Stack Pointer (0x08000000):")
        sp_response = self.send_command("examine 0x08000000 1")
        print(sp_response)

        # Read reset vector (second word in vector table)
        print(f"\nReset Vector (0x08000004):")
        reset_response = self.send_command("examine 0x08000004 1")
        print(reset_response)

        return sp_response, reset_response

    def monitor_reset(self):
        """Monitor the device during reset"""
        print("\n🔄 Reset Monitoring:")
        print("-" * 22)

        # Send monitor reset command
        reset_response = self.send_command("monitor reset")
        print(f"Reset command: {reset_response}")

        # Wait a moment
        time.sleep(2)

        # Read current state
        pc_response = self.send_command("info registers pc")
        pc_match = re.search(r"pc\s+(0x[0-9a-fA-F]+)", pc_response)
        if pc_match:
            print(f"PC after reset: {pc_match.group(1)}")

    def check_device_info(self):
        """Get device information"""
        print("\n📱 Device Information:")
        print("-" * 25)

        # Get target info
        target_info = self.send_command("monitor target list")
        print(f"Target info: {target_info}")

        # Check if device is halted
        halt_status = self.send_command("info program")
        print(f"Program status: {halt_status}")

    def close(self):
        """Close connection to GDB"""
        if self.gdb_process:
            try:
                self.gdb_process.stdin.write("quit\n")
                self.gdb_process.stdin.flush()
                self.gdb_process.wait(timeout=5)
                print("\n🔌 Disconnected from GDB")
            except:
                self.gdb_process.terminate()
                print("\n🔌 Forcefully disconnected from GDB")


def main():
    print("🎯 HRON-Chickadee GDB-based SWD Debug Session")
    print("=" * 55)

    debugger = GDBSWDDebugger()

    if not debugger.connect():
        sys.exit(1)

    try:
        # Halt the device
        print("\n⏸️  Halting device...")
        debugger.send_command("monitor halt")

        # Check device info
        debugger.check_device_info()

        # Read registers
        debugger.read_registers()

        # Check bootloader
        debugger.check_bootloader()

        # Monitor reset
        debugger.monitor_reset()

        print("\n✅ SWD debugging session completed successfully!")

    except KeyboardInterrupt:
        print("\n⚠️  Debug session interrupted")
    except Exception as e:
        print(f"\n❌ Error during debug session: {e}")
    finally:
        debugger.close()

        debugger.close()


if __name__ == "__main__":
    main()
