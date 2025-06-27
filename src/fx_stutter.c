#include <string.h>
#include <stdlib.h>
#include "fx.h"
#include "ringbuffer.h"

#define STUTTER_FRAMES  63
#define STUTTER_SAMPLES   (AUDIO_FRAME_SAMPLES * STUTTER_FRAMES)

static int32_t sample_buffer[STUTTER_SAMPLES][AUDIO_NUM_CHANNELS];
static bool     recording    = false;
static bool     stuttering   = false;
static bool     prev_enabled = false;
static uint32_t rec_pos      = 0;
static uint32_t read_pos     = 0;

const char* fx_name(void) {
    return "Pico Audio FX Stutter";
}

void fx_init(void) {
}

void fx_set_enable(bool enable) {
    if (enable && !prev_enabled) {
        recording  = true;
        stuttering = false;
        rec_pos    = 0;
        read_pos   = 0;
    }
    if (!enable) {
        recording  = false;
        stuttering = false;
    }
    prev_enabled = enable;
}

void fx_process(uint8_t *output, uint8_t *input) {
    if (recording) {
        for (int i = 0; i < AUDIO_FRAME_SAMPLES; i++) {
            for (int ch = 0; ch < AUDIO_NUM_CHANNELS; ch++) {
                int32_t s;
                memcpy(&s,
                       &input[(i * AUDIO_NUM_CHANNELS + ch) * AUDIO_BYTES_PER_SAMPLE],
                       sizeof(s));
                sample_buffer[rec_pos][ch] = s;
            }
            rec_pos++;
            if (rec_pos >= STUTTER_SAMPLES) {
                recording  = false;
                stuttering = true;
                rec_pos    = 0;  // reset for potential next record
                break;
            }
        }
        memcpy(output, input, AUDIO_FRAME_BYTES);
        return;
    }

    if (stuttering) {
        for (int i = 0; i < AUDIO_FRAME_SAMPLES; i++) {
            for (int ch = 0; ch < AUDIO_NUM_CHANNELS; ch++) {
                int32_t s = sample_buffer[read_pos][ch];
                memcpy(&output[(i * AUDIO_NUM_CHANNELS + ch) * AUDIO_BYTES_PER_SAMPLE],
                       &s,
                       sizeof(s));
            }
            read_pos++;
            if (read_pos >= STUTTER_SAMPLES) {
                read_pos = 0;
            }
        }
        return;
    }

    memcpy(output, input, AUDIO_FRAME_BYTES);
}
