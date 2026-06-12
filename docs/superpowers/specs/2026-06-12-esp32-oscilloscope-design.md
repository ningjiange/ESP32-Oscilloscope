# ESP32 Oscilloscope First Version Design

## Goal

Build a first working oscilloscope firmware for an ESP32-WROOM-32 board with a 320x240 ILI9341 SPI display. The first target signal is a low-voltage PWM breathing LED signal.

## Scope

- One ADC channel on GPIO36 / VP.
- One 320x240 ILI9341 display over SPI.
- No touch input.
- No Taojingchi serial display.
- No external analog front-end beyond simple input protection or divider wiring.
- Display a live waveform, grid, status text, and PWM measurements.

## Hardware Assumptions

- ILI9341 wiring:
  - SCK GPIO18
  - MOSI GPIO23
  - MISO GPIO19 optional
  - CS GPIO5
  - DC GPIO16
  - RESET GPIO17
  - Backlight GPIO4
- ADC input:
  - CH1 on GPIO36 / ADC1 channel 0.
  - PWM signal is 0 to 3.3 V, or divided before entering the ADC pin.
  - The measured circuit and ESP32 share GND.

## Architecture

The firmware is split into three layers:

- `osc_core`: pure C signal processing. It converts raw ADC samples into display points and computes min, max, Vpp, average, frequency, and duty cycle.
- `osc_adc`: ESP-IDF ADC continuous sampling wrapper. It fills a sample buffer with ADC raw values.
- `board`: display and pin-level hardware drivers. The first version uses a compact ILI9341 framebuffer renderer instead of depending on LVGL being installed.

`app_main` owns the main loop: read ADC samples, process them, render the oscilloscope screen, and refresh the ILI9341.

## Display

The display uses a 16-bit RGB565 framebuffer. Each frame draws:

- dark background
- oscilloscope grid
- top status line
- waveform polyline
- bottom measurement line

The screen is intentionally simple and direct for the first version. It can later be wrapped behind LVGL or replaced with an LVGL canvas without changing `osc_core`.

## Sampling

The first version uses ESP-IDF `adc_continuous` at 20 kSPS by default. It collects 320 samples per frame so one screen column maps to one sample. This is enough to inspect typical PWM LED signals and breathing duty changes.

## Measurements

For each frame:

- raw ADC min, max, and average are converted to millivolts using a 3300 mV full scale approximation
- Vpp is `max - min`
- PWM frequency and duty cycle are estimated from threshold crossings around the average signal level

## Testing

Host-side Python tests cover the pure signal processing behavior using known synthetic PWM samples. ESP-IDF build verifies the firmware compiles against the hardware APIs.
