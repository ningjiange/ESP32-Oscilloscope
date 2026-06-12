#include "ili9341_display.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LCD_HOST SPI2_HOST
#define LCD_SPI_CLOCK_HZ (20 * 1000 * 1000)
#define COLOR_BG 0x0000
#define COLOR_GRID 0x39E7
#define COLOR_GRID_MINOR 0x18E3
#define COLOR_TEXT 0xFFFF
#define COLOR_WAVE 0x07E0
#define COLOR_ACCENT 0xFFE0
#define COLOR_CENTER 0x7BEF

static const char *TAG = "ili9341";
static spi_device_handle_t s_lcd;
static uint16_t *s_framebuffer;
static uint16_t *s_dma_lines;

static void lcd_cmd(uint8_t cmd)
{
    gpio_set_level(BOARD_PIN_LCD_DC, 0);
    spi_transaction_t transaction = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_lcd, &transaction));
}

static void lcd_data(const void *data, size_t len)
{
    if (len == 0) {
        return;
    }
    gpio_set_level(BOARD_PIN_LCD_DC, 1);
    spi_transaction_t transaction = {
        .length = len * 8,
        .tx_buffer = data,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_lcd, &transaction));
}

static void lcd_cmd_data(uint8_t cmd, const uint8_t *data, size_t len)
{
    lcd_cmd(cmd);
    lcd_data(data, len);
}

