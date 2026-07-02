#ifndef __APPCONFIG_H
#define __APPCONFIG_H

#include "stm32f10x.h"

#define SYSCLK_HZ               72000000UL

#define FREQ_MIN_HZ             1000
#define FREQ_MAX_HZ             2000
#define FREQ_STEP_HZ            50

/* DAC wave table length is selected dynamically to reduce frequency error. */
#define WAVE_TABLE_SIZE_MIN     256
#define WAVE_TABLE_SIZE_MAX     600

#define DAC_BITS                12
#define DAC_MAX_CODE            4095
#define DAC_MID_CODE            420.6
#define DAC_VREF                3.3f

#define DAC_SINE_CAL_SCALE      1.006f
#define DAC_TRI_CAL_SCALE       0.906f

#define ADC_REF_VOLT            3.3f
#define ADC_MAX_CODE_F          4095.0f
#define ADC_SAMPLE_LEN          512
#define ADC_SAMPLE_RATE_HZ      88235.3f
#define ADC_UO_DELAY_SAMPLES    0.5f

#define UI_ADC_CHANNEL          ADC_Channel_1
#define UO_ADC_CHANNEL          ADC_Channel_2

#define UI_SCALE                1.0f
#define UO_SCALE                1.805f

#define US_INIT_RMS             0.1f
#define UO_TARGET_RMS           1.5f

#define AMP_SCALE_INIT          1.0f
#define AMP_SCALE_MIN           0.05f
#define AMP_SCALE_MAX           10.0f

#define AUTO_GAIN_STEP_MIN      0.80f
#define AUTO_GAIN_STEP_MAX      1.25f
#define AUTO_GAIN_CLIP_BACKOFF  0.90f

#define UI_MIN_RMS              0.005f

#define DISPLAY_UPDATE_MS       200
#define KEY_SCAN_PERIOD_MS      10
#define KEY_LONG_PRESS_MS       800

#define KEY1_GPIO               GPIOA
#define KEY1_PIN                GPIO_Pin_0
#define KEY1_RCC                RCC_APB2Periph_GPIOA
#define KEY1_ACTIVE_LEVEL       1

#define KEY2_GPIO               GPIOC
#define KEY2_PIN                GPIO_Pin_8
#define KEY2_RCC                RCC_APB2Periph_GPIOC
#define KEY2_ACTIVE_LEVEL       0

#define KEY3_GPIO               GPIOC
#define KEY3_PIN                GPIO_Pin_9
#define KEY3_RCC                RCC_APB2Periph_GPIOC
#define KEY3_ACTIVE_LEVEL       0

#endif
