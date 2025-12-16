#!/usr/bin/env python3
"""
MAVLink Sensor Monitor for HRON-Chickadee
Queries attitude (gyros), altitude (pressure), and heading (compass) via MAVLink
"""

import argparse
import sys
import time

from pymavlink import mavutil


def print_header():
    """Print column headers for sensor data"""
    print("\n" + "=" * 100)
    print(
        f"{'TIME':<12} {'ROLL':>8} {'PITCH':>8} {'YAW':>8} {'ROLLSPD':>9} {'PITCHSPD':>9} {'YAWSPD':>9} {'ALT_REL':>9} {'ALT_ABS':>9} {'HDG':>6} {'MAG_X':>8} {'MAG_Y':>8} {'MAG_Z':>8}"
    )
    print(
        f"{'(sec)':<12} {'(deg)':>8} {'(deg)':>8} {'(deg)':>8} {'(deg/s)':>9} {'(deg/s)':>9} {'(deg/s)':>9} {'(m)':>9} {'(m)':>9} {'(deg)':>6} {'(mGa)':>8} {'(mGa)':>8} {'(mGa)':>8}"
    )
    print("=" * 100)


def format_value(value, width=8, decimals=2):
    """Format a numeric value with proper width and decimals, handle None"""
    if value is None:
        return "N/A".rjust(width)
    return f"{value:.{decimals}f}".rjust(width)


