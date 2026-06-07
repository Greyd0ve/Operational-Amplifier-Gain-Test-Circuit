#ifndef __SIGNALGEN_H
#define __SIGNALGEN_H

#include "stm32f10x.h"

typedef enum
{
	WAVEFORM_SINE = 0,
	WAVEFORM_TRIANGLE
} Waveform_t;

void SignalGen_Init(void);
void SignalGen_Start(void);
void SignalGen_Stop(void);
void SignalGen_SetFreq(uint32_t freq_hz);
uint32_t SignalGen_GetFreq(void);
void SignalGen_SetAmplitudeScale(float scale);
float SignalGen_GetAmplitudeScale(void);
void SignalGen_SetWaveform(Waveform_t waveform);
Waveform_t SignalGen_GetWaveform(void);
void SignalGen_TIM2_IRQHandler(void);

#endif
