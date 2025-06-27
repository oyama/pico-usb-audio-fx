/*
 * Copyright 2025, Hiroyuki OYAMA
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "bsp/board_api.h"
#include "tusb.h"

#include "usb_descriptors.h"
#include "led.h"
#include "fx.h"

#define _PID_MAP(itf, n) ((CFG_TUD_##itf) << (n))
#define USB_PID                                                                            \
    (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | _PID_MAP(MIDI, 3) | \
     _PID_MAP(AUDIO, 4) | _PID_MAP(VENDOR, 5))
#define CONFIG_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_AUDIO * TUD_AUDIO_INTERFACE_STEREO_DESC_LEN)

#define EPNUM_AUDIO_IN 0x01
#define EPNUM_AUDIO_OUT 0x01
#define EPNUM_AUDIO_INT 0x02

enum {
    STRID_LANGID = 0,
    STRID_MANUFACTURER,
    STRID_PRODUCT,
    STRID_SERIAL,
};

tusb_desc_device_t const desc_device = {.bLength = sizeof(tusb_desc_device_t),
                                        .bDescriptorType = TUSB_DESC_DEVICE,
                                        .bcdUSB = 0x0200,
                                        .bDeviceClass = TUSB_CLASS_MISC,
                                        .bDeviceSubClass = MISC_SUBCLASS_COMMON,
                                        .bDeviceProtocol = MISC_PROTOCOL_IAD,
                                        .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

                                        .idVendor = 0xCafe,
                                        .idProduct = USB_PID,
                                        .bcdDevice = 0x0100,

                                        .iManufacturer = 0x01,
                                        .iProduct = 0x02,
                                        .iSerialNumber = 0x03,

                                        .bNumConfigurations = 0x01};

uint8_t const desc_configuration[] = {
    // Config number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),
    // Interface number, string index, EP Out & EP In address, EP size
    TUD_AUDIO_INTERFACE_STEREO_DESCRIPTOR(2, EPNUM_AUDIO_OUT, EPNUM_AUDIO_IN | 0x80,
                                          EPNUM_AUDIO_INT | 0x80)};

char const *string_desc_arr[] = {
    (const char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    "TinyUSB",                   // 1: Manufacturer
    "Pico Audio FX",             // 2: Product
    NULL,                        // 3: Serials will use unique ID if possible
    "FX Output",                 // 4: Audio Interface
    "FX Input",                  // 5: Audio Interface
};

static uint16_t _desc_str[32 + 1];

static const uint32_t supported_sample_rates[] = {44100, 48000};
static uint32_t current_sample_rate = 48000;
#define N_SAMPLE_RATES TU_ARRAY_SIZE(supported_sample_rates)
static uint8_t current_resolution = 24;

uint32_t usb_current_sample_rate(void) {
    return current_sample_rate;
}

uint8_t const *tud_descriptor_device_cb(void) { return (uint8_t const *)&desc_device; }

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
    (void)index;
    return desc_configuration;
}

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
    (void)langid;
    size_t chr_count;

    switch (index) {
        case STRID_LANGID:
            memcpy(&_desc_str[1], string_desc_arr[0], 2);
            chr_count = 1;
            break;

        case STRID_SERIAL:
            chr_count = board_usb_get_serial(_desc_str + 1, 32);
            break;

        case STRID_PRODUCT:

        default:
            // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
            // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors
            if (!(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])))
                return NULL;

            const char *str = string_desc_arr[index];
            if (index == STRID_PRODUCT)
                str = fx_name();

            chr_count = strlen(str);
            size_t const max_count =
                sizeof(_desc_str) / sizeof(_desc_str[0]) - 1;  // -1 for string type
            if (chr_count > max_count)
                chr_count = max_count;

            for (size_t i = 0; i < chr_count; i++) {
                _desc_str[1 + i] = str[i];
            }
            break;
    }

    _desc_str[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

    return _desc_str;
}

void tud_mount_cb(void) {
    led_set_blink_interval(BLINK_MOUNTED);
}

void tud_umount_cb(void) {
    led_set_blink_interval(BLINK_NOT_MOUNTED);
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    led_set_blink_interval(BLINK_SUSPENDED);
}

void tud_resume_cb(void) {
    led_set_blink_interval(tud_mounted() ? BLINK_MOUNTED : BLINK_NOT_MOUNTED);
}

static bool tud_audio_clock_get_request(uint8_t rhport, audio_control_request_t const *request) {
    if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ) {
        if (request->bRequest == AUDIO_CS_REQ_CUR) {
            audio_control_cur_4_t curf = {(int32_t)tu_htole32(current_sample_rate)};
            return tud_audio_buffer_and_schedule_control_xfer(
                rhport, (tusb_control_request_t const *)request, &curf, sizeof(curf));
        } else if (request->bRequest == AUDIO_CS_REQ_RANGE) {
            audio_control_range_4_n_t(N_SAMPLE_RATES)
                rangef = {.wNumSubRanges = tu_htole16(N_SAMPLE_RATES)};
            for (uint8_t i = 0; i < N_SAMPLE_RATES; i++) {
                rangef.subrange[i].bMin = (int32_t)supported_sample_rates[i];
                rangef.subrange[i].bMax = (int32_t)supported_sample_rates[i];
                rangef.subrange[i].bRes = 0;
            }
            return tud_audio_buffer_and_schedule_control_xfer(
                rhport, (tusb_control_request_t const *)request, &rangef, sizeof(rangef));
        }
    } else if (request->bControlSelector == AUDIO_CS_CTRL_CLK_VALID &&
               request->bRequest == AUDIO_CS_REQ_CUR) {
        audio_control_cur_1_t cur_valid = {.bCur = 1};
        return tud_audio_buffer_and_schedule_control_xfer(
            rhport, (tusb_control_request_t const *)request, &cur_valid, sizeof(cur_valid));
    }
    return false;
}

static bool tud_audio_clock_set_request(uint8_t rhport, audio_control_request_t const *request,
                                        uint8_t const *buf) {
    (void)rhport;

    TU_ASSERT(request->bEntityID == UAC2_ENTITY_CLOCK);
    TU_VERIFY(request->bRequest == AUDIO_CS_REQ_CUR);

    if (request->bControlSelector == AUDIO_CS_CTRL_SAM_FREQ) {
        TU_VERIFY(request->wLength == sizeof(audio_control_cur_4_t));

        current_sample_rate = (uint32_t)((audio_control_cur_4_t const *)buf)->bCur;
        return true;
    } else {
        TU_LOG1("Clock set request not supported, entity = %u, selector = %u, request = %u\r\n",
                request->bEntityID, request->bControlSelector, request->bRequest);
        return false;
    }
}

// Invoked when audio class specific get request received for an entity
bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    audio_control_request_t const *request = (audio_control_request_t const *)p_request;

    if (request->bEntityID == UAC2_ENTITY_CLOCK)
        return tud_audio_clock_get_request(rhport, request);

    return false;
}

// Invoked when audio class specific set request received for an entity
bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request,
                                 uint8_t *buf) {
    audio_control_request_t const *request = (audio_control_request_t const *)p_request;

    if (request->bEntityID == UAC2_ENTITY_CLOCK)
        return tud_audio_clock_set_request(rhport, request, buf);

    return false;
}

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void)rhport;
    uint8_t const itf = tu_u16_low(tu_le16toh(p_request->wIndex));
    uint8_t const alt = tu_u16_low(tu_le16toh(p_request->wValue));

    if (ITF_NUM_AUDIO_STREAMING_SPK == itf && alt != 0)
        led_set_blink_interval(BLINK_STREAMING);

    if (alt != 0) {
        current_resolution = CFG_TUD_AUDIO_FUNC_1_FORMAT_1_RESOLUTION_RX;
    }
    return true;
}
