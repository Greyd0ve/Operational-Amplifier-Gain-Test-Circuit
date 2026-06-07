#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "AD.h"
#include "DAC_Wave.h"
#include "Key.h"
#include "Measure.h"
#include "AutoGain.h"
#include "AppConfig.h"
#include "App.h"

static float App_UiBuf[ADC_SAMPLE_LEN];
static float App_UoBuf[ADC_SAMPLE_LEN];
static MeasureResult_t App_Result;
static uint8_t App_DisplayPage = 0;
static uint16_t App_DisplayTick = 0;

static uint32_t App_LimitFreq(uint32_t freq)
{
    if (freq < FREQ_MIN_HZ)
    {
        return FREQ_MIN_HZ;
    }

    if (freq > FREQ_MAX_HZ)
    {
        return FREQ_MAX_HZ;
    }

    return freq;
}

static void OLED_ShowFixed(uint8_t Line, uint8_t Column, float num, uint8_t int_len, uint8_t frac_len)
{
    uint32_t int_part;
    uint32_t frac_part;
    uint32_t scale;
    uint8_t i;

    if (num < 0.0f)
    {
        OLED_ShowChar(Line, Column, '-');
        Column++;
        num = -num;
    }

    scale = 1;
    for (i = 0; i < frac_len; i++)
    {
        scale *= 10;
    }

    int_part = (uint32_t)num;
    frac_part = (uint32_t)((num - (float)int_part) * (float)scale + 0.5f);

    if (frac_part >= scale)
    {
        int_part++;
        frac_part -= scale;
    }

    OLED_ShowNum(Line, Column, int_part, int_len);
    OLED_ShowChar(Line, Column + int_len, '.');
    OLED_ShowNum(Line, Column + int_len + 1, frac_part, frac_len);
}

static void App_HandleKey(KeyEvent_t event)
{
    uint32_t freq;

    freq = DACWave_GetFreq();

    switch (event)
    {
        case KEY1_SHORT:
            freq = App_LimitFreq(freq + FREQ_STEP_HZ);
            DACWave_SetFreq(freq);
            break;

        case KEY1_LONG:
            if (freq > FREQ_MIN_HZ + FREQ_STEP_HZ)
            {
                freq -= FREQ_STEP_HZ;
            }
            else
            {
                freq = FREQ_MIN_HZ;
            }
            DACWave_SetFreq(freq);
            break;

        case KEY2_SHORT:
            AutoGain_Enable(!AutoGain_IsEnabled());
            break;

        case KEY2_LONG:
            App_DisplayPage = !App_DisplayPage;
            OLED_Clear();
            break;

        case KEY3_SHORT:
            if (DACWave_GetWaveform() == WAVEFORM_SINE)
            {
                DACWave_SetWaveform(WAVEFORM_TRIANGLE);
            }
            else
            {
                DACWave_SetWaveform(WAVEFORM_SINE);
            }
            break;

        case KEY3_LONG:
        default:
            break;
    }
}

static void App_DisplayMain(void)
{
    uint32_t ui_mv;

    OLED_Clear();

    if (DACWave_GetWaveform() == WAVEFORM_SINE)
    {
        OLED_ShowString(1, 1, "S ");
    }
    else
    {
        OLED_ShowString(1, 1, "T ");
    }

    OLED_ShowString(1, 3, "F:");
    OLED_ShowNum(1, 5, DACWave_GetFreq(), 4);

    OLED_ShowString(1, 9, " A:");
    if (AutoGain_IsEnabled())
    {
        OLED_ShowString(1, 12, "ON ");
    }
    else
    {
        OLED_ShowString(1, 12, "OFF");
    }

    ui_mv = (uint32_t)(App_Result.ui_rms * 1000.0f + 0.5f);
    OLED_ShowString(2, 1, "Ui:");
    OLED_ShowNum(2, 4, ui_mv, 4);
    OLED_ShowString(2, 8, "mV");

    OLED_ShowString(3, 1, "Uo:");
    OLED_ShowFixed(3, 4, App_Result.uo_rms, 1, 3);
    OLED_ShowString(3, 9, "V");

    OLED_ShowString(4, 1, "G:");
    if (App_Result.gain_valid)
    {
        OLED_ShowFixed(4, 3, App_Result.gain, 2, 2);
    }
    else
    {
        OLED_ShowString(4, 3, "--.--");
    }
}

