#pragma once

#include <stdbool.h>
#include <stdint.h>

const char *fx_name(void);
void fx_init(void);
void fx_set_enable(bool enable);
void fx_process(uint8_t *output, uint8_t *input);
