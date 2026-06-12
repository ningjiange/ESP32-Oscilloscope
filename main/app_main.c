#include <string.h>
#include <stdio.h>

#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "breath_led.h"
#include "ili9341_display.h"
#include "osc_adc.h"
#include "osc_core.h"

#define FRAME_SAMPLE_COUNT ILI9341_WIDTH
#define CAPTURE_SAMPLE_COUNT (FRAME_SAMPLE_COUNT * 2)
#define HISTORY_ADVANCE_PIXELS 4
#define FRAME_DELAY_MS 60
#define HISTORY_WINDOW_MS ((FRAME_SAMPLE_COUNT * FRAME_DELAY_MS) / HISTORY_ADVANCE_PIXELS)

static uint16_t s_capture[CAPTURE_SAMPLE_COUNT];
static uint16_t s_history[FRAME_SAMPLE_COUNT];
static uint16_t s_points[FRAME_SAMPLE_COUNT];

static void print_vofa_voltage(uint16_t millivolts)
{
    printf("%u.%03u", millivolts / 1000U, millivolts % 1000U);
}

static void print_vofa_frame(uint16_t envelope_mv, const osc_metrics_t *metrics)
{
    print_vofa_voltage(envelope_mv);
    putchar(',');
    print_vofa_voltage(metrics->max_mv);
    putchar(',');
    print_vofa_voltage(metrics->avg_mv);
    putchar('\n');
}

void app_main(void)
{
    ESP_ERROR_CHECK(ili9341_display_init());
    ESP_ERROR_CHECK(breath_led_init());
    ESP_ERROR_CHECK(osc_adc_init());

    osc_metrics_t metrics = {0};

    while (true) {
        size_t count = osc_adc_read(s_capture, CAPTURE_SAMPLE_COUNT);
        if (count == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (count < CAPTURE_SAMPLE_COUNT) {
            uint16_t fill = s_capture[count - 1];
            for (size_t i = count; i < CAPTURE_SAMPLE_COUNT; ++i) {
                s_capture[i] = fill;
            }
            count = CAPTURE_SAMPLE_COUNT;
        }

        static uint16_t filtered_raw;
        uint16_t current_raw = osc_core_trimmed_mean(s_capture, count);
        if (filtered_raw == 0) {
            filtered_raw = current_raw;
        } else {
            filtered_raw = (uint16_t)(((uint32_t)filtered_raw * 3U + current_raw + 2U) / 4U);
        }
        osc_core_scroll_history(s_history, FRAME_SAMPLE_COUNT, filtered_raw, HISTORY_ADVANCE_PIXELS);

        osc_core_make_auto_points(s_history, FRAME_SAMPLE_COUNT, s_points, FRAME_SAMPLE_COUNT);
        osc_core_analyze(s_history, FRAME_SAMPLE_COUNT, OSC_ADC_SAMPLE_RATE_HZ, &metrics);
        ili9341_display_draw(s_points, FRAME_SAMPLE_COUNT, &metrics, OSC_ADC_SAMPLE_RATE_HZ, HISTORY_WINDOW_MS);
        print_vofa_frame(osc_core_raw_to_mv(filtered_raw), &metrics);
        breath_led_update((uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS));
        vTaskDelay(pdMS_TO_TICKS(FRAME_DELAY_MS));
    }
}
