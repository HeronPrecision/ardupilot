# HRON-Chickadee Bootloader Crash Analysis

## Critical Finding: Bootloader Execution Outside Code Region

### Data Evidence
- **PC (Program Counter)**: 0x08132e50
- **Bootloader start**: 0x08000000  
- **Bootloader size**: 18,548 bytes (0x4884)
- **PC offset**: 0x132e50 (1,269,712 bytes)

### Problem Analysis
The bootloader is executing at address 0x08132e50, which is **1.27MB** beyond the bootloader code region. Since the bootloader binary is only 18KB, the processor has jumped to invalid memory, causing:

1. **No USB enumeration** - Bootloader code not executing properly
2. **SWD communication issues** - Processor in undefined state
3. **Inconsistent behavior** - Random memory execution

### Root Cause
This indicates a bootloader code issue such as:
- Function pointer corruption
- Stack overflow causing return address hijacking  
- Memory corruption causing jump to invalid address
- Incorrect linker script or memory layout

### Memory Map Analysis
- 0x08000000-0x08004884: Valid bootloader code (18KB)
- 0x08132e50: Current PC - **INVALID EXECUTION REGION**

### Next Steps
1. Reset processor to valid bootloader start
2. Flash fresh bootloader to recover
3. Investigate bootloader code for corruption issues
4. Verify bootloader memory layout and stack configuration