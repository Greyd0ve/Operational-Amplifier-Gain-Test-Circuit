#ifndef __MEASURE_H
#define __MEASURE_H

#include "stm32f10x.h"

typedef struct
{
    float ui_dc;
    float uo_dc;
    float ui_rms;
    float uo_rms;
    float gain;
    float phase_diff;
    float set_freq;
    float meas_freq;
    uint8_t gain_valid;
    uint8_t phase_valid;
    uint8_t clip;
} MeasureResult_t;

float Measure_CalcDC(const float *buf, uint16_t len);
float Measure_CalcRMS_AC(const float *buf, uint16_t len, float scale);
float Measure_CalcFreqZeroCross(const float *buf, uint16_t len, float sample_rate);
uint8_t Measure_DetectClipVoltage(const float *buf, uint16_t len);
void Measure_ResetPhaseTracking(void);
void Measure_Update(const float *ui_buf, const float *uo_buf, uint16_t len, uint32_t set_freq, MeasureResult_t *result);

#endif
