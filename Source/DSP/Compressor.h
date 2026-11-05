#pragma once
#include "DspUtils.h"

namespace msc
{
// Stereo-linked feed-forward compressor, soft knee, dB-domain attack/release smoothing.
class Compressor
{
public:
    void prepare (double sr) { sampleRate = sr; envDb = 0.0f; setParams (-12.0f, 4.0f, 10.0f, 120.0f, 0.0f); }
    void setParams (float thresholdDb, float ratioIn, float attackMs, float releaseMs, float makeupDb)
    {
        thresh = thresholdDb;
        ratio  = std::max (ratioIn, 1.0f);
        atkCoef = onePoleCoef (attackMs, sampleRate);
        relCoef = onePoleCoef (releaseMs, sampleRate);
        makeup  = makeupDb;
    }
    void process (float* l, float* r, int n)
    {
        constexpr float knee = 6.0f;
        float minGr = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float level = std::max (std::fabs (l[i]), std::fabs (r[i]));
            const float lvDb  = gainToDb (level);
            const float over  = lvDb - thresh;
            float outDb = lvDb;
            if (2.0f * over > knee)       outDb = thresh + over / ratio;
            else if (2.0f * over > -knee) { const float x = over + knee * 0.5f; outDb = lvDb + (1.0f / ratio - 1.0f) * x * x / (2.0f * knee); }
            const float grTarget = outDb - lvDb;               // <= 0
            const float coef = (grTarget < envDb) ? atkCoef : relCoef;
            envDb = grTarget + coef * (envDb - grTarget);
            envDb = flushDenormal (envDb);
            const float g = dbToGain (envDb + makeup);
            l[i] *= g; r[i] *= g;
            minGr = std::min (minGr, envDb);
        }
        grDb = minGr;
    }
    float getGainReductionDb() const { return grDb; }
    void reset() { envDb = 0.0f; grDb = 0.0f; }
private:
    double sampleRate = 44100.0;
    float thresh = -12.0f, ratio = 4.0f, atkCoef = 0.0f, relCoef = 0.0f, makeup = 0.0f;
    float envDb = 0.0f, grDb = 0.0f;
};
} // namespace msc
