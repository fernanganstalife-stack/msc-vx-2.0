#pragma once
#include "Oversampler.h"

namespace msc
{
// Drive + tone + mix distortion, processed at 2x oversampling.
class Distortion
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& c : ch) { c.os.prepare (sr); c.tone.reset(); }
        lastTone = -1.0f;
        setParams (0.3f, 0.5f, 0.5f);
    }
    // drive, tone, mix all 0..1
    void setParams (float driveIn, float toneIn, float mixIn)
    {
        drive = driveIn; mix = mixIn;
        preGain = dbToGain (drive * 34.0f);
        postGain = 1.0f / (1.0f + drive * drive * 6.0f);
        if (std::fabs (toneIn - lastTone) > 1.0e-4f)
        {
            lastTone = toneIn;
            const double fc = std::min (800.0 * std::pow (20.0, (double) toneIn), sampleRate * 0.9);
            const auto c = makeBiquad (FilterType::LowPass, sampleRate * 2.0, fc, 0.7071);
            for (auto& x : ch) x.tone.setCoeffs (c);
        }
    }
    void process (float* l, float* r, int n)
    {
        float* d[2] = { l, r };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                Channel& s = ch[c];
                d[c][i] = s.os.process (d[c][i], [&] (float v)
                {
                    const float x = v * preGain + 0.12f * drive;                    // asymmetry -> even harmonics
                    const float y = (std::tanh (x) - std::tanh (0.12f * drive)) * postGain * 1.4f;
                    return v + mix * (s.tone.process (y) - v);
                });
            }
    }
    void reset() { for (auto& c : ch) { c.os.reset(); c.tone.reset(); } }
private:
    struct Channel { Oversampler2x os; Biquad tone; };
    double sampleRate = 44100.0;
    Channel ch[2];
    float drive = 0.3f, mix = 0.5f, preGain = 1.0f, postGain = 1.0f, lastTone = -1.0f;
};
} // namespace msc
