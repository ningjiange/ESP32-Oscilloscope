#pragma once

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t breath_led_init(void);
void breath_led_update(uint32_t now_ms);

#ifdef __cplusplus
}
#endif
