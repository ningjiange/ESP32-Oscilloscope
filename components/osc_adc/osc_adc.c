#include "osc_adc.h"

#include <string.h>

#include "board_pins.h"
#include "esp_adc/adc_continuous.h"
#include "esp_check.h"

static const char *TAG = "osc_adc";
static adc_continuous_handle_t s_adc_handle;

esp_err_t osc_adc_init(void)
{
    adc_continuous_handle_cfg_t handle_config = {
        .max_store_buf_size = 4096,
        .conv_frame_size = 512,
    };
    ESP_RETURN_ON_ERROR(adc_continuous_new_handle(&handle_config, &s_adc_handle), TAG, "new adc handle failed");

    adc_digi_pattern_config_t pattern = {
        .atten = BOARD_ADC_ATTEN,
        .channel = BOARD_ADC_CHANNEL,
        .unit = BOARD_ADC_UNIT,
        .bit_width = BOARD_ADC_BITWIDTH,
    };
    adc_continuous_config_t adc_config = {
        .sample_freq_hz = OSC_ADC_SAMPLE_RATE_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
        .pattern_num = 1,
        .adc_pattern = &pattern,
    };
    ESP_RETURN_ON_ERROR(adc_continuous_config(s_adc_handle, &adc_config), TAG, "adc config failed");
    ESP_RETURN_ON_ERROR(adc_continuous_start(s_adc_handle), TAG, "adc start failed");
    return ESP_OK;
}

size_t osc_adc_read(uint16_t *samples, size_t sample_count)
{
    if (!samples || sample_count == 0 || !s_adc_handle) {
        return 0;
    }

    uint8_t raw[512];
    size_t written = 0;

    while (written < sample_count) {
        uint32_t bytes_read = 0;
        esp_err_t err = adc_continuous_read(s_adc_handle, raw, sizeof(raw), &bytes_read, 100);
        if (err == ESP_ERR_TIMEOUT) {
            break;
        }
        if (err != ESP_OK) {
            break;
        }

        for (uint32_t offset = 0; offset + sizeof(adc_digi_output_data_t) <= bytes_read;
             offset += sizeof(adc_digi_output_data_t)) {
            adc_digi_output_data_t data;
            memcpy(&data, &raw[offset], sizeof(data));
            if (data.type1.channel == BOARD_ADC_CHANNEL) {
                samples[written++] = data.type1.data;
                if (written >= sample_count) {
                    break;
                }
            }
        }
    }

    return written;
}
