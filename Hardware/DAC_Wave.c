#include "stm32f10x.h"
#include "DAC_Wave.h"
#include "AppConfig.h"
#include <math.h>

#define DAC_TWO_PI              6.283185307f
#define DAC_SQRT2               1.414213562f
#define DAC_NORM_MAX            32767

static uint16_t DACWave_Table[WAVE_TABLE_SIZE];
static int16_t DACWave_NormTable[WAVE_TABLE_SIZE];

static uint32_t DACWave_FreqHz = FREQ_MIN_HZ;
static float DACWave_AmpScale = AMP_SCALE_INIT;
static Waveform_t DACWave_Waveform = WAVEFORM_SINE;
static uint8_t DACWave_Running = 0;

static float DACWave_LimitFloat(float value, float min, float max)
{
    if (value < min)
    {
        return min;
    }

    if (value > max)
    {
        return max;
    }

    return value;
}

static uint32_t DACWave_LimitFreq(uint32_t freq_hz)
{
    if (freq_hz < FREQ_MIN_HZ)
    {
        return FREQ_MIN_HZ;
    }

    if (freq_hz > FREQ_MAX_HZ)
    {
        return FREQ_MAX_HZ;
    }

    return freq_hz;
}

static float DACWave_GetTriangleSample(uint16_t index)
{
    float pos;

    pos = (float)index / (float)WAVE_TABLE_SIZE;

    if (pos < 0.25f)
    {
        return pos * 4.0f;
    }
    else if (pos < 0.75f)
    {
        return 2.0f - pos * 4.0f;
    }
    else
    {
        return pos * 4.0f - 4.0f;
    }
}

/**
 * @brief  生成归一化波形表
 * @note   范围约为 -32767 ~ +32767
 *         只在初始化或切换波形时调用，不在自动调幅时反复调用 sinf
 */
static void DACWave_UpdateNormTable(void)
{
    uint16_t i;
    float sample;
    int32_t value;

    for (i = 0; i < WAVE_TABLE_SIZE; i++)
    {
        if (DACWave_Waveform == WAVEFORM_TRIANGLE)
        {
            sample = DACWave_GetTriangleSample(i);
        }
        else
        {
            sample = sinf(DAC_TWO_PI * (float)i / (float)WAVE_TABLE_SIZE);
        }

        value = (int32_t)(sample * (float)DAC_NORM_MAX);

        if (value > DAC_NORM_MAX)
        {
            value = DAC_NORM_MAX;
        }

        if (value < -DAC_NORM_MAX)
        {
            value = -DAC_NORM_MAX;
        }

        DACWave_NormTable[i] = (int16_t)value;
    }
}

static void DACWave_ConfigTIM6(uint32_t freq_hz)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    uint32_t dac_update_rate;
    uint32_t arr_reload;
    uint32_t prescaler_div;
    uint8_t was_running;

    dac_update_rate = freq_hz * WAVE_TABLE_SIZE;

    if (dac_update_rate == 0)
    {
        dac_update_rate = 1;
    }

    /*
     * STM32F103RCT6 常见配置：
     * SYSCLK = 72MHz
     * APB1 = 36MHz
     * TIM6 时钟 = 72MHz
     *
     * DAC 更新频率 = 输出频率 * 波表点数
     */
    arr_reload = SYSCLK_HZ / dac_update_rate;

    if (arr_reload < 2)
    {
        arr_reload = 2;
    }

    prescaler_div = 1;

    if (arr_reload > 65536UL)
    {
        prescaler_div = (arr_reload + 65535UL) / 65536UL;

        if (prescaler_div < 1)
        {
            prescaler_div = 1;
        }

        if (prescaler_div > 65536UL)
        {
            prescaler_div = 65536UL;
        }

        arr_reload = SYSCLK_HZ / (dac_update_rate * prescaler_div);

        if (arr_reload < 2)
        {
            arr_reload = 2;
        }

        if (arr_reload > 65536UL)
        {
            arr_reload = 65536UL;
        }
    }

    was_running = DACWave_Running;

    if (was_running)
    {
        TIM_Cmd(TIM6, DISABLE);
    }

    TIM_TimeBaseStructure.TIM_Period = (uint16_t)(arr_reload - 1);
    TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t)(prescaler_div - 1);
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM6, &TIM_TimeBaseStructure);

    TIM_SelectOutputTrigger(TIM6, TIM_TRGOSource_Update);
    TIM_SetCounter(TIM6, 0);
    TIM_ClearFlag(TIM6, TIM_FLAG_Update);

    if (was_running)
    {
        TIM_Cmd(TIM6, ENABLE);
    }
}

/**
 * @brief  根据当前幅度系数重新生成 DAC 输出表
 * @note   这个函数不再关闭 TIM6。
 *         这样自动调幅时不会出现明显直线段。
 *         代价是表更新瞬间可能有极轻微幅值过渡，但通常比暂停波形更好。
 */
