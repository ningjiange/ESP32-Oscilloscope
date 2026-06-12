idf_component_register(
    SRCS "ili9341_display.c" "breath_led.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_driver_gpio esp_driver_ledc osc_core
)
