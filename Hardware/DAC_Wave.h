#ifndef __DAC_WAVE_H
#define __DAC_WAVE_H

#include "stm32f10x.h"

typedef enum
{
    WAVEFORM_SINE = 0,
    WAVEFORM_TRIANGLE
} Waveform_t;

void DACWave_Init(void);
void DACWave_Start(void);
void DACWave_Stop(void);
void DACWave_SetFreq(uint32_t freq_hz);
uint32_t DACWave_GetFreq(void);
void DACWave_SetAmplitudeScale(float scale);
float DACWave_GetAmplitudeScale(void);
void DACWave_SetWaveform(Waveform_t waveform);
Waveform_t DACWave_GetWaveform(void);
void DACWave_UpdateTable(void);

#endif