void DACWave_UpdateTable(void)
{
    uint16_t i;
    float base_amp_code_f;
    float amp_code_f;
    int32_t amp_code;
    int32_t code;

    /*
     * 100mVrms 正弦波：
     * Vp = 0.1 * sqrt(2) = 0.141V
     * amp_code = Vp / 3.3 * 4095 ≈ 175
     *
     * PA4 原始 DAC 输出应为：
     * 1.65V ± 0.141V
     * Vpp ≈ 282mV
     */
    base_amp_code_f = US_INIT_RMS * DAC_SQRT2 / DAC_VREF * (float)DAC_MAX_CODE;
    amp_code_f = base_amp_code_f * DACWave_AmpScale;

    if (amp_code_f > (float)(DAC_MID_CODE - 1))
    {
        amp_code_f = (float)(DAC_MID_CODE - 1);
    }

    if (amp_code_f < 0.0f)
    {
        amp_code_f = 0.0f;
    }

    amp_code = (int32_t)(amp_code_f + 0.5f);

    for (i = 0; i < WAVE_TABLE_SIZE; i++)
    {
        code = (int32_t)DAC_MID_CODE;
        code += (amp_code * (int32_t)DACWave_NormTable[i]) / DAC_NORM_MAX;

        if (code < 0)
        {
            code = 0;
        }

        if (code > DAC_MAX_CODE)
        {
            code = DAC_MAX_CODE;
        }

        DACWave_Table[i] = (uint16_t)code;
    }
}

void DACWave_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    DAC_InitTypeDef DAC_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC | RCC_APB1Periph_TIM6, ENABLE);

    /*
     * STM32F103RCT6 / 高密度 F103：
     * DAC Channel1 对应 DMA2_Channel3
     */
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);

    /* PA4 -> DAC_OUT1，配置为模拟模式 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    DACWave_FreqHz = FREQ_MIN_HZ;
    DACWave_AmpScale = AMP_SCALE_INIT;
    DACWave_Waveform = WAVEFORM_SINE;
    DACWave_Running = 0;

    DACWave_UpdateNormTable();
    DACWave_UpdateTable();

    /*
     * DAC Channel1 使用 DMA2_Channel3。
     */
    DMA_DeInit(DMA2_Channel3);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&DAC->DHR12R1;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)DACWave_Table;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = WAVE_TABLE_SIZE;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA2_Channel3, &DMA_InitStructure);

    DAC_InitStructure.DAC_Trigger = DAC_Trigger_T6_TRGO;
    DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
    DAC_InitStructure.DAC_LFSRUnmask_TriangleAmplitude = DAC_LFSRUnmask_Bit0;
    DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
    DAC_Init(DAC_Channel_1, &DAC_InitStructure);

    DAC_Cmd(DAC_Channel_1, ENABLE);
    DAC_SetChannel1Data(DAC_Align_12b_R, DAC_MID_CODE);

    DACWave_ConfigTIM6(DACWave_FreqHz);
}

void DACWave_Start(void)
{
    TIM_Cmd(TIM6, DISABLE);

    DMA_Cmd(DMA2_Channel3, DISABLE);
    DMA_SetCurrDataCounter(DMA2_Channel3, WAVE_TABLE_SIZE);
    DMA_ClearFlag(DMA2_FLAG_GL3 | DMA2_FLAG_TC3 | DMA2_FLAG_HT3 | DMA2_FLAG_TE3);
    DMA_Cmd(DMA2_Channel3, ENABLE);

    DAC_DMACmd(DAC_Channel_1, ENABLE);

    TIM_SetCounter(TIM6, 0);
    TIM_ClearFlag(TIM6, TIM_FLAG_Update);
    TIM_Cmd(TIM6, ENABLE);

    DACWave_Running = 1;
}

void DACWave_Stop(void)
{
    TIM_Cmd(TIM6, DISABLE);
    DAC_DMACmd(DAC_Channel_1, DISABLE);
    DMA_Cmd(DMA2_Channel3, DISABLE);

    DAC_SetChannel1Data(DAC_Align_12b_R, DAC_MID_CODE);

    DACWave_Running = 0;
}

void DACWave_SetFreq(uint32_t freq_hz)
{
    DACWave_FreqHz = DACWave_LimitFreq(freq_hz);
    DACWave_ConfigTIM6(DACWave_FreqHz);
}

uint32_t DACWave_GetFreq(void)
{
    return DACWave_FreqHz;
}

void DACWave_SetAmplitudeScale(float scale)
{
    float new_scale;
    float diff;

    new_scale = DACWave_LimitFloat(scale, AMP_SCALE_MIN, AMP_SCALE_MAX);

    diff = new_scale - DACWave_AmpScale;
    if (diff < 0.0f)
    {
        diff = -diff;
    }

    /*
     * 幅度变化太小时不更新波表，避免自动调幅稳定后仍反复刷新。
     */
    if (diff < 0.001f)
    {
        return;
    }

    DACWave_AmpScale = new_scale;
    DACWave_UpdateTable();
}

float DACWave_GetAmplitudeScale(void)
{
    return DACWave_AmpScale;
}

void DACWave_SetWaveform(Waveform_t waveform)
{
    if (waveform > WAVEFORM_TRIANGLE)
    {
        waveform = WAVEFORM_SINE;
    }

    if (DACWave_Waveform == waveform)
    {
        return;
    }

    DACWave_Waveform = waveform;
    DACWave_UpdateNormTable();
    DACWave_UpdateTable();
}

Waveform_t DACWave_GetWaveform(void)
{
    return DACWave_Waveform;
}