class MAVLinkMonitor:
    def __init__(self, connection_string, baud=57600):
        self.connection_string = connection_string
        self.baud = baud
        self.master = None

        # Sensor data storage
        self.attitude = None
        self.vfr_hud = None
        self.global_position = None
        self.sys_status = None
        self.raw_imu = None
        self.scaled_imu = None

        # Timing
        self.start_time = time.time()
        self.last_heartbeat = 0

        # Message tracking for debug output
        self.commands_sent = []
        self.messages_received_log = []
        self.last_debug_output = 0

    def connect(self, quiet=False):
        """Establish MAVLink connection"""
        if not quiet:
            print(f"Connecting to {self.connection_string} at {self.baud} baud...")

        # Configure serial port if needed
        if self.connection_string.startswith("/dev/"):
            import os

            if not os.path.exists(self.connection_string):
                print(f"Error: Serial device {self.connection_string} not found")
                return False

            # Try to configure the serial port
            try:
                import subprocess

                subprocess.run(
                    [
                        "stty",
                        "-F",
                        self.connection_string,
                        str(self.baud),
                        "raw",
                        "-echo",
                    ],
                    check=False,
                    capture_output=True,
                )
                if not quiet:
                    if not quiet:
                        print(f"Serial port configured: {self.baud} baud, raw mode")
            except Exception as e:
                print(f"Warning: Could not configure serial port: {e}")

        try:
            self.master = mavutil.mavlink_connection(
                self.connection_string,
                baud=self.baud,
                source_system=255,
                source_component=0,
            )

            # Wait for heartbeat to confirm connection
            if not quiet:
                if not quiet:
                    print("Waiting for heartbeat (timeout: 10s)...")

            # Try to receive any data first to debug
            start_time = time.time()
            heartbeat_received = False
            bytes_received = 0
            message_types = set()

            while time.time() - start_time < 10:
                msg = self.master.recv_match(blocking=True, timeout=1.0)
                if msg:
                    bytes_received += 1
                    msg_type = msg.get_type()
                    message_types.add(msg_type)

                    if msg_type == "HEARTBEAT":
                        heartbeat_received = True
                        if not quiet:
                            print(
                                f"Heartbeat received from system {self.master.target_system}, component {self.master.target_component}"
                            )
                            print(
                                f"Vehicle type: {msg.type}, Autopilot: {msg.autopilot}, MAVLink version: {msg.mavlink_version}"
                            )
                            if len(message_types) > 1 and not quiet:
                                print(
                                    f"Also received: {', '.join(sorted(message_types - {'HEARTBEAT'}))}"
                                )
                        return True

            if bytes_received == 0:
                print(f"No data received from {self.connection_string}")
                print("This may indicate:")
                print("  - Device is not sending MAVLink data")
                print("  - Wrong baud rate (try 115200, 57600, or 9600)")
                print("  - Device needs to be reset or is not running ArduPilot")
            else:
                print(
                    f"Received {bytes_received} messages of types: {', '.join(sorted(message_types))} but no HEARTBEAT"
                )

            return False

        except Exception as e:
            print(f"Connection failed: {e}")
            import traceback

            traceback.print_exc()
            return False

    def request_data_streams(self, quiet=False):
        """Request data streams from the autopilot"""
        if not self.master:
            return

        if not quiet:
            print("Requesting data streams...")

        # Request all data streams at 4 Hz
        stream_rates = [
            (mavutil.mavlink.MAV_DATA_STREAM_ALL, 4),
            (mavutil.mavlink.MAV_DATA_STREAM_RAW_SENSORS, 4),
            (mavutil.mavlink.MAV_DATA_STREAM_EXTENDED_STATUS, 2),
            (mavutil.mavlink.MAV_DATA_STREAM_POSITION, 4),
            (mavutil.mavlink.MAV_DATA_STREAM_EXTRA1, 4),
            (mavutil.mavlink.MAV_DATA_STREAM_EXTRA2, 4),
        ]

        for stream_id, rate in stream_rates:
            self.master.mav.request_data_stream_send(
                self.master.target_system,
                self.master.target_component,
                stream_id,
                rate,
                1,  # start streaming
            )
            time.sleep(0.05)  # Small delay between requests

        # Also try setting message intervals for specific messages we need
        # This is the newer method (MAVLink 2)
        if not quiet:
            print("Setting message intervals...")

        # Message IDs for the data we want
        message_ids = [
            (mavutil.mavlink.MAVLINK_MSG_ID_ATTITUDE, 250000),  # 4 Hz (microseconds)
            (mavutil.mavlink.MAVLINK_MSG_ID_VFR_HUD, 250000),  # 4 Hz
            (mavutil.mavlink.MAVLINK_MSG_ID_GLOBAL_POSITION_INT, 250000),  # 4 Hz
            (mavutil.mavlink.MAVLINK_MSG_ID_SYS_STATUS, 1000000),  # 1 Hz
            (mavutil.mavlink.MAVLINK_MSG_ID_SCALED_IMU, 250000),  # 4 Hz
            (mavutil.mavlink.MAVLINK_MSG_ID_RAW_IMU, 250000),  # 4 Hz
        ]

        for msg_id, interval_us in message_ids:
            try:
                self.master.mav.command_long_send(
                    self.master.target_system,
                    self.master.target_component,
                    mavutil.mavlink.MAV_CMD_SET_MESSAGE_INTERVAL,
                    0,  # confirmation
                    msg_id,  # param1: message ID
                    interval_us,  # param2: interval in microseconds
                    0,
                    0,
                    0,
                    0,
                    0,  # param3-7: unused
                )
                self.commands_sent.append(
                    {
                        "time": time.time() - self.start_time,
                        "command": "SET_MESSAGE_INTERVAL",
                        "msg_id": msg_id,
                        "interval_us": interval_us,
                    }
                )
                time.sleep(0.05)
            except Exception as e:
                print(f"Warning: Could not set interval for message {msg_id}: {e}")

        # Count total interval requests
        total_interval_requests = len(message_ids)
        if not quiet:
            print(f"Set intervals for {total_interval_requests} message types")

    def request_specific_messages(self):
        """Actively request specific sensor messages"""
        if not self.master:
            return

        # Request ATTITUDE
        self.master.mav.command_long_send(
            self.master.target_system,
            self.master.target_component,
            mavutil.mavlink.MAV_CMD_REQUEST_MESSAGE,
            0,
            mavutil.mavlink.MAVLINK_MSG_ID_ATTITUDE,
            0,
            0,
            0,
            0,
            0,
            0,
        )
        self.commands_sent.append(
            {
                "time": time.time() - self.start_time,
                "command": "REQUEST_MESSAGE",
                "msg_id": mavutil.mavlink.MAVLINK_MSG_ID_ATTITUDE,
                "msg_name": "ATTITUDE",
            }
        )

        # Request VFR_HUD
        self.master.mav.command_long_send(
            self.master.target_system,
            self.master.target_component,
            mavutil.mavlink.MAV_CMD_REQUEST_MESSAGE,
            0,
            mavutil.mavlink.MAVLINK_MSG_ID_VFR_HUD,
            0,
            0,
            0,
            0,
            0,
            0,
        )
        self.commands_sent.append(
            {
                "time": time.time() - self.start_time,
                "command": "REQUEST_MESSAGE",
                "msg_id": mavutil.mavlink.MAVLINK_MSG_ID_VFR_HUD,
                "msg_name": "VFR_HUD",
            }
        )

        # Request GLOBAL_POSITION_INT
        self.master.mav.command_long_send(
            self.master.target_system,
            self.master.target_component,
            mavutil.mavlink.MAV_CMD_REQUEST_MESSAGE,
            0,
            mavutil.mavlink.MAVLINK_MSG_ID_GLOBAL_POSITION_INT,
            0,
            0,
            0,
            0,
            0,
            0,
        )
        self.commands_sent.append(
            {
                "time": time.time() - self.start_time,
                "command": "REQUEST_MESSAGE",
                "msg_id": mavutil.mavlink.MAVLINK_MSG_ID_GLOBAL_POSITION_INT,
                "msg_name": "GLOBAL_POSITION_INT",
            }
        )

        # Request RAW_IMU
        self.master.mav.command_long_send(
            self.master.target_system,
            self.master.target_component,
            mavutil.mavlink.MAV_CMD_REQUEST_MESSAGE,
            0,
            mavutil.mavlink.MAVLINK_MSG_ID_RAW_IMU,
            0,
            0,
            0,
            0,
            0,
            0,
        )
        # Don't log every request to reduce verbosity

        # Request SCALED_IMU
        self.master.mav.command_long_send(
            self.master.target_system,
            self.master.target_component,
            mavutil.mavlink.MAV_CMD_REQUEST_MESSAGE,
            0,
            mavutil.mavlink.MAVLINK_MSG_ID_SCALED_IMU,
            0,
            0,
            0,
            0,
            0,
            0,
        )
        # Don't log every request to reduce verbosity

    def process_messages(self, timeout=0.1):
        """Process incoming MAVLink messages"""
        msg = self.master.recv_match(blocking=True, timeout=timeout)

        if msg is None:
            return

        msg_type = msg.get_type()

        # Ignore bad data
        if msg_type == "BAD_DATA":
            return

        # Update stored messages based on type
        if msg_type == "ATTITUDE":
            self.attitude = msg
            self.messages_received_log.append(
                {
                    "time": time.time() - self.start_time,
                    "type": "ATTITUDE",
                    "data": {
                        "roll": msg.roll,
                        "pitch": msg.pitch,
                        "yaw": msg.yaw,
                        "rollspeed": msg.rollspeed,
                        "pitchspeed": msg.pitchspeed,
                        "yawspeed": msg.yawspeed,
                    },
                }
            )
        elif msg_type == "VFR_HUD":
            self.vfr_hud = msg
            self.messages_received_log.append(
                {
                    "time": time.time() - self.start_time,
                    "type": "VFR_HUD",
                    "data": {
                        "airspeed": msg.airspeed,
                        "groundspeed": msg.groundspeed,
                        "heading": msg.heading,
                        "throttle": msg.throttle,
                        "alt": msg.alt,
                        "climb": msg.climb,
                    },
                }
            )
        elif msg_type == "GLOBAL_POSITION_INT":
            self.global_position = msg
            self.messages_received_log.append(
                {
                    "time": time.time() - self.start_time,
                    "type": "GLOBAL_POSITION_INT",
                    "data": {
                        "lat": msg.lat / 1e7,
                        "lon": msg.lon / 1e7,
                        "alt": msg.alt / 1000.0,
                        "relative_alt": msg.relative_alt / 1000.0,
                        "vx": msg.vx / 100.0,
                        "vy": msg.vy / 100.0,
                        "vz": msg.vz / 100.0,
                        "hdg": msg.hdg / 100.0,
                    },
                }
            )
        elif msg_type == "SYS_STATUS":
            self.sys_status = msg
        elif msg_type == "HEARTBEAT":
            self.last_heartbeat = time.time()
        elif msg_type == "RAW_IMU":
            self.raw_imu = msg
            self.messages_received_log.append(
                {
                    "time": time.time() - self.start_time,
                    "type": "RAW_IMU",
                    "data": {
                        "xmag": msg.xmag,
                        "ymag": msg.ymag,
                        "zmag": msg.zmag,
                        "xacc": msg.xacc,
                        "yacc": msg.yacc,
                        "zacc": msg.zacc,
                        "xgyro": msg.xgyro,
                        "ygyro": msg.ygyro,
                        "zgyro": msg.zgyro,
                    },
                }
            )
        elif msg_type == "SCALED_IMU":
            self.scaled_imu = msg
            self.messages_received_log.append(
                {
                    "time": time.time() - self.start_time,
                    "type": "SCALED_IMU",
                    "data": {
                        "xmag": msg.xmag,
                        "ymag": msg.ymag,
                        "zmag": msg.zmag,
                        "xacc": msg.xacc,
                        "yacc": msg.yacc,
                        "zacc": msg.zacc,
                        "xgyro": msg.xgyro,
                        "ygyro": msg.ygyro,
                        "zgyro": msg.zgyro,
                    },
                }
            )

    def get_sensor_data(self):
        """Extract and return current sensor readings"""
        import math

        data = {
            "time": time.time() - self.start_time,
            "roll": None,
            "pitch": None,
            "yaw": None,
            "rollspeed": None,
            "pitchspeed": None,
            "yawspeed": None,
            "alt_relative": None,
            "alt_absolute": None,
            "heading": None,
            "mag_x": None,
            "mag_y": None,
            "mag_z": None,
        }

        # Attitude data (roll, pitch, yaw in radians, rates in rad/s)
        if self.attitude:
            data["roll"] = math.degrees(self.attitude.roll)
            data["pitch"] = math.degrees(self.attitude.pitch)
            data["yaw"] = math.degrees(self.attitude.yaw)
            data["rollspeed"] = math.degrees(self.attitude.rollspeed)
            data["pitchspeed"] = math.degrees(self.attitude.pitchspeed)
            data["yawspeed"] = math.degrees(self.attitude.yawspeed)

        # Altitude and heading from VFR_HUD
        if self.vfr_hud:
            data["alt_absolute"] = self.vfr_hud.alt
            data["heading"] = self.vfr_hud.heading

        # Relative altitude from GLOBAL_POSITION_INT
        if self.global_position:
            data["alt_relative"] = (
                self.global_position.relative_alt / 1000.0
            )  # Convert mm to m

        # Magnetometer data (prefer SCALED_IMU, fallback to RAW_IMU)
        if self.scaled_imu:
            data["mag_x"] = self.scaled_imu.xmag
            data["mag_y"] = self.scaled_imu.ymag
            data["mag_z"] = self.scaled_imu.zmag
        elif self.raw_imu:
            # RAW_IMU is in milligauss, same as SCALED_IMU
            data["mag_x"] = self.raw_imu.xmag
            data["mag_y"] = self.raw_imu.ymag
            data["mag_z"] = self.raw_imu.zmag

        return data

    def print_sensor_data(self, data):
        """Print sensor data in formatted columns"""
        print(
            f"{data['time']:<12.2f} "
            f"{format_value(data['roll'], 8, 2)} "
            f"{format_value(data['pitch'], 8, 2)} "
            f"{format_value(data['yaw'], 8, 2)} "
            f"{format_value(data['rollspeed'], 9, 2)} "
            f"{format_value(data['pitchspeed'], 9, 2)} "
            f"{format_value(data['yawspeed'], 9, 2)} "
            f"{format_value(data['alt_relative'], 9, 2)} "
            f"{format_value(data['alt_absolute'], 9, 2)} "
            f"{format_value(data['heading'], 6, 0)} "
            f"{format_value(data['mag_x'], 8, 1)} "
            f"{format_value(data['mag_y'], 8, 1)} "
            f"{format_value(data['mag_z'], 8, 1)}"
        )

    def print_debug_output(self, quiet=False):
        """Print concise MAVLink command and response information"""
        if quiet:
            return

        current_time = time.time() - self.start_time

        print("\n" + "=" * 80)
        print(f"DEBUG OUTPUT @ {current_time:.2f}s")
        print("=" * 80)

        # Count message requests sent in the last 5 seconds (excluding tracked ones)
        print(
            f"Active message requests in last 5s: {len([m for m in self.messages_received_log if current_time - m['time'] <= 5.0])}"
        )

        # Show message type counts only
        recent_messages = [
            msg
            for msg in self.messages_received_log
            if current_time - msg["time"] <= 5.0
        ]
        if recent_messages:
            msg_counts = {}
            for msg in recent_messages:
                msg_counts[msg["type"]] = msg_counts.get(msg["type"], 0) + 1
            print(f"Message types received: {msg_counts}")

            # Show magnetometer status specifically
            if "RAW_IMU" in msg_counts or "SCALED_IMU" in msg_counts:
                latest_mag = None
                # Find most recent magnetometer data
                for msg in reversed(recent_messages):
                    if msg["type"] in ["RAW_IMU", "SCALED_IMU"]:
                        latest_mag = msg
                        break

                if latest_mag:
                    print(
                        f"Latest magnetometer: X={latest_mag['data']['xmag']}, Y={latest_mag['data']['ymag']}, Z={latest_mag['data']['zmag']} (mGa)"
                    )
        else:
            print("No messages received in last 5 seconds")

        print("=" * 80 + "\n")

    def monitor(self, duration=None, update_rate=1.0, quiet=False):
        """Main monitoring loop"""
        if not self.connect(quiet):
            if not quiet:
                print("\n" + "=" * 60)
                print("CONNECTION FAILED - Troubleshooting suggestions:")
                print("=" * 60)
                print("1. Ensure device is properly connected and /dev/ttyACM0 exists")
                print("2. Try resetting the device:")
                print(
                    "   sudo ~/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=SWD reset=HWrst -hardRst"
                )
                print("3. Check if device is sending MAVLink:")
                print("   timeout 3 cat /dev/ttyACM0 | od -A x -t x1z -v | head -20")
                print("4. Try different baud rates: 115200, 57600, 9600")
                print("5. Verify ArduPilot firmware is properly flashed")
                print("=" * 60)
            return 1

        # Request data streams
        self.request_data_streams(quiet)

        # Give some time for data to start flowing
        if not quiet:
            print("Waiting for data streams to start...")
            time.sleep(1.0)

            # Allow time for data to stabilize
            print("Waiting for sensor data...")
            time.sleep(2.0)
        else:
            time.sleep(3.0)  # Still wait when quiet, just don't print

        if not quiet:
            print_header()

        last_print = 0
        last_request = 0
        last_heartbeat_check = 0
        messages_received = 0

        try:
            while True:
                # Process messages
                self.process_messages()
                messages_received += 1

                # Periodically request specific messages (every 2 seconds)
                current_time = time.time()
                if current_time - last_request >= 2.0:
                    self.request_specific_messages()
                    last_request = current_time

                # Print debug output every 5 seconds (unless in quiet mode)
                if not quiet and current_time - self.last_debug_output >= 5.0:
                    self.print_debug_output(quiet)
                    self.last_debug_output = current_time

                # Print at specified rate
                if current_time - last_print >= update_rate:
                    data = self.get_sensor_data()
                    self.print_sensor_data(data)
                    last_print = current_time

                # Check duration limit
                if duration and (current_time - self.start_time) >= duration:
                    break

                # Check for connection loss
                if self.last_heartbeat > 0 and (current_time - self.last_heartbeat) > 5:
                    print("\nWarning: No heartbeat for 5 seconds!")
                    self.last_heartbeat = current_time  # Reset to avoid spam

        except KeyboardInterrupt:
            if not quiet:
                print("\n\nMonitoring stopped by user")
        except Exception as e:
            print(f"\n\nError during monitoring: {e}")
            import traceback

            traceback.print_exc()
            return 1

        if not quiet:
            print(f"\nMonitoring complete. Received {messages_received} messages.")
        return 0


