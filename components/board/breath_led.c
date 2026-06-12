#include "breath_led.h"

#include "board_pins.h"
#include "driver/ledc.h"
#include "esp_check.h"

#define BREATH_LED_TIMER LEDC_TIMER_0
#define BREATH_LED_MODE LEDC_LOW_SPEED_MODE
#define BREATH_LED_CHANNEL LEDC_CHANNEL_0
#define BREATH_LED_FREQ_HZ 5000
#define BREATH_LED_DUTY_MAX ((1U << 10) - 1U)
#define BREATH_LED_PERIOD_MS 5000

static const char *TAG = "breath_led";

esp_err_t breath_led_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = BREATH_LED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = BREATH_LED_TIMER,
        .freq_hz = BREATH_LED_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "timer config failed");

    ledc_channel_config_t channel = {
        .gpio_num = BOARD_PIN_LED_PWM,
        .speed_mode = BREATH_LED_MODE,
        .channel = BREATH_LED_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BREATH_LED_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "channel config failed");
    return ESP_OK;
}

void breath_led_update(uint32_t now_ms)
{
    uint32_t phase = now_ms % BREATH_LED_PERIOD_MS;
    uint32_t half = BREATH_LED_PERIOD_MS / 2U;
    uint32_t ramp = phase < half ? phase : (BREATH_LED_PERIOD_MS - phase);
    uint32_t x = (ramp * 1024U) / half;
    uint32_t smooth = (3U * x * x - ((2U * x * x * x) / 1024U)) / 1024U;
    uint32_t duty = (smooth * BREATH_LED_DUTY_MAX) / 1024U;

    ESP_ERROR_CHECK(ledc_set_duty(BREATH_LED_MODE, BREATH_LED_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(BREATH_LED_MODE, BREATH_LED_CHANNEL));
}
