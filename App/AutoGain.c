#include "stm32f10x.h"
#include "AutoGain.h"
#include "AppConfig.h"

#define AUTO_GAIN_TARGET_TOLERANCE_RMS     0.020f
#define AUTO_GAIN_MIN_VALID_UO_RMS         0.020f
#define AUTO_GAIN_COARSE_ERROR_RMS         0.060f
#define AUTO_GAIN_COARSE_STEP_SCALE        0.080f
#define AUTO_GAIN_FINE_STEP_SCALE          0.030f

static uint8_t AutoGain_Enabled = 0;
static uint8_t AutoGain_Limit = 0;
static float AutoGain_FilteredUo = 0.0f;
static uint8_t AutoGain_FilterReady = 0;

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

void AutoGain_ResetTracking(void)
{
    AutoGain_FilteredUo = 0.0f;
    AutoGain_FilterReady = 0;
}

static float AutoGain_LimitStep(float current_amp, float target_amp, float max_step)
{
    if (target_amp > current_amp)
    {
        if ((target_amp - current_amp) > max_step)
        {
            target_amp = current_amp + max_step;
        }
    }
    else
    {
        if ((current_amp - target_amp) > max_step)
        {
            target_amp = current_amp - max_step;
        }
    }

    return AutoGain_LimitFloat(target_amp, AMP_SCALE_MIN, AMP_SCALE_MAX);
}

void AutoGain_Init(void)
{
    AutoGain_Limit = 0;
    AutoGain_Enabled = 0;
    AutoGain_ResetTracking();
}

void AutoGain_Enable(uint8_t enable)
{
    AutoGain_Enabled = enable ? 1 : 0;
    AutoGain_Limit = 0;
    AutoGain_ResetTracking();
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
    float error;
    float max_step;
    float ratio;
    float desired_amp;
    float new_amp;
    float low_limit;
    float high_limit;

    AutoGain_Limit = 0;

    if (!AutoGain_Enabled)
    {
        return current_amp;
    }

    if (uo_rms < 0.0f)
    {
        uo_rms = 0.0f;
    }

    if (!AutoGain_FilterReady)
    {
        AutoGain_FilteredUo = uo_rms;
        AutoGain_FilterReady = 1;
    }
    else
    {
        AutoGain_FilteredUo = (AutoGain_FilteredUo * 3.0f + uo_rms) * 0.25f;
    }

    low_limit = UO_TARGET_RMS - AUTO_GAIN_TARGET_TOLERANCE_RMS;
    high_limit = UO_TARGET_RMS + AUTO_GAIN_TARGET_TOLERANCE_RMS;

    if (uo_rms < low_limit)
    {
        error = UO_TARGET_RMS - uo_rms;
        max_step = (error > AUTO_GAIN_COARSE_ERROR_RMS) ?
                   AUTO_GAIN_COARSE_STEP_SCALE :
                   AUTO_GAIN_FINE_STEP_SCALE;

        if (uo_rms < AUTO_GAIN_MIN_VALID_UO_RMS)
        {
            desired_amp = current_amp + max_step;
        }
        else
        {
            ratio = UO_TARGET_RMS / uo_rms;
            ratio = AutoGain_LimitFloat(ratio, 1.0f, AUTO_GAIN_STEP_MAX);
            desired_amp = current_amp * ratio;
        }

        if (desired_amp <= current_amp)
        {
            desired_amp = current_amp + max_step;
        }
    }
    else if (uo_rms > high_limit)
    {
        error = uo_rms - UO_TARGET_RMS;
        max_step = (error > AUTO_GAIN_COARSE_ERROR_RMS) ?
                   AUTO_GAIN_COARSE_STEP_SCALE :
                   AUTO_GAIN_FINE_STEP_SCALE;

        if (clip)
        {
            desired_amp = current_amp * AUTO_GAIN_CLIP_BACKOFF;
        }
        else
        {
            ratio = UO_TARGET_RMS / uo_rms;
            ratio = AutoGain_LimitFloat(ratio, AUTO_GAIN_STEP_MIN, 1.0f);
            desired_amp = current_amp * ratio;
        }
    }
    else
    {
        return current_amp;
    }

    new_amp = AutoGain_LimitStep(current_amp, desired_amp, max_step);
    new_amp = AutoGain_LimitFloat(new_amp, AMP_SCALE_MIN, AMP_SCALE_MAX);

    if ((new_amp <= AMP_SCALE_MIN) || (new_amp >= AMP_SCALE_MAX))
    {
        AutoGain_Limit = 1;
    }

    return new_amp;
}
