#include "stm32f10x.h"                  // Device header
#include "SignalGen.h"
#include "MCP4725.h"
#include "AppConfig.h"

#define PHASE_FULL_SCALE                4294967296ULL

static const int16_t SignalGen_SineTable[WAVE_TABLE_SIZE] =
{
	    0,    50,   100,   151,   201,   251,   300,   350,
	  399,   449,   497,   546,   594,   642,   690,   737,
	  783,   830,   875,   920,   965,  1009,  1052,  1095,
	 1137,  1179,  1219,  1259,  1299,  1337,  1375,  1411,
	 1447,  1483,  1517,  1550,  1582,  1614,  1644,  1674,
	 1702,  1729,  1756,  1781,  1805,  1828,  1850,  1871,
	 1891,  1910,  1927,  1944,  1959,  1973,  1986,  1997,
	 2008,  2017,  2025,  2032,  2037,  2041,  2045,  2046,
	 2047,  2046,  2045,  2041,  2037,  2032,  2025,  2017,
	 2008,  1997,  1986,  1973,  1959,  1944,  1927,  1910,
	 1891,  1871,  1850,  1828,  1805,  1781,  1756,  1729,
	 1702,  1674,  1644,  1614,  1582,  1550,  1517,  1483,
	 1447,  1411,  1375,  1337,  1299,  1259,  1219,  1179,
	 1137,  1095,  1052,  1009,   965,   920,   875,   830,
	  783,   737,   690,   642,   594,   546,   497,   449,
	  399,   350,   300,   251,   201,   151,   100,    50,
	    0,   -50,  -100,  -151,  -201,  -251,  -300,  -350,
	 -399,  -449,  -497,  -546,  -594,  -642,  -690,  -737,
	 -783,  -830,  -875,  -920,  -965, -1009, -1052, -1095,
	-1137, -1179, -1219, -1259, -1299, -1337, -1375, -1411,
	-1447, -1483, -1517, -1550, -1582, -1614, -1644, -1674,
	-1702, -1729, -1756, -1781, -1805, -1828, -1850, -1871,
	-1891, -1910, -1927, -1944, -1959, -1973, -1986, -1997,
	-2008, -2017, -2025, -2032, -2037, -2041, -2045, -2046,
	-2047, -2046, -2045, -2041, -2037, -2032, -2025, -2017,
	-2008, -1997, -1986, -1973, -1959, -1944, -1927, -1910,
	-1891, -1871, -1850, -1828, -1805, -1781, -1756, -1729,
	-1702, -1674, -1644, -1614, -1582, -1550, -1517, -1483,
	-1447, -1411, -1375, -1337, -1299, -1259, -1219, -1179,
	-1137, -1095, -1052, -1009,  -965,  -920,  -875,  -830,
	 -783,  -737,  -690,  -642,  -594,  -546,  -497,  -449,
	 -399,  -350,  -300,  -251,  -201,  -151,  -100,   -50
};

static volatile uint32_t SignalGen_PhaseAcc;
static volatile uint32_t SignalGen_PhaseStep;
static volatile uint16_t SignalGen_AmpPeakCode;
static volatile uint8_t SignalGen_Running;
static uint32_t SignalGen_FreqHz;
static float SignalGen_AmpScale;
static Waveform_t SignalGen_Waveform;

static float SignalGen_ClampFloat(float Value, float Min, float Max)
{
	if (Value < Min) return Min;
	if (Value > Max) return Max;
	return Value;
}

static int16_t SignalGen_GetTriangle(uint8_t Index)
{
	if (Index < 64)
	{
		return (int16_t)(Index * 2047 / 64);
	}
	else if (Index < 192)
	{
		return (int16_t)(2047 - (int32_t)(Index - 64) * 4094 / 128);
	}
	else
	{
		return (int16_t)(-2047 + (int32_t)(Index - 192) * 2047 / 64);
	}
}

void SignalGen_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	
	TIM_InternalClockConfig(TIM2);
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = (SYSCLK_HZ / 72 / DAC_UPDATE_RATE_HZ) - 1;
	TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
	
	TIM_ClearFlag(TIM2, TIM_FLAG_Update);
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStructure);
	
	SignalGen_Waveform = WAVEFORM_SINE;
	SignalGen_SetFreq(FREQ_MIN_HZ);
	SignalGen_SetAmplitudeScale(AMP_SCALE_INIT);
	SignalGen_Running = 0;
	TIM_Cmd(TIM2, DISABLE);
}

void SignalGen_Start(void)
{
	SignalGen_PhaseAcc = 0;
	SignalGen_Running = 1;
	TIM_SetCounter(TIM2, 0);
	TIM_Cmd(TIM2, ENABLE);
}

void SignalGen_Stop(void)
{
	SignalGen_Running = 0;
	TIM_Cmd(TIM2, DISABLE);
	MCP4725_WriteFast(DAC_MID_CODE);
}

void SignalGen_SetFreq(uint32_t freq_hz)
{
	uint64_t Temp;
	
	if (freq_hz < FREQ_MIN_HZ) freq_hz = FREQ_MIN_HZ;
	if (freq_hz > FREQ_MAX_HZ) freq_hz = FREQ_MAX_HZ;
	
	SignalGen_FreqHz = freq_hz;
	Temp = (uint64_t)freq_hz * PHASE_FULL_SCALE;
	SignalGen_PhaseStep = (uint32_t)(Temp / DAC_UPDATE_RATE_HZ);
}

uint32_t SignalGen_GetFreq(void)
{
	return SignalGen_FreqHz;
}

void SignalGen_SetAmplitudeScale(float scale)
{
	float Vrms;
	float Vpeak;
	uint16_t PeakCode;
	
	scale = SignalGen_ClampFloat(scale, AMP_SCALE_MIN, AMP_SCALE_MAX);
	SignalGen_AmpScale = scale;
	
	Vrms = US_INIT_RMS * scale;
	Vpeak = Vrms * 1.41421356f;
	PeakCode = (uint16_t)(Vpeak / ADC_REF_VOLT * DAC_MAX_CODE + 0.5f);
	if (PeakCode > DAC_MID_CODE)
	{
		PeakCode = DAC_MID_CODE;
	}
	SignalGen_AmpPeakCode = PeakCode;
}

float SignalGen_GetAmplitudeScale(void)
{
	return SignalGen_AmpScale;
}

void SignalGen_SetWaveform(Waveform_t waveform)
{
	SignalGen_Waveform = waveform;
}

Waveform_t SignalGen_GetWaveform(void)
{
	return SignalGen_Waveform;
}

void SignalGen_TIM2_IRQHandler(void)
{
	uint8_t Index;
	int16_t Wave;
	int32_t Code;
	
	if (SignalGen_Running == 0)
	{
		return;
	}
	
	SignalGen_PhaseAcc += SignalGen_PhaseStep;
	Index = SignalGen_PhaseAcc >> 24;
	
	if (SignalGen_Waveform == WAVEFORM_TRIANGLE)
	{
		Wave = SignalGen_GetTriangle(Index);
	}
	else
	{
		Wave = SignalGen_SineTable[Index];
	}
	
	Code = DAC_MID_CODE + (int32_t)Wave * SignalGen_AmpPeakCode / 2047;
	if (Code < 0) Code = 0;
	if (Code > DAC_MAX_CODE) Code = DAC_MAX_CODE;
	
	MCP4725_WriteFast((uint16_t)Code);
}
