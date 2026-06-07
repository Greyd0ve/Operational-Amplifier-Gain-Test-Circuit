#include "stm32f10x.h"
#include "AutoGain.h"
#include "AppConfig.h"

static uint8_t AutoGain_Enabled = 0;
static uint8_t AutoGain_Limit = 0;

static float AutoGain_LimitFloat(float value, float min, float max)
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

void AutoGain_Init(void)
{
    AutoGain_Enabled = 0;       // 基础版默认关闭，按 KEY2 开启
    AutoGain_Limit = 0;
}

void AutoGain_Enable(uint8_t enable)
{
    AutoGain_Enabled = enable ? 1 : 0;
    AutoGain_Limit = 0;
}

uint8_t AutoGain_IsEnabled(void)
{
    return AutoGain_Enabled;
}

uint8_t AutoGain_IsLimit(void)
{
    return AutoGain_Limit;
}

float AutoGain_Update(float uo_rms, uint8_t clip, float current_amp)
{
    float err;
    float ratio;
    float new_amp;

    AutoGain_Limit = 0;

    if (!AutoGain_Enabled)
    {
        return current_amp;
    }

    if (clip)
    {
        new_amp = current_amp * 0.8f;
        new_amp = AutoGain_LimitFloat(new_amp, AMP_SCALE_MIN, AMP_SCALE_MAX);
        if ((new_amp <= AMP_SCALE_MIN) || (new_amp >= AMP_SCALE_MAX))
        {
            AutoGain_Limit = 1;
        }
        return new_amp;
    }

    if (uo_rms < 0.001f)
    {
        return current_amp;
    }

    err = (UO_TARGET_RMS - uo_rms) / UO_TARGET_RMS;
    if ((err > -AUTO_GAIN_DEADBAND) && (err < AUTO_GAIN_DEADBAND))
    {
        return current_amp;
    }

    ratio = UO_TARGET_RMS / uo_rms;
    ratio = AutoGain_LimitFloat(ratio, AUTO_GAIN_STEP_MIN, AUTO_GAIN_STEP_MAX);

    new_amp = current_amp * ratio;
    new_amp = AutoGain_LimitFloat(new_amp, AMP_SCALE_MIN, AMP_SCALE_MAX);

    if ((new_amp <= AMP_SCALE_MIN) || (new_amp >= AMP_SCALE_MAX))
    {
        AutoGain_Limit = 1;
    }

    return new_amp;
}