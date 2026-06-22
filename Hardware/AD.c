#include "stm32f10x.h"
#include "AD.h"
#include "AppConfig.h"

#define AD_DMA_BUF_LEN          (ADC_SAMPLE_LEN * 2)

static uint16_t AD_RawBuf[AD_DMA_BUF_LEN];
static volatile uint8_t AD_FrameReady = 0;
static volatile uint8_t AD_Running = 0;

static void AD_ConfigRegularChannels(void)
{
    ADC_RegularChannelConfig(ADC1, UI_ADC_CHANNEL, 1, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, UO_ADC_CHANNEL, 2, ADC_SampleTime_55Cycles5);
}

static void AD_ConfigADCScanDMA(void)
{
    ADC_InitTypeDef ADC_InitStructure;

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 2;
    ADC_Init(ADC1, &ADC_InitStructure);

    AD_ConfigRegularChannels();
}

static void AD_ConfigADCSingle(uint8_t ADC_Channel)
{
    ADC_InitTypeDef ADC_InitStructure;

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    ADC_RegularChannelConfig(ADC1, ADC_Channel, 1, ADC_SampleTime_55Cycles5);
}

void AD_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);        // ADC 时钟 72MHz / 6 = 12MHz

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    DMA_DeInit(DMA1_Channel1);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)AD_RawBuf;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize = AD_DMA_BUF_LEN;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    AD_ConfigADCScanDMA();

    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1) == SET);
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1) == SET);

    ADC_Cmd(ADC1, DISABLE);
    ADC_DMACmd(ADC1, DISABLE);
    AD_FrameReady = 0;
    AD_Running = 0;
}

void AD_Start(void)
{
    ADC_SoftwareStartConvCmd(ADC1, DISABLE);
    ADC_DMACmd(ADC1, DISABLE);
    ADC_Cmd(ADC1, DISABLE);

    DMA_Cmd(DMA1_Channel1, DISABLE);
    DMA_SetCurrDataCounter(DMA1_Channel1, AD_DMA_BUF_LEN);
    DMA_ClearFlag(DMA1_FLAG_GL1 | DMA1_FLAG_TC1 | DMA1_FLAG_HT1 | DMA1_FLAG_TE1);
    AD_FrameReady = 0;

    AD_ConfigADCScanDMA();
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    ADC_DMACmd(ADC1, ENABLE);
    DMA_Cmd(DMA1_Channel1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    AD_Running = 1;
}

uint8_t AD_IsFrameReady(void)
{
    return AD_FrameReady;
}

void AD_ClearFrameReady(void)
{
    AD_FrameReady = 0;
    AD_Start();
}

void AD_GetFrame(float *ui_buf, float *uo_buf, uint16_t len)
{
    uint16_t i;
    uint16_t copy_len;
    float k;

    if ((ui_buf == 0) || (uo_buf == 0))
    {
        return;
    }

    copy_len = len;
    if (copy_len > ADC_SAMPLE_LEN)
    {
        copy_len = ADC_SAMPLE_LEN;
    }

    k = ADC_REF_VOLT / ADC_MAX_CODE_F;

    for (i = 0; i < copy_len; i++)
    {
        ui_buf[i] = (float)AD_RawBuf[i * 2] * k;
        uo_buf[i] = (float)AD_RawBuf[i * 2 + 1] * k;
    }
}

uint16_t AD_GetValue(uint8_t ADC_Channel)
{
    uint32_t timeout;
    uint16_t value;
    uint8_t was_running;

    value = 0;
    was_running = AD_Running;

    ADC_SoftwareStartConvCmd(ADC1, DISABLE);
    ADC_DMACmd(ADC1, DISABLE);
    ADC_Cmd(ADC1, DISABLE);
    DMA_Cmd(DMA1_Channel1, DISABLE);
    DMA_ClearFlag(DMA1_FLAG_GL1 | DMA1_FLAG_TC1 | DMA1_FLAG_HT1 | DMA1_FLAG_TE1);
    AD_Running = 0;

    AD_ConfigADCSingle(ADC_Channel);
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC);
    ADC_Cmd(ADC1, ENABLE);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    timeout = 100000;
    while ((ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) && (timeout > 0))
    {
        timeout--;
    }

    if (timeout != 0)
    {
        value = ADC_GetConversionValue(ADC1);
    }

    ADC_SoftwareStartConvCmd(ADC1, DISABLE);
    ADC_Cmd(ADC1, DISABLE);
    AD_ConfigADCScanDMA();

    if (was_running)
    {
        AD_Start();
    }

    return value;
}

void AD_DMA1_Channel1_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC1) != RESET)
    {
        DMA_ClearITPendingBit(DMA1_IT_GL1);
        ADC_SoftwareStartConvCmd(ADC1, DISABLE);
        ADC_DMACmd(ADC1, DISABLE);
        ADC_Cmd(ADC1, DISABLE);
        DMA_Cmd(DMA1_Channel1, DISABLE);
        AD_Running = 0;
        AD_FrameReady = 1;
    }
}
