#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OSC_ADC_SAMPLE_RATE_HZ 20000

esp_err_t osc_adc_init(void);
size_t osc_adc_read(uint16_t *samples, size_t sample_count);

#ifdef __cplusplus
}
#endif
