#include "stm32f10x.h"
#include "DAC_Wave.h"
#include "AppConfig.h"
#include <math.h>

#define DACWAVE_SAMPLE_COUNT_MIN        ((uint16_t)WAVE_TABLE_SIZE_MIN)
#define DACWAVE_SAMPLE_COUNT_MAX        ((uint16_t)WAVE_TABLE_SIZE_MAX)
#define DACWAVE_SHAPE_SCALE             32767.0
#define DACWAVE_PI                      3.14159265358979323846
#define DACWAVE_SCALE_SEARCH_ITERS      ((uint8_t)24u)
#define DACWAVE_FREQUENCY_MAX_ERROR     0.0005

static uint16_t DACWave_Table[DACWAVE_SAMPLE_COUNT_MAX];
static int16_t DACWave_ShapeTable[DACWAVE_SAMPLE_COUNT_MAX];

static uint32_t DACWave_FreqHz = FREQ_MIN_HZ;
static float DACWave_AmpScale = AMP_SCALE_INIT;
static Waveform_t DACWave_Waveform = WAVEFORM_SINE;
static uint8_t DACWave_Running = 0;
static uint16_t DACWave_SampleCount = 0;
static uint16_t DACWave_TimerPeriod = 0;
static uint32_t DACWave_TimerClockHz = SYSCLK_HZ;

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

static double DACWave_AbsDouble(double value)
{
    return (value < 0.0) ? -value : value;
}

static uint16_t DACWave_ToDacCode(double value)
{
    if (value <= 0.0)
    {
        return 0u;
    }

    if (value >= (double)DAC_MAX_CODE)
    {
        return (uint16_t)DAC_MAX_CODE;
    }

    return (uint16_t)(value + 0.5);
}

static uint16_t DACWave_GetMidCode(void)
{
    return DACWave_ToDacCode((double)DAC_MID_CODE);
}

static uint32_t DACWave_GetTIM6ClockHz(void)
{
    RCC_ClocksTypeDef clocks;

    RCC_GetClocksFreq(&clocks);

    if (clocks.PCLK1_Frequency == 0u)
    {
        return SYSCLK_HZ;
    }

    if (clocks.PCLK1_Frequency == clocks.HCLK_Frequency)
    {
        return clocks.PCLK1_Frequency;
    }

    return clocks.PCLK1_Frequency * 2u;
}

static double DACWave_RawSample(Waveform_t waveform, uint16_t index, uint16_t sample_count)
{
    double phase;

    phase = (double)index / (double)sample_count;

    if (waveform == WAVEFORM_SINE)
    {
        return sin(2.0 * DACWAVE_PI * phase);
    }

    if (phase < 0.25)
    {
        return 4.0 * phase;
    }

    if (phase < 0.75)
    {
        return 2.0 - 4.0 * phase;
    }

    return -4.0 + 4.0 * phase;
}

static double DACWave_ShapeSample(uint16_t index)
{
    return (double)DACWave_ShapeTable[index] / DACWAVE_SHAPE_SCALE;
}

static void DACWave_BuildShapeTable(void)
{
    uint16_t i;
    double raw;
    double shape;

    for (i = 0; i < DACWave_SampleCount; i++)
    {
        raw = DACWave_RawSample(DACWave_Waveform, i, DACWave_SampleCount);
        shape = raw * DACWAVE_SHAPE_SCALE;

        if (shape >= DACWAVE_SHAPE_SCALE)
        {
            DACWave_ShapeTable[i] = (int16_t)DACWAVE_SHAPE_SCALE;
        }
        else if (shape <= -DACWAVE_SHAPE_SCALE)
        {
            DACWave_ShapeTable[i] = (int16_t)(-DACWAVE_SHAPE_SCALE);
        }
        else if (shape >= 0.0)
        {
            DACWave_ShapeTable[i] = (int16_t)(shape + 0.5);
        }
        else
        {
            DACWave_ShapeTable[i] = (int16_t)(shape - 0.5);
        }
    }
}

static double DACWave_CalcShapeRms(void)
{
    uint16_t i;
    double sample;
    double sum;

    sum = 0.0;

    for (i = 0; i < DACWave_SampleCount; i++)
    {
        sample = DACWave_ShapeSample(i);
        sum += sample * sample;
    }

    return sqrt(sum / (double)DACWave_SampleCount);
}

static double DACWave_CalcRoundedRms(double scale)
{
    uint16_t i;
    uint16_t code;
    double sample;
    double ac_code;
    double sum;

    sum = 0.0;

    for (i = 0; i < DACWave_SampleCount; i++)
    {
        sample = DACWave_ShapeSample(i);
        code = DACWave_ToDacCode((double)DAC_MID_CODE + scale * sample);
        ac_code = (double)code - (double)DAC_MID_CODE;
        sum += ac_code * ac_code;
    }

    return sqrt(sum / (double)DACWave_SampleCount);
}

