#ifndef __AUTOGAIN_H
#define __AUTOGAIN_H

#include "stm32f10x.h"

void AutoGain_Init(void);
void AutoGain_Enable(uint8_t enable);
uint8_t AutoGain_IsEnabled(void);
float AutoGain_Update(float uo_rms, uint8_t clip, float current_amp);
uint8_t AutoGain_IsLimit(void);

#endif