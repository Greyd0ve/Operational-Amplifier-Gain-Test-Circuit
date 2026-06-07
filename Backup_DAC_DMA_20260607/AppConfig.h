#ifndef __APPCONFIG_H
#define __APPCONFIG_H

#include "stm32f10x.h"

#define SYSCLK_HZ               72000000UL

#define FREQ_MIN_HZ             1000
#define FREQ_MAX_HZ             2000
#define FREQ_STEP_HZ            50

#define DAC_UPDATE_RATE_HZ      8000
#define WAVE_TABLE_SIZE         256

#define MCP4725_ADDR_7BIT       0x60
#define MCP4725_ADDR_8BIT       0xC0

#define DAC_BITS                12
#define DAC_MAX_CODE            4095
#define DAC_MID_CODE            2048

#define ADC_REF_VOLT            3.3f
#define ADC_MAX_CODE_F          4095.0f
#define ADC_BIAS_VOLT           1.65f

#define ADC_SAMPLE_LEN          256

#define UI_ADC_CHANNEL          ADC_Channel_0
#define UO_ADC_CHANNEL          ADC_Channel_1

#define UI_SCALE                1.0f
#define UO_SCALE                1.0f

#define US_INIT_RMS             0.1f
#define UO_TARGET_RMS           1.5f

#define AMP_SCALE_INIT          1.0f
#define AMP_SCALE_MIN           0.05f
#define AMP_SCALE_MAX           5.0f

#define AUTO_GAIN_DEADBAND      0.005f
#define AUTO_GAIN_STEP_MIN      0.80f
#define AUTO_GAIN_STEP_MAX      1.25f

#define UI_MIN_RMS              0.005f
#define UO_SAFE_MAX_RMS         2.0f

#define DISPLAY_UPDATE_MS       200
#define MEASURE_UPDATE_MS       100
#define KEY_SCAN_PERIOD_MS      10

#define KEY_UP_PORT             GPIOB
#define KEY_UP_PIN              GPIO_Pin_1
#define KEY_DOWN_PORT           GPIOB
#define KEY_DOWN_PIN            GPIO_Pin_11
#define KEY_AUTO_PORT           GPIOB
#define KEY_AUTO_PIN            GPIO_Pin_10
#define KEY_MODE_PORT           GPIOB
#define KEY_MODE_PIN            GPIO_Pin_12

#endif
