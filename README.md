# ESP32 Oscilloscope Demo

ESP-IDF project for a small ESP32 oscilloscope-style display demo using a 2.4 inch ILI9341 SPI LCD.

Current demo:

- Drives a green LED breathing waveform from GPIO25 with LEDC PWM.
- Samples the LED node through VP / GPIO36 using ADC continuous mode.
- Displays the envelope trend on a 320x240 ILI9341 SPI LCD.
- Shows grid, edge ticks, time window, max voltage, volts-per-division, VPP, and average voltage.

## Hardware

- ESP32-WROOM-32 minimum system board
- 2.4 inch 320x240 ILI9341 SPI display
- Green LED
- 1 kOhm resistor

Suggested signal wiring:

```text
GPIO25 -> 1k resistor -> LED anode / VP sample node
LED cathode -> GND
VP(GPIO36) -> LED anode / sample node
GND shared
```

LCD pins are defined in `components/board/board_pins.h`.

## Build

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build.ps1
```

## Flash

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\flash.ps1 -Port COM14
```

Change `COM14` to the serial port used by your ESP32.

## Tests

```powershell
D:\DevTools\.espressif\python_env\idf5.4_py3.13_env\Scripts\python.exe tests\test_osc_core.py
```
