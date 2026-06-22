# 运算放大电路增益测试电路设计

这是一个基于 STM32F103RCT6 的电子设计竞赛校赛项目软件工程，用于产生测试信号、采集运放输入/输出信号，并计算交流有效值与电压增益。

## 开发环境

- 主控芯片：STM32F103RCT6
- IDE：Keil MDK
- 外设库：STM32F10x Standard Peripheral Library
- 不使用 HAL、CubeMX、Arduino

## 主要功能

- PA4 / DAC_OUT1 输出测试信号 Us
- 支持正弦波和三角波输出
- 输出频率范围 1000Hz - 2000Hz
- 频率步进由 `FREQ_STEP_HZ` 宏控制
- DAC 使用 TIM6 触发，DMA 循环搬运波形表
- ADC1 采集 Ui、Uo 两路信号
- 计算并显示：
  - Ui 直流平均值
  - Uo 直流平均值
  - Ui 交流有效值
  - Uo 交流有效值
  - Gain = Uo_rms / Ui_rms
- 支持自动调幅，使 Uo 接近目标有效值
- OLED 显示主页面和调试页面

## IO 分配

| 功能 | 引脚 | 说明 |
| --- | --- | --- |
| DAC 输出 | PA4 | DAC_OUT1，输出测试信号 Us |
| Ui 采样 | PA1 | ADC1_IN1 |
| Uo 采样 | PA2 | ADC1_IN2 |
| OLED SCL | PB4 | 软件 I2C |
| OLED SDA | PB5 | 软件 I2C |
| KEY1 | PA0 | 频率调节 |
| KEY2 | PC8 | 自动调幅 / 页面切换 |
| KEY3 | PC9 | 波形切换 |

## 按键说明

| 按键 | 操作 | 功能 |
| --- | --- | --- |
| KEY1 | 短按 | 频率增加 |
| KEY1 | 长按 | 频率减少 |
| KEY2 | 短按 | 开启或关闭自动调幅 |
| KEY2 | 长按 | 切换 OLED 页面 |
| KEY3 | 短按 | 切换正弦波 / 三角波 |
| KEY3 | 长按 | 预留 |

## 目录结构

```text
App/        应用层逻辑、测量计算、自动调幅配置
Hardware/   DAC、ADC、按键、OLED 等硬件驱动
Library/    STM32F10x 标准外设库
Start/      启动和系统文件
System/     延时等基础模块
User/       main.c 和中断文件
```

## 关键配置

常用参数在 `App/AppConfig.h` 中配置：

- `FREQ_MIN_HZ`
- `FREQ_MAX_HZ`
- `FREQ_STEP_HZ`
- `WAVE_TABLE_SIZE`
- `DAC_SINE_CAL_SCALE`
- `DAC_TRI_CAL_SCALE`
- `US_INIT_RMS`
- `UO_TARGET_RMS`
- `AMP_SCALE_MIN`
- `AMP_SCALE_MAX`

## 调试建议

1. 先测试 PA4 DAC 输出。
2. 使用数字毫伏表测量正弦波和三角波 AC RMS。
3. 根据实测值修正 `DAC_SINE_CAL_SCALE` 和 `DAC_TRI_CAL_SCALE`。
4. 测试 PA1 ADC 直流采样：
   - PA1 接 GND，UiD 应接近 0V。
   - PA1 接 3.3V，UiD 应接近 3.3V。
   - PA1 接 PA4，UiD 应接近 DAC 波形直流中心。
5. 确认 Ui AC RMS 能正确变化。
6. 接入运放测试板，测试 Uo 和 Gain。
7. 最后开启自动调幅功能。

## 编译下载

使用 Keil MDK 打开 `Project.uvprojx`，执行 Rebuild 后下载到 STM32F103RCT6 开发板。

## 注意事项

- PA0 是板载按键，不作为 ADC 输入。
- DAC Channel1 在 STM32F103RCT6 上使用 DMA2_Channel3。
- ADC1 使用 DMA1_Channel1。
- 主页面显示的是交流有效值 AC RMS。
- 调试页面显示的 UiD、UoD 是直流平均值。
- 自动调幅默认关闭，测试 DAC 基础输出幅值时应保持 A:OFF。
