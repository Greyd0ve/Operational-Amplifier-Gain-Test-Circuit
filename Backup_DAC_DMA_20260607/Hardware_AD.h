#ifndef __AD_H
#define __AD_H

#include "stm32f10x.h"

void AD_Init(void);
uint16_t AD_GetValue(uint8_t ADC_Channel);
void AD_GetFrame(float *ui_buf, float *uo_buf, uint16_t len);

#endif
