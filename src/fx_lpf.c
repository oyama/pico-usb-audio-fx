#include <math.h>
#include <string.h>

#include "fx.h"
#include "ringbuffer.h"

#define FC_TABLE_SIZE 128

typedef struct {
    float b0, b1, b2;
    float a1, a2;
    float z1, z2;
} biquad_t;

static const float fc_min = 500.0f;
static const float fc_max = 24000.0f;
static float fc_table[FC_TABLE_SIZE];
static float fc_control = 0.0f;

static void biquad_calc_lowpass(float fs, float fc, float q, float *b0, float *b1, float *b2,
                                float *a1, float *a2) {
    float omega = 2.0f * M_PI * (fc / fs);
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float alpha = sin_omega / (2.0f * q);

    float a0_inv = 1.0f / (1.0f + alpha);
    *b0 = ((1.0f - cos_omega) / 2.0f) * a0_inv;
    *b1 = (1.0f - cos_omega) * a0_inv;
    *b2 = *b0;
    *a1 = -2.0f * cos_omega * a0_inv;
    *a2 = (1.0f - alpha) * a0_inv;
}

static void biquad_update_coeff(biquad_t *bq, float fs, float fc, float q) {
    biquad_calc_lowpass(fs, fc, q, &bq->b0, &bq->b1, &bq->b2, &bq->a1, &bq->a2);
}

static void biquad_set(biquad_t *bq, float fs, float fc, float q) {
    biquad_update_coeff(bq, fs, fc, q);
    bq->z1 = 0.0f;
    bq->z2 = 0.0f;
}

static float biquad_process(biquad_t *bq, float x) {
    float y = bq->b0 * x + bq->b1 * bq->z1 + bq->b2 * bq->z2 - bq->a1 * bq->z1 - bq->a2 * bq->z2;
    bq->z2 = bq->z1;
    bq->z1 = y;
    return y;
}

static void biquad_process_buffer(int32_t *buf, size_t frames, size_t stride, biquad_t *bq) {
    for (size_t i = 0; i < frames; i++, buf += stride) {
        float x = (float)(buf[0]) / (1 << 8);  // 24bitスロット →  float変換
        float y = biquad_process(bq, x);
        buf[0] = (int32_t)(y * (1 << 8));  // float →  24bitスロット
    }
}

static void init_fc_table(void) {
    const float gamma = 0.5f;

    for (int i = 0; i < FC_TABLE_SIZE; ++i) {
        float norm = (float)i / (FC_TABLE_SIZE - 1);
        float shaped = powf(norm, gamma);
        fc_table[i] = fc_min * powf((fc_max / fc_min), shaped);
    }
}

const char *fx_name(void) { return "Pico Audio FX LPF"; }

void fx_init(void) { init_fc_table(); }

void fx_set_enable(bool enable) {
    if (enable) {
        fc_control -= 0.0005f;
        if (fc_control < 0.0f)
            fc_control = 0.0f;

    } else {
        fc_control += 0.0002f;
        if (fc_control > 1.0f)
            fc_control = 1.0f;
    }
}

void fx_process(uint8_t *output, uint8_t *input) {
    int index = (int)(fc_control * (FC_TABLE_SIZE - 1));
    if (index < 0)
        index = 0;
    if (index >= FC_TABLE_SIZE)
        index = FC_TABLE_SIZE - 1;
    float fc_current = fc_table[index];

    static biquad_t lpf_l, lpf_r;
    static biquad_t lpf_l2, lpf_r2;
    static bool initialized = false;
    if (!initialized) {
        biquad_set(&lpf_l, 48000.0f, fc_current, 6.0f);
        biquad_set(&lpf_r, 48000.0f, fc_current, 6.0f);
        biquad_set(&lpf_l2, 48000.0f, fc_current, 6.0f);
        biquad_set(&lpf_r2, 48000.0f, fc_current, 6.0f);
        initialized = true;
    }

    memcpy(output, input, AUDIO_FRAME_BYTES);

    size_t stride = 2;
    size_t frames = AUDIO_FRAME_SAMPLES;
    static float fc_prev = 0.0f;
    if (fabsf(fc_current - fc_prev) > 100.0f) {
        biquad_update_coeff(&lpf_l, 48000.0f, fc_current, 6.0f);
        biquad_update_coeff(&lpf_r, 48000.0f, fc_current, 6.0f);
        biquad_update_coeff(&lpf_l2, 48000.0f, fc_current, 6.0f);
        biquad_update_coeff(&lpf_r2, 48000.0f, fc_current, 6.0f);
        fc_prev = fc_current;
    }
    biquad_process_buffer((int32_t *)output + 0, frames, stride, &lpf_l);
    biquad_process_buffer((int32_t *)output + 0, frames, stride, &lpf_l2);
    biquad_process_buffer((int32_t *)output + 1, frames, stride, &lpf_r);
    biquad_process_buffer((int32_t *)output + 1, frames, stride, &lpf_r2);
}
