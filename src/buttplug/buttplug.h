#ifndef GD_BUTTPLUG_H
#define GD_BUTTPLUG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum bp_result {
    BP_OK = 0,
    BP_NOT_CONNECTED = 1,
    BP_NO_DEVICES = 2,
    BP_INVALID_ARGUMENT = 3,
    BP_ERROR = 4
} bp_result;

/*
 * Starts the background Buttplug client.
 *
 * The client attempts to connect to:
 *
 *     ws://127.0.0.1:12345
 *
 * This function does not block waiting for a connection.
 */
bp_result bp_init(void);

/*
 * Stops the background client and closes the connection.
 */
void bp_shutdown(void);

/*
 * Returns non-zero when the WebSocket connection to Intiface
 * is currently established.
 */
int32_t bp_is_connected(void);

/*
 * Returns the number of currently known vibrator features.
 */
uint32_t bp_vibrator_count(void);

/*
 * Vibrates every discovered vibrator feature.
 *
 * intensity is expressed as 0..100.
 * duration_ms is the duration in milliseconds.
 */
bp_result bp_vibrate(uint32_t duration_ms, uint8_t intensity);

/*
 * Immediately stops every known vibrator.
 */
bp_result bp_stop(void);

bp_result bp_set_intensity(uint8_t intensity);

bp_result bp_set_vibration(uint8_t intensity);

#ifdef __cplusplus
}
#endif

#endif
