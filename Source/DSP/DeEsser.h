#pragma once
#include "Biquad.h"

namespace msc
{
// Band-limited de-esser: a band-pass side-chain around `freq` is compressed on its own and
// recombined with the untouched remainder of the signal.
class DeEsser
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        atk = onePoleCoef (0.5f, sr);
        rel = onePoleCoef (40.0f, sr);
        for (auto& b : bp) b.reset();
        env = 0.0f; lastFreq = -1.0f;
        setParams (-30.0f, 6000.0f, 12.0f, 0.7f);
    }
    // thresholdDb: side-chain threshold, maxReductionDb: positive number, amount: 0..1
    void setParams (float thresholdDb, float freqHz, float maxReductionDb, float amountIn)
    {
        thresh = thresholdDb; maxRed = std::fabs (maxReductionDb); amount = amountIn;
        if (std::fabs (freqHz - lastFreq) > 0.01f)
        {
            lastFreq = freqHz;
            const auto c = makeBiquad (FilterType::BandPass, sampleRate, freqHz, 1.6);
            for (auto& b : bp) b.setCoeffs (c);
        }
    }
    void process (float* l, float* r, int n)
    {
        float* ch[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float bl = bp[0].process (ch[0][i]);
            const float br = bp[1].process (ch[1][i]);
            const float det = std::max (std::fabs (bl), std::fabs (br));
            const float coef = det > env ? atk : rel;
            env = det + coef * (env - det);
            env = flushDenormal (env);
            const float over = gainToDb (env) - thresh;
            const float red = over > 0.0f ? std::min (over * 0.75f, maxRed) * amount : 0.0f;
            const float g = dbToGain (-red);
            ch[0][i] += bl * (g - 1.0f);
            ch[1][i] += br * (g - 1.0f);
        }
    }
    void reset() { for (auto& b : bp) b.reset(); env = 0.0f; }
private:
    double sampleRate = 44100.0;
    Biquad bp[2];
    float thresh = -30.0f, maxRed = 12.0f, amount = 0.7f, env = 0.0f, atk = 0.0f, rel = 0.0f, lastFreq = -1.0f;
};
} // namespace msc