static void App_DisplayDebug(void)
{
    OLED_Clear();

    OLED_ShowString(1, 1, "Amp:");
    OLED_ShowFixed(1, 5, DACWave_GetAmplitudeScale(), 2, 3);

    OLED_ShowString(2, 1, "UiD:");
    OLED_ShowFixed(2, 5, App_Result.ui_dc, 1, 3);

    OLED_ShowString(3, 1, "UoD:");
    OLED_ShowFixed(3, 5, App_Result.uo_dc, 1, 3);

    OLED_ShowString(4, 1, "Clip:");
    OLED_ShowNum(4, 6, App_Result.clip, 1);
    OLED_ShowString(4, 8, "Lim:");
    OLED_ShowNum(4, 12, AutoGain_IsLimit(), 1);
}

static void App_DisplayUpdate(void)
{
    if (App_DisplayPage == 0)
    {
        App_DisplayMain();
    }
    else
    {
        App_DisplayDebug();
    }
}

void App_Init(void)
{
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(1, 1, "Gain Tester");
    OLED_ShowString(2, 1, "DAC DMA Ready");

    Key_Init();
    AD_Init();
    DACWave_Init();
    AutoGain_Init();

    DACWave_SetFreq(FREQ_MIN_HZ);
    DACWave_SetAmplitudeScale(AMP_SCALE_INIT);
    DACWave_SetWaveform(WAVEFORM_SINE);
    DACWave_Start();
    AD_Start();

    App_Result.ui_dc = 0.0f;
    App_Result.uo_dc = 0.0f;
    App_Result.ui_rms = 0.0f;
    App_Result.uo_rms = 0.0f;
    App_Result.gain = 0.0f;
    App_Result.set_freq = (float)FREQ_MIN_HZ;
    App_Result.meas_freq = (float)FREQ_MIN_HZ;
    App_Result.gain_valid = 0;
    App_Result.clip = 0;

    Delay_ms(300);
    OLED_Clear();
}

void App_Run(void)
{
    KeyEvent_t event;
    float amp;
    float old_amp;
    float diff;

    Delay_ms(KEY_SCAN_PERIOD_MS);

    Key_Scan10ms();
    event = Key_GetEvent();

    if (event != KEY_EVENT_NONE)
    {
        App_HandleKey(event);
    }

    if (AD_IsFrameReady())
    {
        AD_GetFrame(App_UiBuf, App_UoBuf, ADC_SAMPLE_LEN);
        AD_ClearFrameReady();

        Measure_Update(App_UiBuf, App_UoBuf, ADC_SAMPLE_LEN, DACWave_GetFreq(), &App_Result);

        if (AutoGain_IsEnabled())
        {
            old_amp = DACWave_GetAmplitudeScale();
            amp = AutoGain_Update(App_Result.uo_rms, App_Result.clip, old_amp);

            diff = amp - old_amp;
            if (diff < 0.0f)
            {
                diff = -diff;
            }

            /*
             * 只有幅度变化明显时才更新 DAC 表。
             * 避免自动调幅稳定后仍反复调用 DACWave_SetAmplitudeScale。
             */
            if (diff > 0.001f)
            {
                DACWave_SetAmplitudeScale(amp);
            }
        }
    }

    App_DisplayTick += KEY_SCAN_PERIOD_MS;
    if (App_DisplayTick >= DISPLAY_UPDATE_MS)
    {
        App_DisplayTick = 0;
        App_DisplayUpdate();
    }
}