static double DACWave_GetTargetRmsCode(void)
{
    double target_rms_code;

    target_rms_code = ((double)US_INIT_RMS * (double)DACWave_AmpScale *
                       (double)DAC_MAX_CODE) / (double)DAC_VREF;

    if (DACWave_Waveform == WAVEFORM_TRIANGLE)
    {
        target_rms_code *= (double)DAC_TRI_CAL_SCALE;
    }
    else
    {
        target_rms_code *= (double)DAC_SINE_CAL_SCALE;
    }

    return target_rms_code;
}

static double DACWave_SelectScale(double nominal_scale, double target_rms_code)
{
    uint8_t i;
    double low;
    double high;
    double mid;
    double low_error;
    double high_error;
    double nominal_error;

    low = nominal_scale * 0.96;
    high = nominal_scale * 1.04;

    for (i = 0; i < DACWAVE_SCALE_SEARCH_ITERS; i++)
    {
        mid = (low + high) * 0.5;

        if (DACWave_CalcRoundedRms(mid) < target_rms_code)
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }

    low_error = DACWave_AbsDouble(DACWave_CalcRoundedRms(low) - target_rms_code);
    high_error = DACWave_AbsDouble(DACWave_CalcRoundedRms(high) - target_rms_code);
    nominal_error = DACWave_AbsDouble(DACWave_CalcRoundedRms(nominal_scale) - target_rms_code);

    if ((nominal_error <= low_error) && (nominal_error <= high_error))
    {
        return nominal_scale;
    }

    return (low_error <= high_error) ? low : high;
}

void DACWave_UpdateTable(void)
{
    uint16_t i;
    double shape_rms;
    double target_rms_code;
    double nominal_scale;
    double scale;
    double sample;

    if (DACWave_SampleCount == 0u)
    {
        return;
    }

    target_rms_code = DACWave_GetTargetRmsCode();
    shape_rms = DACWave_CalcShapeRms();

    if (shape_rms <= 0.0)
    {
        scale = 0.0;
    }
    else
    {
        nominal_scale = target_rms_code / shape_rms;
        scale = DACWave_SelectScale(nominal_scale, target_rms_code);
    }

    for (i = 0; i < DACWave_SampleCount; i++)
    {
        sample = DACWave_ShapeSample(i);
        DACWave_Table[i] = DACWave_ToDacCode((double)DAC_MID_CODE + scale * sample);
    }
}

static void DACWave_SelectTiming(uint32_t freq_hz, uint16_t *sample_count, uint16_t *timer_period)
{
    uint16_t count;
    uint32_t period;
    uint16_t best_count;
    uint16_t best_period;
    uint16_t fallback_count;
    uint16_t fallback_period;
    double target_total_ticks;
    double actual_frequency;
    double error;
    double best_error;
    double fallback_error;

    target_total_ticks = (double)DACWave_TimerClockHz / (double)freq_hz;
    best_count = 0u;
    best_period = 0u;
    best_error = 1.0;
    fallback_count = DACWAVE_SAMPLE_COUNT_MIN;
    fallback_period = (uint16_t)(target_total_ticks / (double)DACWAVE_SAMPLE_COUNT_MIN + 0.5);
    fallback_error = 1.0;

    for (count = DACWAVE_SAMPLE_COUNT_MIN; count <= DACWAVE_SAMPLE_COUNT_MAX; count++)
    {
        period = (uint32_t)(target_total_ticks / (double)count + 0.5);

        if ((period == 0u) || (period > 0xFFFFu))
        {
            continue;
        }

        actual_frequency = (double)DACWave_TimerClockHz / ((double)period * (double)count);
        error = DACWave_AbsDouble(actual_frequency - (double)freq_hz) / (double)freq_hz;

        if ((error < fallback_error) ||
            ((DACWave_AbsDouble(error - fallback_error) < 0.000000001) &&
             (count > fallback_count)))
        {
            fallback_error = error;
            fallback_count = count;
            fallback_period = (uint16_t)period;
        }

        if (error > DACWAVE_FREQUENCY_MAX_ERROR)
        {
            continue;
        }

        if ((best_count == 0u) ||
            (count > best_count) ||
            ((count == best_count) && (error < best_error)))
        {
            best_error = error;
            best_count = count;
            best_period = (uint16_t)period;
        }
    }

    if (best_count == 0u)
    {
        best_count = fallback_count;
        best_period = fallback_period;
    }

    *sample_count = best_count;
    *timer_period = best_period;
}

