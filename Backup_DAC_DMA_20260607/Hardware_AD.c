#include "stm32f10x.h"                  // Device header
#include "AppConfig.h"

/**
  * 函    数：AD初始化
  * 参    数：无
  * 返 回 值：无
  */
void AD_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
	RCC_ADCCLKConfig(RCC_PCLK2_Div6);		// ADCCLK = 72MHz / 6 = 12MHz
	
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	ADC_InitTypeDef ADC_InitStructure;
	ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_ScanConvMode = DISABLE;
	ADC_InitStructure.ADC_NbrOfChannel = 1;
	ADC_Init(ADC1, &ADC_InitStructure);
	
	ADC_Cmd(ADC1, ENABLE);
	
	ADC_ResetCalibration(ADC1);
	while (ADC_GetResetCalibrationStatus(ADC1) == SET);
	ADC_StartCalibration(ADC1);
	while (ADC_GetCalibrationStatus(ADC1) == SET);
}

/**
  * 函    数：获取单次AD转换值
  * 参    数：ADC_Channel 指定ADC通道
  * 返 回 值：0~4095
  */
uint16_t AD_GetValue(uint8_t ADC_Channel)
{
	uint32_t Timeout = 10000;
	
	ADC_RegularChannelConfig(ADC1, ADC_Channel, 1, ADC_SampleTime_55Cycles5);
	ADC_SoftwareStartConvCmd(ADC1, ENABLE);
	while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET)
	{
		if (Timeout-- == 0)
		{
			return 0;
		}
	}
	return ADC_GetConversionValue(ADC1);
}

/**
  * 函    数：采集一帧Ui和Uo电压
  * 参    数：ui_buf Ui电压缓冲区，单位V
  * 参    数：uo_buf Uo电压缓冲区，单位V
  * 参    数：len 采样点数
  * 说    明：基础版采用轮询采样，简单可靠；后续可升级为TIM3触发ADC扫描+DMA。
  */
void AD_GetFrame(float *ui_buf, float *uo_buf, uint16_t len)
{
	uint16_t i;
	uint16_t ui_raw;
	uint16_t uo_raw;
	
	for (i = 0; i < len; i ++)
	{
		ui_raw = AD_GetValue(UI_ADC_CHANNEL);
		uo_raw = AD_GetValue(UO_ADC_CHANNEL);
		
		ui_buf[i] = ui_raw * ADC_REF_VOLT / ADC_MAX_CODE_F;
		uo_buf[i] = uo_raw * ADC_REF_VOLT / ADC_MAX_CODE_F;
	}
}
