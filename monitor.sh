sudo /home/user/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI -c port=SWD reset=HWrst -hardRst && sleep 5 && timeout 25 cat /dev/ttyACM0 | strings
