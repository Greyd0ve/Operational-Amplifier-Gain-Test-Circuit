#include "stm32f10x.h"
#include "Measure.h"
#include "AppConfig.h"
#include <math.h>

float Measure_CalcDC(const float *buf, uint16_t len)
{
    uint16_t i;
    float sum;

    if ((buf == 0) || (len == 0))
    {
        return 0.0f;
    }

    sum = 0.0f;
    for (i = 0; i < len; i++)
    {
        sum += buf[i];
    }

    return sum / (float)len;
}

float Measure_CalcRMS_AC(const float *buf, uint16_t len, float scale)
{
    uint16_t i;
    float dc;
    float ac;
    float sum2;

    if ((buf == 0) || (len == 0))
    {
        return 0.0f;
    }

    dc = Measure_CalcDC(buf, len);
    sum2 = 0.0f;

    for (i = 0; i < len; i++)
    {
        ac = buf[i] - dc;
        sum2 += ac * ac;
    }

    return sqrtf(sum2 / (float)len) * scale;
}

float Measure_CalcFreqZeroCross(const float *buf, uint16_t len, float sample_rate)
{
    uint16_t i;
    float dc;
    float last;
    float now;
    uint16_t cross_count;
    uint16_t first_cross;
    uint16_t last_cross;

    if ((buf == 0) || (len < 4) || (sample_rate <= 0.0f))
    {
        return 0.0f;
    }

    dc = Measure_CalcDC(buf, len);
    last = buf[0] - dc;
    cross_count = 0;
    first_cross = 0;
    last_cross = 0;

    for (i = 1; i < len; i++)
    {
        now = buf[i] - dc;
        if ((last < 0.0f) && (now >= 0.0f))
        {
            if (cross_count == 0)
            {
                first_cross = i;
            }
            last_cross = i;
            cross_count++;
        }
        last = now;
    }

    if ((cross_count < 2) || (last_cross <= first_cross))
    {
        return 0.0f;
    }

    return (float)(cross_count - 1) * sample_rate / (float)(last_cross - first_cross);
}

uint8_t Measure_DetectClipVoltage(const float *buf, uint16_t len)
{
    uint16_t i;

    if ((buf == 0) || (len == 0))
    {
        return 0;
    }

    for (i = 0; i < len; i++)
    {
        if ((buf[i] < 0.05f) || (buf[i] > (ADC_REF_VOLT - 0.05f)))
        {
            return 1;
        }
    }

    return 0;
}

void Measure_Update(const float *ui_buf, const float *uo_buf, uint16_t len, uint32_t set_freq, MeasureResult_t *result)
{
    if ((ui_buf == 0) || (uo_buf == 0) || (result == 0) || (len == 0))
    {
        return;
    }

    result->ui_dc = Measure_CalcDC(ui_buf, len);
    result->uo_dc = Measure_CalcDC(uo_buf, len);
    result->ui_rms = Measure_CalcRMS_AC(ui_buf, len, UI_SCALE);
    result->uo_rms = Measure_CalcRMS_AC(uo_buf, len, UO_SCALE);
    result->set_freq = (float)set_freq;
    result->meas_freq = (float)set_freq;      // 基础版先用设定频率代替实测频率

    if (result->ui_rms < UI_MIN_RMS)
    {
        result->gain_valid = 0;
        result->gain = 0.0f;
    }
    else
    {
        result->gain_valid = 1;
        result->gain = result->uo_rms / result->ui_rms;
    }

    result->clip = 0;
    if (Measure_DetectClipVoltage(ui_buf, len) || Measure_DetectClipVoltage(uo_buf, len))
    {
        result->clip = 1;
    }
    if (result->uo_rms > UO_SAFE_MAX_RMS)
    {
        result->clip = 1;
    }
}