# ESP32 Oscilloscope First Version Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a first ESP32 firmware that samples GPIO36 and draws a live PWM waveform on a 320x240 ILI9341 SPI display.

**Architecture:** Keep signal processing in a host-testable pure C module, keep ADC and display drivers isolated, and make `app_main` a small loop that connects capture, processing, and rendering. Use direct ILI9341 framebuffer rendering for this first version so the project builds without downloading LVGL components.

**Tech Stack:** ESP-IDF 5.4, C, ESP32 ADC continuous driver, SPI master driver, Python host tests.

---

### Task 1: Project Skeleton and Core Tests

**Files:**
- Create: `CMakeLists.txt`
- Create: `main/CMakeLists.txt`
- Create: `tests/test_osc_core.py`

- [ ] **Step 1: Write failing host tests**

Create Python tests that expect a CLI helper to process synthetic PWM samples and return JSON metrics.

- [ ] **Step 2: Run the tests to verify failure**

Run: `py tests\test_osc_core.py`

Expected: FAIL because `tools\osc_core_probe.py` does not exist yet.

### Task 2: Pure Signal Processing Core

**Files:**
- Create: `components/osc_core/CMakeLists.txt`
- Create: `components/osc_core/osc_core.h`
- Create: `components/osc_core/osc_core.c`
- Create: `tools/osc_core_probe.py`

- [ ] **Step 1: Implement the minimal core API**

Add sample statistics, display point mapping, and PWM measurement functions.

- [ ] **Step 2: Run host tests**

Run: `py tests\test_osc_core.py`

Expected: PASS.

### Task 3: Board and Display Driver

**Files:**
- Create: `components/board/CMakeLists.txt`
- Create: `components/board/board_pins.h`
- Create: `components/board/ili9341_display.h`
- Create: `components/board/ili9341_display.c`

- [ ] **Step 1: Add board pin definitions**

Define ILI9341, backlight, and ADC pins.

- [ ] **Step 2: Add ILI9341 renderer**

Initialize SPI and ILI9341, maintain a RGB565 framebuffer, draw grid, text, and waveform, then flush to display.

### Task 4: ADC Driver and Main Loop

**Files:**
- Create: `components/osc_adc/CMakeLists.txt`
- Create: `components/osc_adc/osc_adc.h`
- Create: `components/osc_adc/osc_adc.c`
- Create: `main/app_main.c`

- [ ] **Step 1: Add ADC continuous sampling wrapper**

Configure GPIO36 / ADC1 channel 0 at 20 kSPS.

- [ ] **Step 2: Add main loop**

Read samples, process them through `osc_core`, draw the display, and delay for roughly 25 FPS.

### Task 5: Verification

**Files:**
- Modify as needed if compilation exposes API mismatches.

- [ ] **Step 1: Run host tests**

Run: `py tests\test_osc_core.py`

Expected: PASS.

- [ ] **Step 2: Build firmware**

Run: `idf.py build`

Expected: build succeeds.
