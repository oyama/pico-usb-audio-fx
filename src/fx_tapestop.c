#include <math.h>
#include <string.h>

#include "fx.h"
#include "ringbuffer.h"

static float playback_pos = 0.0f;
static float playback_speed = 1.0f;
static bool is_slowing_down = false;
static bool is_recovering = false;

static float prev_out_l = 0.0f, prev_out_r = 0.0f;
static const float frame_slow_factor = 0.995213f;
static const float fs = 48000.0f;
static const float nyquist = fs * 0.5f;
static const float dt = 1.0f / fs;

static int32_t sample_buffer[TOTAL_SAMPLES][AUDIO_NUM_CHANNELS];
static volatile uint32_t write_sample_pos = 0;

#define FC_TABLE_SIZE 256
#define MIX_TABLE_SIZE 256
static float fc_table[FC_TABLE_SIZE];
static float mix_table[FC_TABLE_SIZE];

static void init_mix_table(void) {
    const float gamma = 1.5f;
    for (int i = 0; i < MIX_TABLE_SIZE; ++i) {
        float norm = (float)i / (MIX_TABLE_SIZE - 1);
        mix_table[i] = (norm >= 0.9f) ? 1.0f : powf(norm, gamma);
    }
}

static void init_fc_table(void) {
    const float fc_min = 500.0f;
    const float fc_max = 24000.0f;
    const float gamma = 0.5f;

    for (int i = 0; i < FC_TABLE_SIZE; ++i) {
        float norm = (float)i / (FC_TABLE_SIZE - 1);
        float shaped = powf(norm, gamma);
        fc_table[i] = fc_min * powf((fc_max / fc_min), shaped);
    }
}

const char *fx_name(void) { return "Pico Audio FX TapeStop"; }

void fx_init(void) {
    init_mix_table();
    init_fc_table();
}

void fx_set_enable(bool enable) {
    if (enable) {
        is_slowing_down = true;
        is_recovering = false;
    } else {
        if (is_slowing_down) {
            is_recovering = true;
            playback_speed = 0.0f;
        }
        is_slowing_down = false;
    }
}

void fx_process(uint8_t *output, uint8_t *input) {
    for (int i = 0; i < AUDIO_FRAME_SAMPLES; i++) {
        for (int ch = 0; ch < AUDIO_NUM_CHANNELS; ch++) {
            int32_t sample;
            memcpy(&sample, &input[(i * AUDIO_NUM_CHANNELS + ch) * 4], 4);
            sample_buffer[write_sample_pos][ch] = sample;
        }
        write_sample_pos = (write_sample_pos + 1) % TOTAL_SAMPLES;
    }

    const size_t frames = AUDIO_FRAME_SAMPLES;
    if (is_slowing_down) {
        playback_speed *= frame_slow_factor;
        if (playback_speed < 0.00001f)
            playback_speed = 0.0f;
    }
    if (is_recovering) {
        playback_speed += (1.0f - playback_speed) * 0.003f;
        if (playback_speed >= 0.999f) {
            playback_speed = 1.0f;
            is_recovering = false;
        }
    }

    float speed = playback_speed;
    float fc = speed * nyquist;
    float RC = 1.0f / (2.0f * M_PI * fc + 1e-9f);
    float alpha = dt / (RC + dt);
    alpha = fmaxf(alpha, 0.001f);

    int32_t *out_ptr = (int32_t *)output;
    for (size_t i = 0; i < frames; i++) {
        int pos_int = ((int)playback_pos) % TOTAL_SAMPLES;
        int next_pos = (pos_int + 1) % TOTAL_SAMPLES;
        float frac = playback_pos - floorf(playback_pos);

        for (int ch = 0; ch < AUDIO_NUM_CHANNELS; ch++) {
            int32_t s1 = sample_buffer[pos_int][ch];
            int32_t s2 = sample_buffer[next_pos][ch];
            int32_t interp = (frac < 1e-4f) ? s1 : (int32_t)(s1 * (1.0f - frac) + s2 * frac);

            float raw_f = (float)interp;
            float filt_f = (ch == 0) ? (prev_out_l + alpha * (raw_f - prev_out_l))
                                     : (prev_out_r + alpha * (raw_f - prev_out_r));
            if (ch == 0)
                prev_out_l = filt_f;
            else
                prev_out_r = filt_f;

            float norm = (speed <= 0.0f) ? 0.0f : speed / 0.9f;
            float mix = 1.0f;
            if (speed < 0.9f) {
                int index = (int)(norm * (MIX_TABLE_SIZE - 1));
                if (index < 0)
                    index = 0;
                if (index >= MIX_TABLE_SIZE)
                    index = MIX_TABLE_SIZE - 1;
                mix = mix_table[index];
            }

            *out_ptr++ = (int32_t)(filt_f * (1.0f - mix) + raw_f * mix);
        }

        if (speed > 0.0f) {
            playback_pos += speed;
            if (playback_pos >= TOTAL_SAMPLES)
                playback_pos -= TOTAL_SAMPLES;
        }
    }
}
