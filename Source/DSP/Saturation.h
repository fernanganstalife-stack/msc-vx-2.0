#pragma once
#include "Oversampler.h"

namespace msc
{
enum class SaturationType { Tube = 0, Tape = 1, Transistor = 2 };

// Three saturation characters, 2x oversampled, with DC blocking.
class Saturation
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        dcCoef = std::exp (-kTwoPi * 20.0 / sr);
        const auto lp = makeBiquad (FilterType::LowPass, sr * 2.0, std::min (13000.0, sr * 0.9), 0.7071);
        for (auto& c : ch) { c.os.prepare (sr); c.tapeLp.setCoeffs (lp); c.tapeLp.reset(); c.dcX = c.dcY = 0.0f; }
        setParams (0.35f, 0.4f, SaturationType::Tube);
    }
    void setParams (float driveIn, float mixIn, SaturationType t) { drive = driveIn; mix = mixIn; type = t; }

    void process (float* l, float* r, int n)
    {
        float* d[2] = { l, r };
        const float g = 1.0f + drive * 9.0f;
        const float comp = 1.0f / std::sqrt (g);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                Channel& s = ch[c];
                float y = s.os.process (d[c][i], [&] (float v)
                {
                    const float x = v * g;
                    float w;
                    switch (type)
                    {
                        case SaturationType::Tube:       w = std::tanh (x + 0.25f) - std::tanh (0.25f); break;
                        case SaturationType::Tape:       w = s.tapeLp.process (std::atan (x * 1.3f) * 0.8f); break;
                        default:                         w = x / std::pow (1.0f + std::pow (std::fabs (x), 3.0f), 1.0f / 3.0f); break;
                    }
                    return v + mix * (w * comp * 1.2f - v);
                });
                // DC blocker
                const float outV = y - s.dcX + dcCoef * s.dcY;
                s.dcX = y; s.dcY = flushDenormal (outV);
                d[c][i] = outV;
            }
    }
    void reset() { for (auto& c : ch) { c.os.reset(); c.tapeLp.reset(); c.dcX = c.dcY = 0.0f; } }
private:
    struct Channel { Oversampler2x os; Biquad tapeLp; float dcX = 0, dcY = 0; };
    double sampleRate = 44100.0;
    float dcCoef = 0.999f, drive = 0.35f, mix = 0.4f;
    SaturationType type = SaturationType::Tube;
    Channel ch[2];
};
} // namespace msc
