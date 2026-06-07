#include "stm32f10x.h"                  // Device header
#include "Measure.h"
#include "AppConfig.h"
#include <math.h>

float Measure_CalcDC(const float *buf, uint16_t len)
{
	uint16_t i;
	float Sum = 0;
	
	if (len == 0)
	{
		return 0;
	}
	
	for (i = 0; i < len; i ++)
	{
		Sum += buf[i];
	}
	return Sum / len;
}

float Measure_CalcRMS_AC(const float *buf, uint16_t len, float scale)
{
	uint16_t i;
	float DC;
	float AC;
	float Sum = 0;
	
	if (len == 0)
	{
		return 0;
	}
	
	DC = Measure_CalcDC(buf, len);
	for (i = 0; i < len; i ++)
	{
		AC = buf[i] - DC;
		Sum += AC * AC;
	}
	
	return sqrtf(Sum / len) * scale;
}

float Measure_CalcFreqZeroCross(const float *buf, uint16_t len, float sample_rate)
{
	uint16_t i;
	uint16_t First = 0;
	uint16_t Last = 0;
	uint16_t Count = 0;
	float DC;
	
	if (len < 2 || sample_rate <= 0)
	{
		return 0;
	}
	
	DC = Measure_CalcDC(buf, len);
	for (i = 1; i < len; i ++)
	{
		if (buf[i - 1] < DC && buf[i] >= DC)
		{
			if (Count == 0)
			{
				First = i;
			}
			Last = i;
			Count ++;
		}
	}
	
	if (Count < 2 || Last <= First)
	{
		return 0;
	}
	
	return (float)(Count - 1) * sample_rate / (float)(Last - First);
}

uint8_t Measure_DetectClipVoltage(const float *buf, uint16_t len)
{
	uint16_t i;
	
	for (i = 0; i < len; i ++)
	{
		if (buf[i] < 0.05f || buf[i] > (ADC_REF_VOLT - 0.05f))
		{
			return 1;
		}
	}
	return 0;
}

void Measure_Update(const float *ui_buf,
					const float *uo_buf,
					uint16_t len,
					uint32_t set_freq,
					MeasureResult_t *result)
{
	if (result == 0)
	{
		return;
	}
	
	result->ui_dc = Measure_CalcDC(ui_buf, len);
	result->uo_dc = Measure_CalcDC(uo_buf, len);
	result->ui_rms = Measure_CalcRMS_AC(ui_buf, len, UI_SCALE);
	result->uo_rms = Measure_CalcRMS_AC(uo_buf, len, UO_SCALE);
	result->set_freq = (float)set_freq;
	result->meas_freq = (float)set_freq;		// 基础版先用设定频率代替实测频率
	result->clip = Measure_DetectClipVoltage(ui_buf, len) | Measure_DetectClipVoltage(uo_buf, len);
	
	if (result->uo_rms > UO_SAFE_MAX_RMS)
	{
		result->clip = 1;
	}
	
	if (result->ui_rms < UI_MIN_RMS)
	{
		result->gain_valid = 0;
		result->gain = 0;
	}
	else
	{
		result->gain_valid = 1;
		result->gain = result->uo_rms / result->ui_rms;
	}
}