def main():
    parser = argparse.ArgumentParser(
        description="MAVLink Sensor Monitor for HRON-Chickadee",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  Monitor on /dev/ttyACM0 at 57600 baud:
    %(prog)s /dev/ttyACM0
  
  Monitor on /dev/ttyACM0 at 115200 baud for 60 seconds:
    %(prog)s /dev/ttyACM0 -b 115200 -d 60
  
  Monitor in quiet mode (no debug output):
    %(prog)s /dev/ttyACM0 -q
  
  Monitor via UDP:
    %(prog)s udp:localhost:14550
        """,
    )

    parser.add_argument(
        "connection",
        nargs="?",
        default="/dev/ttyACM0",
        help="MAVLink connection string (default: /dev/ttyACM0)",
    )
    parser.add_argument(
        "-b",
        "--baud",
        type=int,
        default=57600,
        help="Baud rate for serial connection (default: 57600)",
    )
    parser.add_argument(
        "-d",
        "--duration",
        type=float,
        default=None,
        help="Monitoring duration in seconds (default: run until Ctrl+C)",
    )
    parser.add_argument(
        "-r",
        "--rate",
        type=float,
        default=1.0,
        help="Update rate in seconds (default: 1.0)",
    )
    parser.add_argument(
        "-q",
        "--quiet",
        action="store_true",
        help="Suppress debug output (show sensor data only)",
    )

    args = parser.parse_args()

    monitor = MAVLinkMonitor(args.connection, args.baud)
    sys.exit(monitor.monitor(args.duration, args.rate, args.quiet))


if __name__ == "__main__":
    main()