static void DACWave_ConfigTIM6(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    TIM_TimeBaseStructInit(&TIM_TimeBaseStructure);
    TIM_TimeBaseStructure.TIM_Period = (uint16_t)(DACWave_TimerPeriod - 1u);
    TIM_TimeBaseStructure.TIM_Prescaler = 0u;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM6, &TIM_TimeBaseStructure);

    TIM_SelectOutputTrigger(TIM6, TIM_TRGOSource_Update);
    TIM_SetCounter(TIM6, 0u);
    TIM_ClearFlag(TIM6, TIM_FLAG_Update);
}

static void DACWave_ConfigDMA(void)
{
    DMA_InitTypeDef DMA_InitStructure;

    DMA_DeInit(DMA2_Channel3);
    DMA_StructInit(&DMA_InitStructure);
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)&DAC->DHR12R1;
    DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t)DACWave_Table;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = DACWave_SampleCount;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA2_Channel3, &DMA_InitStructure);
}

static void DACWave_StopTransfer(void)
{
    TIM_Cmd(TIM6, DISABLE);
    DAC_DMACmd(DAC_Channel_1, DISABLE);
    DMA_Cmd(DMA2_Channel3, DISABLE);
    DMA_ClearFlag(DMA2_FLAG_GL3 | DMA2_FLAG_TC3 | DMA2_FLAG_HT3 | DMA2_FLAG_TE3);
}

static void DACWave_StartTransfer(void)
{
    TIM_Cmd(TIM6, DISABLE);

    DMA_Cmd(DMA2_Channel3, DISABLE);
    DMA_SetCurrDataCounter(DMA2_Channel3, DACWave_SampleCount);
    DMA_ClearFlag(DMA2_FLAG_GL3 | DMA2_FLAG_TC3 | DMA2_FLAG_HT3 | DMA2_FLAG_TE3);
    DMA_Cmd(DMA2_Channel3, ENABLE);

    DAC_SetChannel1Data(DAC_Align_12b_R, DACWave_Table[0]);
    DAC_DMACmd(DAC_Channel_1, ENABLE);

    TIM_SetCounter(TIM6, 0u);
    TIM_ClearFlag(TIM6, TIM_FLAG_Update);
    TIM_Cmd(TIM6, ENABLE);
}

static void DACWave_ApplyTiming(void)
{
    uint8_t was_running;

    was_running = DACWave_Running;
    if (was_running)
    {
        DACWave_StopTransfer();
        DACWave_Running = 0;
    }

    DACWave_SelectTiming(DACWave_FreqHz, &DACWave_SampleCount, &DACWave_TimerPeriod);
    DACWave_BuildShapeTable();
    DACWave_UpdateTable();
    DACWave_ConfigTIM6();
    DACWave_ConfigDMA();

    if (was_running)
    {
        DACWave_StartTransfer();
        DACWave_Running = 1;
    }
}

void DACWave_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    DAC_InitTypeDef DAC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC | RCC_APB1Periph_TIM6, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA2, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    DACWave_TimerClockHz = DACWave_GetTIM6ClockHz();
    DACWave_FreqHz = FREQ_MIN_HZ;
    DACWave_AmpScale = AMP_SCALE_INIT;
    DACWave_Waveform = WAVEFORM_SINE;
    DACWave_Running = 0;

    DAC_StructInit(&DAC_InitStructure);
    DAC_InitStructure.DAC_Trigger = DAC_Trigger_T6_TRGO;
    DAC_InitStructure.DAC_WaveGeneration = DAC_WaveGeneration_None;
    DAC_InitStructure.DAC_OutputBuffer = DAC_OutputBuffer_Enable;
    DAC_Init(DAC_Channel_1, &DAC_InitStructure);

    DAC_Cmd(DAC_Channel_1, ENABLE);
    DAC_SetChannel1Data(DAC_Align_12b_R, DACWave_GetMidCode());

    DACWave_ApplyTiming();
}

void DACWave_Start(void)
{
    DACWave_StartTransfer();
    DACWave_Running = 1;
}

void DACWave_Stop(void)
{
    DACWave_StopTransfer();
    DAC_SetChannel1Data(DAC_Align_12b_R, DACWave_GetMidCode());
    DACWave_Running = 0;
}

void DACWave_SetFreq(uint32_t freq_hz)
{
    freq_hz = DACWave_LimitFreq(freq_hz);

    if (DACWave_FreqHz == freq_hz)
    {
        return;
    }

    DACWave_FreqHz = freq_hz;
    DACWave_ApplyTiming();
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
    DACWave_BuildShapeTable();
    DACWave_UpdateTable();
}

Waveform_t DACWave_GetWaveform(void)
{
    return DACWave_Waveform;
}
