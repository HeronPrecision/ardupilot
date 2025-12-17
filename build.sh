#!/usr/bin/env bash
if [ "$1" == "clean" ]; then
    docker run --rm -v $(pwd):/ardupilot ardupilot-build /bin/bash -c "cd /ardupilot && ./waf clean"
elif [ "$1" == "bootloader" ]; then
    docker run --rm -v $(pwd):/ardupilot ardupilot-build /bin/bash -c "cd /ardupilot && ./waf bootloader"
else
    docker run --rm -v $(pwd):/ardupilot ardupilot-build /bin/bash -c "cd /ardupilot && ./waf --board=HRON-Chickadee copter --debug"
fi
