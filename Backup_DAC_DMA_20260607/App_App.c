#include "stm32f10x.h"                  // Device header
#include "App.h"
#include "AppConfig.h"
#include "Delay.h"
#include "OLED.h"
#include "AD.h"
#include "Key.h"
#include "MCP4725.h"
#include "SignalGen.h"
#include "Measure.h"
#include "AutoGain.h"

static float App_UiBuf[ADC_SAMPLE_LEN];
static float App_UoBuf[ADC_SAMPLE_LEN];
static MeasureResult_t App_Result;
static uint8_t App_DisplayPage;
static uint8_t App_HasMeasure;

static void App_ClearLine(uint8_t Line)
{
	OLED_ShowString(Line, 1, "                ");
}

static uint32_t App_Pow10(uint8_t n)
{
	uint32_t Result = 1;
	while (n --)
	{
		Result *= 10;
	}
	return Result;
}

static void OLED_ShowFixed(uint8_t Line, uint8_t Column, float num, uint8_t int_len, uint8_t frac_len)
{
	uint32_t Scale;
	uint32_t IntPart;
	uint32_t FracPart;
	
	if (num < 0)
	{
		OLED_ShowChar(Line, Column, '-');
		Column ++;
		num = -num;
	}
	
	Scale = App_Pow10(frac_len);
	IntPart = (uint32_t)num;
	FracPart = (uint32_t)((num - IntPart) * Scale + 0.5f);
	
	if (FracPart >= Scale)
	{
		IntPart ++;
		FracPart -= Scale;
	}
	
	OLED_ShowNum(Line, Column, IntPart, int_len);
	OLED_ShowChar(Line, Column + int_len, '.');
	OLED_ShowNum(Line, Column + int_len + 1, FracPart, frac_len);
}

static void App_ShowMainPage(void)
{
	uint32_t Ui_mV;
	
	App_ClearLine(1);
	App_ClearLine(2);
	App_ClearLine(3);
	App_ClearLine(4);
	
	OLED_ShowString(1, 1, "F:");
	OLED_ShowNum(1, 3, SignalGen_GetFreq(), 4);
	OLED_ShowString(1, 7, "Hz A:");
	if (AutoGain_IsEnabled())
	{
		OLED_ShowString(1, 12, "ON ");
	}
	else
	{
		OLED_ShowString(1, 12, "OFF");
	}
	
	Ui_mV = (uint32_t)(App_Result.ui_rms * 1000.0f + 0.5f);
	if (Ui_mV > 9999)
	{
		Ui_mV = 9999;
	}
	OLED_ShowString(2, 1, "Ui:");
	OLED_ShowNum(2, 4, Ui_mV, 4);
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
		OLED_ShowString(4, 3, "----");
	}
}

static void App_ShowDebugPage(void)
{
	uint32_t Err;
	
	App_ClearLine(1);
	App_ClearLine(2);
	App_ClearLine(3);
	App_ClearLine(4);
	
	OLED_ShowString(1, 1, "Amp:");
	OLED_ShowFixed(1, 5, SignalGen_GetAmplitudeScale(), 1, 3);
	
	OLED_ShowString(2, 1, "UiD:");
	OLED_ShowFixed(2, 5, App_Result.ui_dc, 1, 3);
	
	OLED_ShowString(3, 1, "UoD:");
	OLED_ShowFixed(3, 5, App_Result.uo_dc, 1, 3);
	
	Err = MCP4725_GetErrorCount();
	if (Err > 9999)
	{
		Err = 9999;
	}
	OLED_ShowString(4, 1, "Err:");
	OLED_ShowNum(4, 5, Err, 4);
}

static void App_Display(void)
{
	if (App_DisplayPage == 0)
	{
		App_ShowMainPage();
	}
	else
	{
		App_ShowDebugPage();
	}
}

static void App_HandleKey(void)
{
	KeyNum_t KeyNum;
	uint32_t Freq;
	
	KeyNum = Key_GetNum();
	Freq = SignalGen_GetFreq();
	
	if (KeyNum == KEY_UP)
	{
		if (Freq + FREQ_STEP_HZ <= FREQ_MAX_HZ)
		{
			Freq += FREQ_STEP_HZ;
		}
		else
		{
			Freq = FREQ_MAX_HZ;
		}
		SignalGen_SetFreq(Freq);
		App_Display();
	}
	else if (KeyNum == KEY_DOWN)
	{
		if (Freq >= FREQ_MIN_HZ + FREQ_STEP_HZ)
		{
			Freq -= FREQ_STEP_HZ;
		}
		else
		{
			Freq = FREQ_MIN_HZ;
		}
		SignalGen_SetFreq(Freq);
		App_Display();
	}
	else if (KeyNum == KEY_AUTO)
	{
		AutoGain_Enable(!AutoGain_IsEnabled());
		App_Display();
	}
	else if (KeyNum == KEY_MODE)
	{
		App_DisplayPage = !App_DisplayPage;
		OLED_Clear();
		App_Display();
	}
}

void App_Init(void)
{
	OLED_Init();
	OLED_Clear();
	Key_Init();
	AD_Init();
	MCP4725_Init();
	SignalGen_Init();
	AutoGain_Init();
	
	SignalGen_SetFreq(FREQ_MIN_HZ);
	SignalGen_SetAmplitudeScale(AMP_SCALE_INIT);
	SignalGen_SetWaveform(WAVEFORM_SINE);
	SignalGen_Start();
	
	App_Result.ui_dc = ADC_BIAS_VOLT;
	App_Result.uo_dc = ADC_BIAS_VOLT;
	App_Result.ui_rms = 0;
	App_Result.uo_rms = 0;
	App_Result.gain = 0;
	App_Result.set_freq = FREQ_MIN_HZ;
	App_Result.meas_freq = FREQ_MIN_HZ;
	App_Result.gain_valid = 0;
	App_Result.clip = 0;
	App_DisplayPage = 0;
	App_HasMeasure = 0;
	
	App_Display();
}

void App_Run(void)
{
	static uint16_t MeasureTick = 0;
	static uint16_t DisplayTick = 0;
	float Amp;
	
	App_HandleKey();
	
	Delay_ms(KEY_SCAN_PERIOD_MS);
	MeasureTick += KEY_SCAN_PERIOD_MS;
	DisplayTick += KEY_SCAN_PERIOD_MS;
	
	if (MeasureTick >= MEASURE_UPDATE_MS)
	{
		MeasureTick = 0;
		
		/* 采样期间不刷新OLED，避免I2C显示操作造成采样间隔严重不均匀。 */
		AD_GetFrame(App_UiBuf, App_UoBuf, ADC_SAMPLE_LEN);
		Measure_Update(App_UiBuf, App_UoBuf, ADC_SAMPLE_LEN, SignalGen_GetFreq(), &App_Result);
		App_HasMeasure = 1;
		
		if (AutoGain_IsEnabled())
		{
			Amp = AutoGain_Update(App_Result.uo_rms, App_Result.clip, SignalGen_GetAmplitudeScale());
			SignalGen_SetAmplitudeScale(Amp);
		}
	}
	
	if (DisplayTick >= DISPLAY_UPDATE_MS)
	{
		DisplayTick = 0;
		if (App_HasMeasure)
		{
			App_Display();
		}
	}
}
