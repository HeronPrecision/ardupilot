# ArduPilot HRON-Chickadee Project Status

## Current Focus – Intermittent USB Enumeration Failures
- Issue: After reset the HRON-Chickadee target (STM32H743) often fails to expose the USB CDC serial interface.
- Scope: Main firmware only. Bootloader timeouts already addressed previously and appear stable.
- Impact: No USB activity prevents normal console monitoring, crippling field diagnostics without SWD access.

## Recent Engineering Actions
1. **Boot Marker Instrumentation**
   - A `.noinit` ring buffer (`g_hron_boot_marker_log`) now records 256 timestamped entries keyed by phase.
   - Macros: `HRON_BOOT_DEBUG_MARK_ENCODED()` etc. expand to no-ops on other boards; enabled via `HRON_CHICKADEE_DEBUG`.
   - Current emit points:
     - `__early_init()` – reset entry and post-clock markers.
     - `__late_init()` – markers after USB descriptors prepared.
     - `usb_initialise()` – entry, re-entry, and connect completion.
     - `boardInit()` – final handoff before application code.
     - **New** markers in `usbcfg.c` for USB core lifecycle (address/configured/reset/suspend/wakeup/stall) and CDC class requests.
   - Outcome: We can see exactly how far boot proceeds even without USB enumeration using SWD (OpenOCD/GDB).

2. **VBUS Handling Review**
   - `PA9` configured as `VBUS INPUT OPENDRAIN` within the hwdef. Other H743 boards use similar definitions; no discrepancies yet flagged.
   - No code change made to leverage VBUS in AnalogIn or power flags beyond existing logic.

3. **Comparative Target Survey (In progress)**
   - Baseline reference: MatekH743 and other dual-supported (ArduPilot + Betaflight) H7 boards.
   - Initial inspection shows common USB bring-up sequences and 1.5 ms pull-down delay; no immediate divergence.

4. **Firmware Build + Flash Attempt (Dec 15)**
   - `./build.sh` completed successfully, producing `build/HRON-Chickadee/bin/arducopter_with_bl.hex`.
   - Two consecutive flash attempts failed with `ST-LINK error (DEV_CONNECT_ERR)`; lingering programmer processes on the host are the suspected cause.

## Data Capture Instructions
1. Build with `build.sh` (Docker environment) and flash via `flash.sh`. Retry flash at most twice; stop and report persistent failures.
2. After reset:
   - If USB enumerates, use `monitor.sh` (be aware it can leave STM-based processes; kill manually when done).
   - If USB is silent, attach SWD and in GDB:
     ```
     (gdb) p/x g_hron_boot_marker_log.head
     (gdb) x/32x &g_hron_boot_marker_log.entries
     ```
   - Decode entries: top byte = channel, low 24 bits = marker code. Timestamps are raw DWT cycle counts.
3. Share failing boot dumps plus any lsusb/dmesg excerpts to correlate host-side activity.

## Observations & Hypotheses
- When USB succeeds, markers are expected in this order:
  - Reset (`0x01000000`), clocks (`0x02000000`), USB init start (`0x03000100`), connect complete (`0x030001FF`), USB events such as address/configured (`0x03000210`), then handoff (`0x04000000`).
- If failures occur before `0x030001FF`, suspect hardware detect, clock init, or USB driver start.
- Latest SWD dump returned `head = 0` with all 256 entries zeroed, showing the buffer was cleared but no markers were appended before halting; rerun the capture with a longer resume window to confirm whether markers ever emit.
- If `usb_event` markers appear but host never enumerates, focus on descriptors/endpoints or VBUS sensing.
- If markers stop after handoff, application code may disable USB inadvertently—requires deeper inspection.
- Latest rebuild confirms `g_hron_boot_marker_log` is present in `build/HRON-Chickadee/bin/arducopter` (verified with `arm-none-eabi-nm`), so boot marker instrumentation is active.

## Next Steps
1. **Collect Failing Boot Logs**
   - Obtain multiple SWD dumps highlighting the exact final marker and DWT timing.
   - Record whether host sees any transient USB activity (lsusb, `dmesg`).

2. **Capture Boot Marker Dumps**
   - Attach SWD and dump `g_hron_boot_marker_log.head` plus the ring entries now that the symbol is confirmed in the ELF.
   - Archive `/tmp/hron_markers.txt` together with the tail of the OpenOCD log for decoding.

3. **Extend Instrumentation (Targeted)**
   - Candidate sites once data arrives:
     - `sduConfigureHookI`, `sduSuspendHookI`, CDC driver state transitions.
     - Scheduler start and task priorities affecting USB threads.

4. **Cross-Board Comparison**
   - Diff `usbcfg.c`, hwdef USB pin/power settings, and early init between HRON-Chickadee and a known good H743 (e.g., MatekH743).
   - Examine Betaflight definition (`HERONPRECISION_CHICKADEE/config.h`) for hints on VBUS handling or pull-ups not mirrored.

5. **Hardware Verification**
   - Confirm VBUS pin wiring and presence of pull-up resistor consistent with hwdef assumptions.
   - Validate that the board isn’t resetting due to power sag; consider logging board voltage if possible.

6. **Process Safeguards**
   - Minimize flashes; hardware is near write-cycle limits.
   - After using `monitor.sh` or flashing, inspect for lingering OpenOCD or `STM32_Programmer_CLI` processes (e.g., `ps -eo pid,comm,args | grep -i 'stm32'`) and terminate them before retrying.
   - Document all test outcomes here to keep status synchronized.

## Open Questions
- Do marker logs show we ever reach `USB_EVENT_CONFIGURED` during failed boots?
- Is VBUS ever read low when USB cable is connected, suggesting hardware detect issue?
- Are there race conditions between `usbConnectBus()` and host interactions that require additional delays or reinitialization logic?

**Please append new findings, marker dumps, and test results here as work progresses.**