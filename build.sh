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
        # Bootloader needs its own configure (--bootloader) + clean, like Tools/scripts/build_bootloaders.py.
        # Installs the result into Tools/bootloaders (embedded into *_with_bl.hex by vehicle builds), then
        # re-runs the normal app configure so a following vehicle build is not left on the bootloader config.
        docker run --rm -v "$(pwd):/ardupilot" ardupilot-build /bin/bash -c "cd /ardupilot && ./waf configure --board=$BOARD --bootloader --no-submodule-update && ./waf clean && ./waf bootloader && cp build/$BOARD/bin/AP_Bootloader.bin Tools/bootloaders/${BOARD}_bl.bin && python3 Tools/scripts/bin2hex.py --offset 0x08000000 Tools/bootloaders/${BOARD}_bl.bin Tools/bootloaders/${BOARD}_bl.hex && ./waf configure --board=$BOARD"
        ;;
    configure)
        docker run --rm -v "$(pwd):/ardupilot" ardupilot-build /bin/bash -c "cd /ardupilot && ./waf configure --board=$BOARD"
        ;;
    ""|copter|plane|rover|sub|blinky|antennatracker|tracker)
        VEHICLE="${1:-copter}"
        [ "$VEHICLE" = "tracker" ] && VEHICLE=antennatracker
        docker run --rm -v "$(pwd):/ardupilot" ardupilot-build /bin/bash -c "cd /ardupilot && ./waf --board=$BOARD $VEHICLE --debug"
        ;;
    *)
        echo "Usage: $0 [rc3|rc4|rc5] [clean|bootloader|configure|copter|plane|rover|sub|blinky|antennatracker]"
        echo "Default board: RC3"
        echo "Examples:"
        echo "  $0 rc4 configure       # Configure RC4 build"
        echo "  $0 rc5 copter          # Build RC5 copter"
        echo "  $0 rc3 antennatracker  # Build RC3 antenna tracker"
        echo "  $0 rc3 clean           # Clean RC3 build"
        exit 1
        ;;
esac
