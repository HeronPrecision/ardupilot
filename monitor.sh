#!/bin/bash
# HRON-Chickadee MAVLink Monitor Script
# Queries attitude (gyros), altitude (pressure), and heading (compass) via MAVLink

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default values
SERIAL_PORT="/dev/ttyACM0"
BAUD_RATE=57600
DURATION="30"  # Default to 30 seconds to ensure exit
UPDATE_RATE=1.0
RESET_DEVICE=true
QUIET=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            SERIAL_PORT="$2"
            shift 2
            ;;
        -b|--baud)
            BAUD_RATE="$2"
            shift 2
            ;;
        -d|--duration)
            DURATION="$2"
            shift 2
            ;;
        -r|--rate)
            UPDATE_RATE="$2"
            shift 2
            ;;
        -q|--quiet)
            QUIET=true
            shift
            ;;
        --no-reset)
            RESET_DEVICE=false
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -p, --port PORT      Serial port (default: /dev/ttyACM0)"
            echo "  -b, --baud RATE      Baud rate (default: 57600)"
            echo "  -d, --duration SEC   Run for SEC seconds (default: run until Ctrl+C)"
            echo "  -r, --rate SEC       Update rate in seconds (default: 1.0)"
            echo "  -q, --quiet          Suppress debug output (show sensor data only)"
            echo "  --no-reset           Don't reset device before monitoring"
            echo "  -h, --help           Show this help message"
            echo ""
            echo "Example:"
            echo "  $0 -b 115200 -d 30 -r 0.5"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            echo "Use -h or --help for usage information"
            exit 1
            ;;
    esac
done

# Function to check if serial port exists
check_serial_port() {
    local max_wait=10
    local count=0
    
    while [ $count -lt $max_wait ]; do
        if [ -e "$SERIAL_PORT" ]; then
            echo -e "${GREEN}Serial port $SERIAL_PORT found${NC}"
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done
    
    echo -e "${RED}Error: Serial port $SERIAL_PORT not found after ${max_wait}s${NC}"
    return 1
}

# Reset the device via SWD if requested
if [ "$RESET_DEVICE" = true ]; then
    echo -e "${BLUE}Resetting HRON-Chickadee via SWD...${NC}"
    sudo /home/user/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI \
        -c port=SWD reset=HWrst -hardRst 2>&1 | grep -E "(STM32|Device|Reset|Error)" || true
    
    echo -e "${YELLOW}Waiting for device to initialize MAVLink...${NC}"
    sleep 3
fi

# Check if device is available
if ! check_serial_port; then
    echo -e "${RED}Available serial devices:${NC}"
    ls -la /dev/tty{ACM,USB}* 2>/dev/null || echo "  None found"
    exit 1
fi

# Set proper permissions for the serial port
echo -e "${BLUE}Setting permissions for $SERIAL_PORT...${NC}"
sudo chmod 666 "$SERIAL_PORT"

# Build the command
CMD="uv run python3 mavlink_monitor.py $SERIAL_PORT -b $BAUD_RATE -r $UPDATE_RATE"
if [ -n "$DURATION" ]; then
    CMD="$CMD -d $DURATION"
fi

# Run the MAVLink monitor
if [ "$QUIET" = false ]; then
    echo ""
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}HRON-Chickadee MAVLink Sensor Monitor${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo -e "Port:        $SERIAL_PORT"
    echo -e "Baud Rate:   $BAUD_RATE"
    echo -e "Update Rate: ${UPDATE_RATE}s"
    if [ -n "$DURATION" ]; then
        echo -e "Duration:    ${DURATION}s"
    else
        echo -e "Duration:    Continuous (Ctrl+C to stop)"
    fi
    echo ""
    echo -e "${BLUE}Monitoring: Attitude (gyros), Altitude (pressure), Heading (compass)${NC}"
    if [ "$QUIET" = false ]; then
        echo -e "${YELLOW}Press Ctrl+C to stop${NC}"
    else
        echo -e "Quiet mode enabled - sensor data only"
    fi
    echo ""
fi

# Execute the monitor with timeout as backup
if [ "$QUIET" = true ]; then
    timeout $((DURATION + 10))s $CMD -q || {
        # If timeout occurred or failed, exit with appropriate code
        exit_code=$?
        if [ $exit_code -eq 124 ]; then
            # Quiet mode - no timeout message
            exit 0
        else
            exit $exit_code
        fi
    }
else
    timeout $((DURATION + 10))s $CMD || {
        # If timeout occurred or failed, exit with appropriate code
        exit_code=$?
        if [ $exit_code -eq 124 ]; then
            echo -e "\n${YELLOW}Monitor timed out after ${DURATION} seconds${NC}"
            exit 0
        else
            exit $exit_code
        fi
    }
fi

# Exit with the same code as the monitor
exit $?
