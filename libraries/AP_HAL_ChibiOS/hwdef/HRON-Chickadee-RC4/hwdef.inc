# USB setup - Heron Precision vendor ID for main firmware
USB_VENDOR 0x35B0  # Heron Precision
USB_PRODUCT 0x0002  # Chickadee RC4
USB_STRING_MANUFACTURER "Heron Precision"
USB_STRING_PRODUCT "HRON-Chickadee-RC4"

# Order of UARTs (and USB) for main firmware
# Note: UART5 is used for OSD STM32G, UART4 now used for CAN1
SERIAL_ORDER OTG1 USART1 USART2 USART3 USART6 UART7 UART8
