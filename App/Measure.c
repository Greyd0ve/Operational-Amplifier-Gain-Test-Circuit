#include "stm32f10x.h"
#include "Measure.h"
#include "AppConfig.h"
#include <math.h>

#define MEASURE_MAX_CROSSINGS               32
#define MEASURE_PI                          3.14159265358979323846f
#define MEASURE_PHASE_MIN_HYST_V            ((4.0f * ADC_REF_VOLT) / ADC_MAX_CODE_F)
#define MEASURE_PHASE_FILTER_OLD_WEIGHT     0.88f
#define MEASURE_PHASE_FILTER_NEW_WEIGHT     0.12f
#define MEASURE_PHASE_OUTLIER_DEG           12.0f
#define MEASURE_PHASE_OUTLIER_ACCEPT_COUNT  5

static uint8_t Measure_PhaseFilterReady = 0;
static uint8_t Measure_PhaseOutlierCount = 0;
static float Measure_PhaseFilterSin = 0.0f;
static float Measure_PhaseFilterCos = 1.0f;
static uint32_t Measure_PhaseLastFreq = 0;

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

static float Measure_AbsFloat(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float Measure_WrapPhase(float phase)
{
    while (phase > 180.0f)
    {
        phase -= 360.0f;
    }

    while (phase <= -180.0f)
    {
        phase += 360.0f;
    }

    return phase;
}

static void Measure_ResetPhaseFilter(void)
{
    Measure_PhaseFilterReady = 0;
    Measure_PhaseOutlierCount = 0;
    Measure_PhaseFilterSin = 0.0f;
    Measure_PhaseFilterCos = 1.0f;
}

void Measure_ResetPhaseTracking(void)
{
    Measure_PhaseLastFreq = 0;
    Measure_ResetPhaseFilter();
}

static void Measure_CalcMinMax(const float *buf, uint16_t len, float *min_value, float *max_value)
{
    uint16_t i;
    float min_temp;
    float max_temp;

    min_temp = buf[0];
    max_temp = buf[0];

    for (i = 1; i < len; i++)
    {
        if (buf[i] < min_temp)
        {
            min_temp = buf[i];
        }
        if (buf[i] > max_temp)
        {
            max_temp = buf[i];
        }
    }

    *min_value = min_temp;
    *max_value = max_temp;
}

static float Measure_SelectPhaseHysteresis(float min_value, float max_value)
{
    float hysteresis;

    if (max_value <= min_value)
    {
        return MEASURE_PHASE_MIN_HYST_V;
    }

    hysteresis = (max_value - min_value) / 12.0f;
    if (hysteresis < MEASURE_PHASE_MIN_HYST_V)
    {
        hysteresis = MEASURE_PHASE_MIN_HYST_V;
    }

    return hysteresis;
}

static float Measure_NormalizeSampleDelta(float delta, float period)
{
    float half_period;

    half_period = period * 0.5f;

    while (delta > half_period)
    {
        delta -= period;
    }

    while (delta <= -half_period)
    {
        delta += period;
    }

    return delta;
}

static uint16_t Measure_CollectRisingCrossings(const float *buf,
                                               uint16_t len,
                                               float threshold,
                                               float hysteresis,
                                               float *cross,
                                               uint16_t max_cross)
{
    uint16_t i;
    uint16_t count;
    uint8_t armed;
    float below_threshold;
    float last;
    float now;
    float denom;

    if ((buf == 0) || (cross == 0) || (len < 2) || (max_cross == 0))
    {
        return 0;
    }

    below_threshold = threshold - hysteresis;
    last = buf[0];
    armed = (last < below_threshold) ? 1 : 0;
    count = 0;

    for (i = 1; i < len; i++)
    {
        now = buf[i];

        if (now < below_threshold)
        {
            armed = 1;
        }

        if ((armed != 0) &&
            (last < threshold) &&
            (now >= threshold) &&
            (now > last))
        {
            denom = now - last;
            if (denom != 0.0f)
            {
                cross[count] = (float)(i - 1) + ((threshold - last) / denom);
            }
            else
            {
                cross[count] = (float)i;
            }

            count++;
            armed = 0;

            if (count >= max_cross)
            {
                break;
            }
        }

        last = now;
    }

    return count;
}

static uint8_t Measure_FilterPhase(float raw_phase, float *phase_deg)
{
    float filtered_phase;
    float phase_delta;
    float raw_rad;
    float magnitude;

    if (phase_deg == 0)
    {
        return 0;
    }

    raw_phase = Measure_WrapPhase(raw_phase);

    if (!Measure_PhaseFilterReady)
    {
        raw_rad = raw_phase * MEASURE_PI / 180.0f;
        Measure_PhaseFilterSin = sinf(raw_rad);
        Measure_PhaseFilterCos = cosf(raw_rad);
        Measure_PhaseFilterReady = 1;
        Measure_PhaseOutlierCount = 0;
        *phase_deg = raw_phase;
        return 1;
    }

    filtered_phase = atan2f(Measure_PhaseFilterSin, Measure_PhaseFilterCos) * 180.0f / MEASURE_PI;
    phase_delta = Measure_WrapPhase(raw_phase - filtered_phase);

    if (Measure_AbsFloat(phase_delta) > MEASURE_PHASE_OUTLIER_DEG)
    {
        if (Measure_PhaseOutlierCount < MEASURE_PHASE_OUTLIER_ACCEPT_COUNT)
        {
            Measure_PhaseOutlierCount++;
        }

        if (Measure_PhaseOutlierCount < MEASURE_PHASE_OUTLIER_ACCEPT_COUNT)
        {
            *phase_deg = Measure_WrapPhase(filtered_phase);
            return 1;
        }

        raw_rad = raw_phase * MEASURE_PI / 180.0f;
        Measure_PhaseFilterSin = sinf(raw_rad);
        Measure_PhaseFilterCos = cosf(raw_rad);
        Measure_PhaseOutlierCount = 0;
        *phase_deg = raw_phase;
        return 1;
    }

    Measure_PhaseOutlierCount = 0;
    raw_rad = raw_phase * MEASURE_PI / 180.0f;
    Measure_PhaseFilterSin = Measure_PhaseFilterSin * MEASURE_PHASE_FILTER_OLD_WEIGHT +
                             sinf(raw_rad) * MEASURE_PHASE_FILTER_NEW_WEIGHT;
    Measure_PhaseFilterCos = Measure_PhaseFilterCos * MEASURE_PHASE_FILTER_OLD_WEIGHT +
                             cosf(raw_rad) * MEASURE_PHASE_FILTER_NEW_WEIGHT;

    magnitude = sqrtf(Measure_PhaseFilterSin * Measure_PhaseFilterSin +
                      Measure_PhaseFilterCos * Measure_PhaseFilterCos);
    if (magnitude < 0.001f)
    {
        Measure_PhaseFilterSin = sinf(raw_rad);
        Measure_PhaseFilterCos = cosf(raw_rad);
    }
    else
    {
        Measure_PhaseFilterSin /= magnitude;
        Measure_PhaseFilterCos /= magnitude;
    }

    filtered_phase = atan2f(Measure_PhaseFilterSin, Measure_PhaseFilterCos) * 180.0f / MEASURE_PI;
    *phase_deg = Measure_WrapPhase(filtered_phase);
    return 1;
}

static uint8_t Measure_CalcPhaseDiff(const float *ui_buf, const float *uo_buf, uint16_t len, uint32_t set_freq, float *phase_deg)
{
    float ui_cross[MEASURE_MAX_CROSSINGS];
    float uo_cross[MEASURE_MAX_CROSSINGS];
    uint16_t ui_count;
    uint16_t uo_count;
    uint16_t i;
    uint16_t j;
    uint16_t pair_count;
    float ui_dc;
    float uo_dc;
    float ui_min;
    float ui_max;
    float uo_min;
    float uo_max;
    float ui_hysteresis;
    float uo_hysteresis;
    float period_sum;
    float period_samples;
    float delta;
    float best_delta;
    float best_abs;
    float abs_delta;
    float phase_deg_raw;
    float phase_rad;
    float sin_sum;
    float cos_sum;

    if ((ui_buf == 0) || (uo_buf == 0) || (phase_deg == 0) || (len < 4))
    {
        return 0;
    }

    if (set_freq != Measure_PhaseLastFreq)
    {
        Measure_PhaseLastFreq = set_freq;
        Measure_ResetPhaseFilter();
    }

    ui_dc = Measure_CalcDC(ui_buf, len);
    uo_dc = Measure_CalcDC(uo_buf, len);

    Measure_CalcMinMax(ui_buf, len, &ui_min, &ui_max);
    Measure_CalcMinMax(uo_buf, len, &uo_min, &uo_max);

    ui_hysteresis = Measure_SelectPhaseHysteresis(ui_min, ui_max);
    uo_hysteresis = Measure_SelectPhaseHysteresis(uo_min, uo_max);

    ui_count = Measure_CollectRisingCrossings(ui_buf, len, ui_dc, ui_hysteresis, ui_cross, MEASURE_MAX_CROSSINGS);
    uo_count = Measure_CollectRisingCrossings(uo_buf, len, uo_dc, uo_hysteresis, uo_cross, MEASURE_MAX_CROSSINGS);

    if ((ui_count < 2) || (uo_count == 0))
    {
        return 0;
    }

    period_sum = 0.0f;
    for (i = 1; i < ui_count; i++)
    {
        period_sum += ui_cross[i] - ui_cross[i - 1];
    }

    period_samples = period_sum / (float)(ui_count - 1);
    if (period_samples < 2.0f)
    {
        return 0;
    }

    sin_sum = 0.0f;
    cos_sum = 0.0f;
    pair_count = 0;

    for (i = 0; i < ui_count; i++)
    {
        best_delta = 0.0f;
        best_abs = period_samples;

        for (j = 0; j < uo_count; j++)
        {
            delta = uo_cross[j] - ui_cross[i];
            delta = Measure_NormalizeSampleDelta(delta, period_samples);
            abs_delta = Measure_AbsFloat(delta);

            if (abs_delta < best_abs)
            {
                best_abs = abs_delta;
                best_delta = delta;
            }
        }

        if (best_abs <= (period_samples * 0.5f))
        {
            delta = best_delta + ADC_UO_DELAY_SAMPLES;
            phase_deg_raw = Measure_WrapPhase(delta * 360.0f / period_samples);
            phase_rad = phase_deg_raw * MEASURE_PI / 180.0f;
            sin_sum += sinf(phase_rad);
            cos_sum += cosf(phase_rad);
            pair_count++;
        }
    }

    if (pair_count == 0)
    {
        return 0;
    }

    phase_deg_raw = atan2f(sin_sum, cos_sum) * 180.0f / MEASURE_PI;
    phase_deg_raw = Measure_WrapPhase(phase_deg_raw);

    return Measure_FilterPhase(phase_deg_raw, phase_deg);
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
    result->uo_dc = Measure_CalcDC(uo_buf, len) * UO_SCALE;
    result->ui_rms = Measure_CalcRMS_AC(ui_buf, len, UI_SCALE);
    result->uo_rms = Measure_CalcRMS_AC(uo_buf, len, UO_SCALE);
    result->phase_diff = 0.0f;
    result->phase_valid = 0;
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

    if ((result->ui_rms >= UI_MIN_RMS) && (result->uo_rms >= UI_MIN_RMS))
    {
        result->phase_valid = Measure_CalcPhaseDiff(ui_buf, uo_buf, len, set_freq, &result->phase_diff);
    }

    result->clip = 0;
    if (Measure_DetectClipVoltage(uo_buf, len))
    {
        result->clip = 1;
    }
}
