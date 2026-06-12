#include "osc_core.h"

#include <stdbool.h>
#include <string.h>

uint16_t osc_core_raw_to_mv(uint16_t raw)
{
    if (raw > OSC_CORE_ADC_MAX) {
        raw = OSC_CORE_ADC_MAX;
    }
    return (uint16_t)(((uint32_t)raw * 3300U) / OSC_CORE_ADC_MAX);
}

static uint16_t raw_to_y(uint16_t raw)
{
    if (raw > OSC_CORE_ADC_MAX) {
        raw = OSC_CORE_ADC_MAX;
    }
    const uint32_t height = OSC_CORE_WAVE_BOTTOM - OSC_CORE_WAVE_TOP;
    return (uint16_t)(OSC_CORE_WAVE_BOTTOM - (((uint32_t)raw * height) / OSC_CORE_ADC_MAX));
}

void osc_core_make_points(const uint16_t *samples, size_t count, uint16_t *points, size_t point_count)
{
    if (!samples || !points || count == 0 || point_count == 0) {
        return;
    }

    for (size_t i = 0; i < point_count; ++i) {
        size_t src = (i * count) / point_count;
        if (src >= count) {
            src = count - 1;
        }
        points[i] = raw_to_y(samples[src]);
    }
}

void osc_core_make_auto_points(const uint16_t *samples, size_t count, uint16_t *points, size_t point_count)
{
    if (!samples || !points || count == 0 || point_count == 0) {
        return;
    }

    uint16_t min_raw = OSC_CORE_ADC_MAX;
    uint16_t max_raw = 0;
    for (size_t i = 0; i < count; ++i) {
        uint16_t raw = samples[i] > OSC_CORE_ADC_MAX ? OSC_CORE_ADC_MAX : samples[i];
        if (raw < min_raw) {
            min_raw = raw;
        }
        if (raw > max_raw) {
            max_raw = raw;
        }
    }

    uint16_t span = max_raw > min_raw ? max_raw - min_raw : 1;
    if (span < 80) {
        span = 80;
    }

    int32_t center = ((int32_t)min_raw + (int32_t)max_raw) / 2;
    int32_t low = center - (int32_t)span / 2;
    int32_t high = center + (int32_t)span / 2;
    const int32_t margin = span / 8;
    low -= margin;
    high += margin;
    if (high <= low) {
        high = low + 1;
    }

    const int32_t height = OSC_CORE_WAVE_BOTTOM - OSC_CORE_WAVE_TOP;
    for (size_t i = 0; i < point_count; ++i) {
        size_t src = (i * count) / point_count;
        if (src >= count) {
            src = count - 1;
        }
        int32_t raw = samples[src];
        if (raw < low) {
            raw = low;
        }
        if (raw > high) {
            raw = high;
        }
        points[i] = (uint16_t)(OSC_CORE_WAVE_BOTTOM - (((raw - low) * height) / (high - low)));
    }
}

void osc_core_make_smoothed_points(const uint16_t *samples, size_t count, uint16_t *smoothed, uint16_t *points,
                                   size_t point_count)
{
    if (!samples || !smoothed || !points || count == 0 || point_count == 0) {
        return;
    }

    const size_t window = 40;
    for (size_t i = 0; i < point_count; ++i) {
        size_t start = (i * count) / point_count;
        size_t end = (((i + 1U) * count) / point_count);
        if (end <= start) {
            end = start + 1U;
        }
        if (end - start < window) {
            if (count <= window) {
                start = 0;
                end = count;
            } else if (start + window <= count) {
                end = start + window;
            } else {
                start = count - window;
                end = count;
            }
        }

        uint32_t sum = 0;
        uint16_t min_raw = OSC_CORE_ADC_MAX;
        uint16_t max_raw = 0;
        size_t used = 0;
        for (size_t src = start; src < end; ++src) {
            uint16_t raw = samples[src] > OSC_CORE_ADC_MAX ? OSC_CORE_ADC_MAX : samples[src];
            sum += raw;
            if (raw < min_raw) {
                min_raw = raw;
            }
            if (raw > max_raw) {
                max_raw = raw;
            }
            used++;
        }
        if (used >= 5) {
            sum -= min_raw;
            sum -= max_raw;
            used -= 2;
        }
        smoothed[i] = used > 0 ? (uint16_t)((sum + (used / 2U)) / used) : 0;
    }

    osc_core_make_auto_points(smoothed, point_count, points, point_count);
}

uint16_t osc_core_trimmed_mean(const uint16_t *samples, size_t count)
{
    if (!samples || count == 0) {
        return 0;
    }

    uint32_t sum = 0;
    uint32_t low_sum = 0;
    uint32_t high_sum = 0;
    size_t low_count = 0;
    size_t high_count = 0;
    uint16_t low_limit = OSC_CORE_ADC_MAX / 20U;
    uint16_t high_limit = OSC_CORE_ADC_MAX - low_limit;

    for (size_t i = 0; i < count; ++i) {
        uint16_t raw = samples[i] > OSC_CORE_ADC_MAX ? OSC_CORE_ADC_MAX : samples[i];
        sum += raw;
        if (raw <= low_limit) {
            low_sum += raw;
            low_count++;
        }
        if (raw >= high_limit) {
            high_sum += raw;
            high_count++;
        }
    }

    if (count > 16 && low_count > 0 && high_count > 0 && low_count + high_count < count / 4U) {
        sum -= low_sum;
        sum -= high_sum;
        count -= low_count + high_count;
    }

    return (uint16_t)((sum + (count / 2U)) / count);
}

