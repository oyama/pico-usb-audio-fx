#include "led.h"
#include "bsp/board_api.h"

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

void led_set_blink_interval(uint32_t interval_ms) {
    blink_interval_ms = interval_ms;
}

void led_update(void) {
    static uint32_t start_ms = 0;
    static bool led_state = false;

    // Blink every interval ms
    if (board_millis() - start_ms < blink_interval_ms)
        return;
    start_ms += blink_interval_ms;

    board_led_write(led_state);
    led_state = 1 - led_state;
}
