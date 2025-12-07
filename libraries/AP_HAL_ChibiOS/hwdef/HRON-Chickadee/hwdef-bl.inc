# crystal frequency
OSCILLATOR_HZ 25000000

# USB setup - follow working mRo bootloader pattern
USB_STRING_MANUFACTURER "mRo"

# board ID for HRON-Chickadee
APJ_BOARD_ID AP_HW_HRON_Chickadee

# Enable bootloader timeout fix for HRON-Chickadee (board-specific fix)
# This allows timeout logic to work even with SERIAL_ORDER defined
define BOOTLOADER_TIMEOUT_FIX

FLASH_SIZE_KB 2048

# bootloader takes first 128KB - matching main firmware config
FLASH_RESERVE_START_KB 0

# the location where the bootloader will put the firmware
# the H743 has 128k sectors
FLASH_BOOTLOADER_LOAD_KB 128

define HAL_LED_ON 0

# order of UARTs (and USB)
SERIAL_ORDER OTG1

# LED configuration - avoid SWD pin conflicts (PA13/PA14 are SWD)
PB0 LED_BOOTLOADER OUTPUT HIGH

# USB pins
PA11 OTG_FS_DM OTG1
PA12 OTG_FS_DP OTG1

# SWD pins for debugging
PA13 JTMS-SWDIO SWD
PA14 JTCK-SWCLK SWD

# enable DFU by default
ENABLE_DFU_BOOT 1
