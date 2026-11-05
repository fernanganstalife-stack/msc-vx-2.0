#pragma once
#include "Biquad.h"

namespace msc
{
// 2x oversampler for one channel: zero-stuff + 4th-order Butterworth low-pass up,
// the same filter + decimation down. Used by the non-linear stages to tame aliasing.
class Oversampler2x
{
public:
    void prepare (double sampleRate)
    {
        const double fs2 = sampleRate * 2.0;
        const double fc = std::min (sampleRate * 0.45, fs2 * 0.49);
        const auto c1 = makeBiquad (FilterType::LowPass, fs2, fc, 0.5412);
        const auto c2 = makeBiquad (FilterType::LowPass, fs2, fc, 1.3066);
        for (int i = 0; i < 2; ++i) { up[i].setCoeffs (i == 0 ? c1 : c2); dn[i].setCoeffs (i == 0 ? c1 : c2); }
        reset();
    }
    void reset() { for (int i = 0; i < 2; ++i) { up[i].reset(); dn[i].reset(); } }

    // f is called twice per input sample, at the doubled rate.
    template <typename F>
    float process (float x, F&& f)
    {
        float out = 0.0f;
        for (int k = 0; k < 2; ++k)
        {
            float s = (k == 0 ? x * 2.0f : 0.0f);
            s = up[1].process (up[0].process (s));
            s = f (s);
            s = dn[1].process (dn[0].process (s));
            if (k == 1) out = s;
        }
        return out;
    }
private:
    Biquad up[2], dn[2];
};
} // namespace msc