static void lcd_reset(void)
{
    gpio_set_level(BOARD_PIN_LCD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(BOARD_PIN_LCD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t caset[] = {x0 >> 8, x0 & 0xff, x1 >> 8, x1 & 0xff};
    uint8_t raset[] = {y0 >> 8, y0 & 0xff, y1 >> 8, y1 & 0xff};
    lcd_cmd_data(0x2A, caset, sizeof(caset));
    lcd_cmd_data(0x2B, raset, sizeof(raset));
    lcd_cmd(0x2C);
}

static void lcd_init_sequence(void)
{
    lcd_reset();
    lcd_cmd(0x01);
    vTaskDelay(pdMS_TO_TICKS(120));

    const uint8_t pixel_format[] = {0x55};
    lcd_cmd_data(0x3A, pixel_format, sizeof(pixel_format));

    const uint8_t memory_access[] = {0x28};
    lcd_cmd_data(0x36, memory_access, sizeof(memory_access));

    const uint8_t power_b[] = {0x00, 0xC1, 0x30};
    lcd_cmd_data(0xCF, power_b, sizeof(power_b));
    const uint8_t power_seq[] = {0x64, 0x03, 0x12, 0x81};
    lcd_cmd_data(0xED, power_seq, sizeof(power_seq));
    const uint8_t driver_timing_a[] = {0x85, 0x00, 0x78};
    lcd_cmd_data(0xE8, driver_timing_a, sizeof(driver_timing_a));
    const uint8_t power_a[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    lcd_cmd_data(0xCB, power_a, sizeof(power_a));
    const uint8_t pump_ratio[] = {0x20};
    lcd_cmd_data(0xF7, pump_ratio, sizeof(pump_ratio));
    const uint8_t driver_timing_b[] = {0x00, 0x00};
    lcd_cmd_data(0xEA, driver_timing_b, sizeof(driver_timing_b));
    const uint8_t power_control_1[] = {0x23};
    lcd_cmd_data(0xC0, power_control_1, sizeof(power_control_1));
    const uint8_t power_control_2[] = {0x10};
    lcd_cmd_data(0xC1, power_control_2, sizeof(power_control_2));
    const uint8_t vcom_1[] = {0x3E, 0x28};
    lcd_cmd_data(0xC5, vcom_1, sizeof(vcom_1));
    const uint8_t vcom_2[] = {0x86};
    lcd_cmd_data(0xC7, vcom_2, sizeof(vcom_2));

    lcd_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_cmd(0x29);
}

static void put_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= ILI9341_WIDTH || y < 0 || y >= ILI9341_HEIGHT) {
        return;
    }
    s_framebuffer[y * ILI9341_WIDTH + x] = color;
}

static void draw_hline(int x0, int x1, int y, uint16_t color)
{
    if (y < 0 || y >= ILI9341_HEIGHT) {
        return;
    }
    if (x0 < 0) {
        x0 = 0;
    }
    if (x1 >= ILI9341_WIDTH) {
        x1 = ILI9341_WIDTH - 1;
    }
    for (int x = x0; x <= x1; ++x) {
        put_pixel(x, y, color);
    }
}

static void draw_vline(int x, int y0, int y1, uint16_t color)
{
    if (x < 0 || x >= ILI9341_WIDTH) {
        return;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (y1 >= ILI9341_HEIGHT) {
        y1 = ILI9341_HEIGHT - 1;
    }
    for (int y = y0; y <= y1; ++y) {
        put_pixel(x, y, color);
    }
}

static void draw_line(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y0 - y1 : y1 - y0;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;

    while (true) {
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static uint8_t font5x7(char c, int col)
{
    static const uint8_t blank[5] = {0, 0, 0, 0, 0};
    static const uint8_t minus[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
    static const uint8_t dot[5] = {0, 0, 0x60, 0x60, 0};
    static const uint8_t colon[5] = {0, 0x36, 0x36, 0, 0};
    static const uint8_t percent[5] = {0x62, 0x64, 0x08, 0x13, 0x23};
    static const uint8_t slash[5] = {0x40, 0x20, 0x10, 0x08, 0x04};
    static const uint8_t digits[10][5] = {
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, {0x00, 0x42, 0x7F, 0x40, 0x00},
        {0x42, 0x61, 0x51, 0x49, 0x46}, {0x21, 0x41, 0x45, 0x4B, 0x31},
        {0x18, 0x14, 0x12, 0x7F, 0x10}, {0x27, 0x45, 0x45, 0x45, 0x39},
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, {0x01, 0x71, 0x09, 0x05, 0x03},
        {0x36, 0x49, 0x49, 0x49, 0x36}, {0x06, 0x49, 0x49, 0x29, 0x1E},
    };
    static const uint8_t letters[26][5] = {
        {0x7E, 0x11, 0x11, 0x11, 0x7E}, {0x7F, 0x49, 0x49, 0x49, 0x36},
        {0x3E, 0x41, 0x41, 0x41, 0x22}, {0x7F, 0x41, 0x41, 0x22, 0x1C},
        {0x7F, 0x49, 0x49, 0x49, 0x41}, {0x7F, 0x09, 0x09, 0x09, 0x01},
        {0x3E, 0x41, 0x49, 0x49, 0x7A}, {0x7F, 0x08, 0x08, 0x08, 0x7F},
        {0x00, 0x41, 0x7F, 0x41, 0x00}, {0x20, 0x40, 0x41, 0x3F, 0x01},
        {0x7F, 0x08, 0x14, 0x22, 0x41}, {0x7F, 0x40, 0x40, 0x40, 0x40},
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, {0x7F, 0x04, 0x08, 0x10, 0x7F},
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, {0x7F, 0x09, 0x09, 0x09, 0x06},
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, {0x7F, 0x09, 0x19, 0x29, 0x46},
        {0x46, 0x49, 0x49, 0x49, 0x31}, {0x01, 0x01, 0x7F, 0x01, 0x01},
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, {0x1F, 0x20, 0x40, 0x20, 0x1F},
        {0x3F, 0x40, 0x38, 0x40, 0x3F}, {0x63, 0x14, 0x08, 0x14, 0x63},
        {0x07, 0x08, 0x70, 0x08, 0x07}, {0x61, 0x51, 0x49, 0x45, 0x43},
    };

    const uint8_t *glyph = blank;
    if (c >= '0' && c <= '9') {
        glyph = digits[c - '0'];
    } else if (c >= 'A' && c <= 'Z') {
        glyph = letters[c - 'A'];
    } else if (c >= 'a' && c <= 'z') {
        glyph = letters[c - 'a'];
    } else if (c == '-') {
        glyph = minus;
    } else if (c == '.') {
        glyph = dot;
    } else if (c == ':') {
        glyph = colon;
    } else if (c == '%') {
        glyph = percent;
    } else if (c == '/') {
        glyph = slash;
    }
    return glyph[col];
}

static void draw_char(int x, int y, char c, uint16_t color)
{
    for (int col = 0; col < 5; ++col) {
        uint8_t bits = font5x7(c, col);
        for (int row = 0; row < 7; ++row) {
            if (bits & (1 << row)) {
                put_pixel(x + col, y + row, color);
            }
        }
    }
}

static void draw_text(int x, int y, uint16_t color, const char *fmt, ...)
{
    char text[96];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);

    for (size_t i = 0; text[i] != '\0'; ++i) {
        draw_char(x + (int)i * 6, y, text[i], color);
    }
}

static void draw_grid(void)
{
    for (int x = 0; x < ILI9341_WIDTH; x += 40) {
        draw_vline(x, OSC_CORE_WAVE_TOP, OSC_CORE_WAVE_BOTTOM, COLOR_GRID);
    }
    for (int y = OSC_CORE_WAVE_TOP; y <= OSC_CORE_WAVE_BOTTOM; y += 30) {
        draw_hline(0, ILI9341_WIDTH - 1, y, COLOR_GRID);
    }
    draw_hline(0, ILI9341_WIDTH - 1, OSC_CORE_WAVE_TOP, COLOR_ACCENT);
    draw_hline(0, ILI9341_WIDTH - 1, OSC_CORE_WAVE_BOTTOM, COLOR_ACCENT);

    const int center_y = (OSC_CORE_WAVE_TOP + OSC_CORE_WAVE_BOTTOM) / 2;
    draw_hline(0, ILI9341_WIDTH - 1, center_y, COLOR_CENTER);

    for (int x = 0; x < ILI9341_WIDTH; x += 10) {
        int tick = (x % 40) == 0 ? 5 : 2;
        draw_vline(x, OSC_CORE_WAVE_TOP, OSC_CORE_WAVE_TOP + tick, COLOR_GRID_MINOR);
        draw_vline(x, OSC_CORE_WAVE_BOTTOM - tick, OSC_CORE_WAVE_BOTTOM, COLOR_GRID_MINOR);
    }
    for (int y = OSC_CORE_WAVE_TOP; y <= OSC_CORE_WAVE_BOTTOM; y += 10) {
        int tick = ((y - OSC_CORE_WAVE_TOP) % 30) == 0 ? 5 : 2;
        draw_hline(0, tick, y, COLOR_GRID_MINOR);
        draw_hline(ILI9341_WIDTH - 1 - tick, ILI9341_WIDTH - 1, y, COLOR_GRID_MINOR);
    }
}

static void flush_framebuffer(void)
{
    lcd_set_window(0, 0, ILI9341_WIDTH - 1, ILI9341_HEIGHT - 1);
    gpio_set_level(BOARD_PIN_LCD_DC, 1);

    const size_t rows_per_chunk = 16;
    for (size_t y = 0; y < ILI9341_HEIGHT; y += rows_per_chunk) {
        size_t rows = rows_per_chunk;
        if (y + rows > ILI9341_HEIGHT) {
            rows = ILI9341_HEIGHT - y;
        }
        memcpy(s_dma_lines, &s_framebuffer[y * ILI9341_WIDTH], ILI9341_WIDTH * rows * sizeof(uint16_t));
        spi_transaction_t transaction = {
            .length = ILI9341_WIDTH * rows * 16,
            .tx_buffer = s_dma_lines,
        };
        ESP_ERROR_CHECK(spi_device_polling_transmit(s_lcd, &transaction));
    }
}

esp_err_t ili9341_display_init(void)
{
    gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << BOARD_PIN_LCD_DC) | (1ULL << BOARD_PIN_LCD_RST) | (1ULL << BOARD_PIN_LCD_BL),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&output_config), TAG, "gpio config failed");

    spi_bus_config_t bus_config = {
        .mosi_io_num = BOARD_PIN_LCD_MOSI,
        .miso_io_num = BOARD_PIN_LCD_MISO,
        .sclk_io_num = BOARD_PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = ILI9341_WIDTH * 16 * 2,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO), TAG, "spi bus failed");

    spi_device_interface_config_t device_config = {
        .clock_speed_hz = LCD_SPI_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = BOARD_PIN_LCD_CS,
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(LCD_HOST, &device_config, &s_lcd), TAG, "spi device failed");

    s_framebuffer = heap_caps_malloc(ILI9341_WIDTH * ILI9341_HEIGHT * sizeof(uint16_t), MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(s_framebuffer, ESP_ERR_NO_MEM, TAG, "framebuffer allocation failed");
    s_dma_lines = heap_caps_malloc(ILI9341_WIDTH * 16 * sizeof(uint16_t), MALLOC_CAP_DMA);
    ESP_RETURN_ON_FALSE(s_dma_lines, ESP_ERR_NO_MEM, TAG, "dma line allocation failed");

    gpio_set_level(BOARD_PIN_LCD_BL, 1);
    lcd_init_sequence();
    memset(s_framebuffer, 0, ILI9341_WIDTH * ILI9341_HEIGHT * sizeof(uint16_t));
    flush_framebuffer();

    return ESP_OK;
}

void ili9341_display_draw(const uint16_t *points, uint16_t point_count, const osc_metrics_t *metrics,
                          uint32_t sample_rate_hz, uint32_t window_ms)
{
    if (!s_framebuffer || !points || point_count < 2 || !metrics) {
        return;
    }

    memset(s_framebuffer, 0, ILI9341_WIDTH * ILI9341_HEIGHT * sizeof(uint16_t));
    draw_grid();

    uint16_t mv_per_div = (uint16_t)((metrics->vpp_mv + 3U) / 6U);
    if (mv_per_div == 0) {
        mv_per_div = 1;
    }

    draw_text(2, 5, COLOR_TEXT, "CH1 ENV WIN:%lu.%lus MAX:%umV", (unsigned long)(window_ms / 1000U),
              (unsigned long)((window_ms % 1000U) / 100U), metrics->max_mv);
    draw_text(2, 224, COLOR_TEXT, "V/DIV:%umV VPP:%umV", mv_per_div, metrics->vpp_mv);
    draw_text(204, 224, COLOR_TEXT, "AVG:%umV", metrics->avg_mv);

    uint16_t max_points = point_count > ILI9341_WIDTH ? ILI9341_WIDTH : point_count;
    for (uint16_t x = 1; x < max_points; ++x) {
        draw_line(x - 1, points[x - 1], x, points[x], COLOR_WAVE);
    }

    flush_framebuffer();
}
