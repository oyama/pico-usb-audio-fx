#pragma once

#include <stdint.h>

#define AUDIO_SAMPLE_RATE 48000
#define AUDIO_NUM_CHANNELS 2
#define AUDIO_BITS_PER_SAMPLE 24
#define AUDIO_BYTES_PER_SAMPLE 4                        // 32bit aligned (24bit data + padding)
#define AUDIO_FRAME_SAMPLES (AUDIO_SAMPLE_RATE / 1000)  // 48 samples per frame
#define AUDIO_FRAME_BYTES (AUDIO_FRAME_SAMPLES * AUDIO_NUM_CHANNELS * AUDIO_BYTES_PER_SAMPLE)

#define RINGBUF_FRAMES 16
#define TOTAL_SAMPLES (RINGBUF_FRAMES * AUDIO_FRAME_SAMPLES)

typedef struct {
    uint8_t buffer[RINGBUF_FRAMES][AUDIO_FRAME_BYTES];
    volatile uint8_t read_idx;
    volatile uint8_t write_idx;
} ringbuf_t;


