# ESP32 简易示波器演示

这是一个基于 ESP-IDF 的 ESP32 简易示波器演示项目，使用 2.4 寸 320x240 ILI9341 SPI 屏显示波形。

当前版本主要用于演示“PWM 呼吸灯信号”的采样和显示：

- 使用 GPIO25 通过 LEDC PWM 驱动绿色 LED 呼吸灯。
- 使用 VP / GPIO36 通过 ADC 连续采样 LED 节点电压。
- 在 ILI9341 SPI 屏上显示呼吸灯包络趋势，而不是直接显示高速 PWM 方波。
- 屏幕显示网格、边缘刻度、时间窗口、最大电压、每格电压、峰峰值和平均电压。

## 硬件

- ESP32-WROOM-32 最小系统板
- 2.4 寸 320x240 ILI9341 SPI 显示屏
- 绿色 LED
- 1 kOhm 电阻

推荐信号接线：

```text
GPIO25 -> 1k 电阻 -> LED 正极 / VP 采样点
LED 负极 -> GND
VP(GPIO36) -> LED 正极 / 采样点
GND 共地
```

LCD 引脚定义在 `components/board/board_pins.h`。

## 屏幕标注

- `CH1`：当前通道，采样输入为 VP / GPIO36。
- `ENV`：当前显示的是包络/亮度趋势，不是原始 PWM 方波。
- `WIN`：整屏横向时间窗口。
- `MAX`：当前屏幕历史波形中的最大电压。
- `V/DIV`：当前自动缩放下每个纵向大格约等于多少毫伏。
- `VPP`：峰峰值，等于当前屏幕内最高电压减最低电压。
- `AVG`：当前屏幕历史波形的平均电压。

## 构建

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\build.ps1
```

## 烧录

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\scripts\flash.ps1 -Port COM14
```

如果你的 ESP32 串口不是 `COM14`，请改成实际端口。

## 测试

```powershell
D:\DevTools\.espressif\python_env\idf5.4_py3.13_env\Scripts\python.exe tests\test_osc_core.py
```

## 目录结构

```text
main/                  应用入口
components/board/      屏幕、引脚和呼吸灯驱动
components/osc_adc/    ADC 连续采样
components/osc_core/   波形分析、缩放、滤波和历史缓冲
scripts/               Windows PowerShell 构建/烧录脚本
tests/                 核心算法测试
tools/                 测试辅助工具
```
