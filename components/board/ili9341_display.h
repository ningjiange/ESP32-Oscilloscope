#pragma once

#include <stdint.h>

#include "esp_err.h"
#include "osc_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ILI9341_WIDTH 320
#define ILI9341_HEIGHT 240

esp_err_t ili9341_display_init(void);
void ili9341_display_draw(const uint16_t *points, uint16_t point_count, const osc_metrics_t *metrics,
                          uint32_t sample_rate_hz, uint32_t window_ms);

#ifdef __cplusplus
}
#endif
