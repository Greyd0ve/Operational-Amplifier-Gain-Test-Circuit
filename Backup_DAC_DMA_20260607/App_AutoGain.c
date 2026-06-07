#include "stm32f10x.h"                  // Device header
#include "AutoGain.h"
#include "AppConfig.h"

static uint8_t AutoGain_Enabled;

static float AutoGain_Clamp(float Value, float Min, float Max)
{
	if (Value < Min) return Min;
	if (Value > Max) return Max;
	return Value;
}

void AutoGain_Init(void)
{
	AutoGain_Enabled = 1;
}

void AutoGain_Enable(uint8_t enable)
{
	AutoGain_Enabled = enable ? 1 : 0;
}

uint8_t AutoGain_IsEnabled(void)
{
	return AutoGain_Enabled;
}

float AutoGain_Update(float uo_rms, uint8_t clip, float current_amp)
{
	float Ratio;
	float Error;
	float NewAmp;
	
	current_amp = AutoGain_Clamp(current_amp, AMP_SCALE_MIN, AMP_SCALE_MAX);
	
	if (AutoGain_Enabled == 0)
	{
		return current_amp;
	}
	
	if (clip)
	{
		return AutoGain_Clamp(current_amp * 0.80f, AMP_SCALE_MIN, AMP_SCALE_MAX);
	}
	
	if (uo_rms < 0.001f)
	{
		return current_amp;
	}
	
	if (uo_rms > UO_TARGET_RMS)
	{
		Error = (uo_rms - UO_TARGET_RMS) / UO_TARGET_RMS;
	}
	else
	{
		Error = (UO_TARGET_RMS - uo_rms) / UO_TARGET_RMS;
	}
	
	if (Error < AUTO_GAIN_DEADBAND)
	{
		return current_amp;
	}
	
	Ratio = UO_TARGET_RMS / uo_rms;
	Ratio = AutoGain_Clamp(Ratio, AUTO_GAIN_STEP_MIN, AUTO_GAIN_STEP_MAX);
	NewAmp = current_amp * Ratio;
	
	return AutoGain_Clamp(NewAmp, AMP_SCALE_MIN, AMP_SCALE_MAX);
}
