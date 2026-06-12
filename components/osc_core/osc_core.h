#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OSC_CORE_ADC_MAX 4095
#define OSC_CORE_SCREEN_WIDTH 320
#define OSC_CORE_WAVE_TOP 24
#define OSC_CORE_WAVE_BOTTOM 207

typedef struct {
    uint16_t min_raw;
    uint16_t max_raw;
    uint16_t avg_raw;
    uint16_t min_mv;
    uint16_t max_mv;
    uint16_t avg_mv;
    uint16_t vpp_mv;
    uint32_t frequency_hz;
    uint16_t duty_permille;
} osc_metrics_t;

void osc_core_analyze(const uint16_t *samples, size_t count, uint32_t sample_rate_hz, osc_metrics_t *out);
void osc_core_make_points(const uint16_t *samples, size_t count, uint16_t *points, size_t point_count);
void osc_core_make_auto_points(const uint16_t *samples, size_t count, uint16_t *points, size_t point_count);
void osc_core_make_smoothed_points(const uint16_t *samples, size_t count, uint16_t *smoothed, uint16_t *points,
                                   size_t point_count);
uint16_t osc_core_trimmed_mean(const uint16_t *samples, size_t count);
void osc_core_scroll_history(uint16_t *history, size_t count, uint16_t value, size_t advance);
size_t osc_core_find_rising_trigger(const uint16_t *samples, size_t count);
void osc_core_copy_triggered(const uint16_t *samples, size_t count, size_t trigger_index, uint16_t *out, size_t out_count);
uint16_t osc_core_raw_to_mv(uint16_t raw);

#ifdef __cplusplus
}
#endif