void osc_core_scroll_history(uint16_t *history, size_t count, uint16_t value, size_t advance)
{
    if (!history || count == 0 || advance == 0) {
        return;
    }
    if (advance > count) {
        advance = count;
    }

    uint16_t previous = history[count - advance - 1U];
    memmove(history, history + advance, (count - advance) * sizeof(history[0]));
    for (size_t i = 0; i < advance; ++i) {
        uint32_t numerator = (uint32_t)previous * (advance - i - 1U) + (uint32_t)value * (i + 1U);
        history[count - advance + i] = (uint16_t)((numerator + (advance / 2U)) / advance);
    }
}

size_t osc_core_find_rising_trigger(const uint16_t *samples, size_t count)
{
    if (!samples || count < 4) {
        return 0;
    }

    uint16_t min_raw = OSC_CORE_ADC_MAX;
    uint16_t max_raw = 0;
    for (size_t i = 0; i < count; ++i) {
        uint16_t raw = samples[i] > OSC_CORE_ADC_MAX ? OSC_CORE_ADC_MAX : samples[i];
        if (raw < min_raw) {
            min_raw = raw;
        }
        if (raw > max_raw) {
            max_raw = raw;
        }
    }

    if (max_raw <= min_raw || max_raw - min_raw < 32) {
        return 0;
    }

    const uint16_t threshold = min_raw + ((max_raw - min_raw) / 2);
    bool previous_high = samples[0] >= threshold;
    for (size_t i = 1; i < count; ++i) {
        bool high = samples[i] >= threshold;
        if (high && !previous_high) {
            return i;
        }
        previous_high = high;
    }
    return 0;
}

void osc_core_copy_triggered(const uint16_t *samples, size_t count, size_t trigger_index, uint16_t *out, size_t out_count)
{
    if (!samples || !out || count == 0 || out_count == 0) {
        return;
    }

    if (trigger_index >= count) {
        trigger_index = 0;
    }
    for (size_t i = 0; i < out_count; ++i) {
        size_t src = trigger_index + i;
        if (src >= count) {
            src = count - 1;
        }
        out[i] = samples[src];
    }
}

static void measure_pwm(const uint16_t *samples, size_t count, uint32_t sample_rate_hz, uint16_t threshold,
                        uint32_t *frequency_hz, uint16_t *duty_permille)
{
    *frequency_hz = 0;
    *duty_permille = 0;

    if (count < 4 || sample_rate_hz == 0) {
        return;
    }

    size_t first_rise = count;
    size_t second_rise = count;
    bool previous_high = samples[0] >= threshold;

    for (size_t i = 1; i < count; ++i) {
        bool high = samples[i] >= threshold;
        if (high && !previous_high) {
            if (first_rise == count) {
                first_rise = i;
            } else {
                second_rise = i;
                break;
            }
        }
        previous_high = high;
    }

    if (first_rise == count || second_rise == count || second_rise <= first_rise) {
        return;
    }

    size_t high_count = 0;
    for (size_t i = first_rise; i < second_rise; ++i) {
        if (samples[i] >= threshold) {
            high_count++;
        }
    }

    const size_t period_samples = second_rise - first_rise;
    *frequency_hz = (uint32_t)((sample_rate_hz + (period_samples / 2U)) / period_samples);
    *duty_permille = (uint16_t)((high_count * 1000U + (period_samples / 2U)) / period_samples);
}

void osc_core_analyze(const uint16_t *samples, size_t count, uint32_t sample_rate_hz, osc_metrics_t *out)
{
    if (!out) {
        return;
    }

    *out = (osc_metrics_t){0};
    if (!samples || count == 0) {
        return;
    }

    uint16_t min_raw = OSC_CORE_ADC_MAX;
    uint16_t max_raw = 0;
    uint32_t sum = 0;

    for (size_t i = 0; i < count; ++i) {
        uint16_t raw = samples[i] > OSC_CORE_ADC_MAX ? OSC_CORE_ADC_MAX : samples[i];
        if (raw < min_raw) {
            min_raw = raw;
        }
        if (raw > max_raw) {
            max_raw = raw;
        }
        sum += raw;
    }

    out->min_raw = min_raw;
    out->max_raw = max_raw;
    out->avg_raw = (uint16_t)((sum + (count / 2U)) / count);
    out->min_mv = osc_core_raw_to_mv(out->min_raw);
    out->max_mv = osc_core_raw_to_mv(out->max_raw);
    out->avg_mv = osc_core_raw_to_mv(out->avg_raw);
    out->vpp_mv = out->max_mv - out->min_mv;

    measure_pwm(samples, count, sample_rate_hz, out->avg_raw, &out->frequency_hz, &out->duty_permille);
}
