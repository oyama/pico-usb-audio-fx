/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bootsel_button.h"
#include "bsp/board_api.h"
#include "fx.h"
#include "hardware/clocks.h"
#include "led.h"
#include "pico/stdlib.h"
#include "ringbuffer.h"
#include "tusb.h"
#include "usb_descriptors.h"

static ringbuf_t rx_ringbuf = {0};
static ringbuf_t tx_ringbuf = {0};

static uint8_t silence_buf[AUDIO_FRAME_BYTES] = {0};

void audio_task(void) {
    fx_set_enable(bb_get_bootsel_button());

    if (rx_ringbuf.read_idx != rx_ringbuf.write_idx &&
        (tx_ringbuf.write_idx + 1) % RINGBUF_FRAMES != tx_ringbuf.read_idx) {
        uint8_t *input = rx_ringbuf.buffer[rx_ringbuf.read_idx];
        uint8_t *output = tx_ringbuf.buffer[tx_ringbuf.write_idx];

        fx_process(output, input);

        tx_ringbuf.write_idx = (tx_ringbuf.write_idx + 1) % RINGBUF_FRAMES;
        rx_ringbuf.read_idx = (rx_ringbuf.read_idx + 1) % RINGBUF_FRAMES;
    }
}

void led_task(void) { led_update(); }

bool tud_audio_rx_done_pre_read_cb(uint8_t rhport, uint16_t n_bytes_received, uint8_t func_id,
                                   uint8_t ep_out, uint8_t cur_alt_setting) {
    uint8_t next_write = (rx_ringbuf.write_idx + 1) % RINGBUF_FRAMES;
    if (next_write == rx_ringbuf.read_idx) {
        return true;
    }

    uint16_t rx_size = tud_audio_read(rx_ringbuf.buffer[rx_ringbuf.write_idx], n_bytes_received);
    if (rx_size != n_bytes_received)
        return true;
    rx_ringbuf.write_idx = next_write;
    return true;
}

bool tud_audio_tx_done_pre_load_cb(uint8_t rhport, uint8_t itf, uint8_t ep_in,
                                   uint8_t cur_alt_setting) {
    if (tx_ringbuf.read_idx == tx_ringbuf.write_idx) {
        tud_audio_write(silence_buf, AUDIO_FRAME_BYTES);
    } else {
        uint8_t *output = tx_ringbuf.buffer[tx_ringbuf.read_idx];

        tud_audio_write(output, AUDIO_FRAME_BYTES);
        tx_ringbuf.read_idx = (tx_ringbuf.read_idx + 1) % RINGBUF_FRAMES;
    }
    return true;
}

int main(void) {
    set_sys_clock_khz(240000, true);
    stdio_init_all();

    board_init();
    tusb_rhport_init_t dev_init = {.role = TUSB_ROLE_DEVICE, .speed = TUSB_SPEED_AUTO};
    tusb_init(BOARD_TUD_RHPORT, &dev_init);
    board_init_after_tusb();

    fx_init();

    while (1) {
        tud_task();
        audio_task();
        led_task();
    }
}
