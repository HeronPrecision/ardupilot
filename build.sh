#!/usr/bin/env bash

# Default board
BOARD="${BOARD:-HRON-Chickadee-RC3}"

# Parse command line arguments for board selection
case "$1" in
    rc3|RC3)
        BOARD="HRON-Chickadee-RC3"
        shift
        ;;
    rc4|RC4)
        BOARD="HRON-Chickadee-RC4"
        shift
        ;;
    rc5|RC5)
        BOARD="HRON-Chickadee-RC5"
        shift
        ;;
esac

# Handle commands
case "$1" in
    clean)
        rm -rf build/"$BOARD"
        ;;
    bootloader)
        docker run --rm -v "$(pwd):/ardupilot" ardupilot-build /bin/bash -c "cd /ardupilot && ./waf configure --board=$BOARD && ./waf bootloader"
        ;;
    configure)
        docker run --rm -v "$(pwd):/ardupilot" ardupilot-build /bin/bash -c "cd /ardupilot && ./waf configure --board=$BOARD"
        ;;
    ""|copter|plane|rover|sub|blinky)
        docker run --rm -v "$(pwd):/ardupilot" ardupilot-build /bin/bash -c "cd /ardupilot && ./waf --board=$BOARD ${1:-copter} --debug"
        ;;
    *)
        echo "Usage: $0 [rc3|rc4|rc5] [clean|bootloader|configure|copter|plane|rover|sub|blinky]"
        echo "Default board: RC3"
        echo "Examples:"
        echo "  $0 rc4 configure    # Configure RC4 build"
        echo "  $0 rc5 copter      # Build RC5 copter"
        echo "  $0 rc3 clean       # Clean RC3 build"
        exit 1
        ;;
esac
