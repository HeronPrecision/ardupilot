#pragma once

#include <stdint.h>

#ifdef __has_include
# if __has_include("hwdef.h")
#  include "hwdef.h"
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief A single HRON boot marker entry.
 */
typedef struct {
    uint32_t timestamp_cycles;
    uint32_t payload;
} HRONBootMarker;

/**
 * @brief Boot marker ring buffer shared between firmware and debug tooling.
 *
 * The buffer holds up to 256 entries. The @p head index is incremented for every
 * write and wraps naturally through modulo arithmetic applied by readers.
 */
typedef struct {
    volatile uint32_t head;
    HRONBootMarker entries[256];
} HRONBootMarkerLog;

/* Boot marker channel identifiers */
#define HRON_BOOT_CH_STAGE_RESET      0x01U
#define HRON_BOOT_CH_STAGE_CLOCKS     0x02U
#define HRON_BOOT_CH_STAGE_USB        0x03U
#define HRON_BOOT_CH_STAGE_HANDOFF    0x04U

/* Pack a channel identifier with a 24-bit value */
#define HRON_BOOT_ENCODE_CHANNEL_VALUE(channel, value) \
    ((((uint32_t)(channel) & 0xFFU) << 24) | ((uint32_t)(value) & 0x00FFFFFFU))

#if defined(HRON_CHICKADEE_DEBUG)

/** @brief Global marker buffer instance provided by board-specific implementation. */
extern HRONBootMarkerLog g_hron_boot_marker_log;

/** @brief Reset the marker log to an empty state. */
void hron_boot_marker_log_reset(void);

/** @brief Append a channel/value marker to the log. */
void hron_boot_marker_log_append(uint8_t channel, uint32_t value);

/** @brief Append a raw payload marker to the log. */
void hron_boot_marker_log_append_raw(uint32_t payload);

#endif /* HRON_CHICKADEE_DEBUG */

#ifdef __cplusplus
}
#endif

#if defined(HRON_CHICKADEE_DEBUG)

#define HRON_BOOT_DEBUG_MARK(channel, value) \
    do { hron_boot_marker_log_append((uint8_t)(channel), (uint32_t)(value)); } while (0)

#define HRON_BOOT_DEBUG_MARK_RAW(payload) \
    do { hron_boot_marker_log_append_raw((uint32_t)(payload)); } while (0)

#define HRON_BOOT_DEBUG_MARK_ENCODED(channel, value) \
    HRON_BOOT_DEBUG_MARK_RAW(HRON_BOOT_ENCODE_CHANNEL_VALUE((channel), (value)))

#else  /* HRON_CHICKADEE_DEBUG not defined */

#define HRON_BOOT_DEBUG_MARK(channel, value) \
    do { (void)(channel); (void)(value); } while (0)

#define HRON_BOOT_DEBUG_MARK_RAW(payload) \
    do { (void)(payload); } while (0)

#define HRON_BOOT_DEBUG_MARK_ENCODED(channel, value) \
    do { (void)(channel); (void)(value); } while (0)

#endif /* HRON_CHICKADEE_DEBUG */