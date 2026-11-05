#pragma once
#include "Biquad.h"

namespace msc
{
// "Air / brightness": gentle 11 kHz high shelf plus a soft-saturated high-band exciter.
class Air
{
public:
    void prepare (double sr)
    {
        sampleRate = sr; lastAmount = -1.0f;
        const auto hp = makeBiquad (FilterType::HighPass, sr, 5500.0, 0.7071);
        for (auto& h : hpf) h.setCoeffs (hp);
        reset(); setAmount (0.0f);
    }
    void setAmount (float a) // 0..1
    {
        amount = a;
        if (std::fabs (a - lastAmount) < 1.0e-4f) return;
        lastAmount = a;
        const auto s = makeBiquad (FilterType::HighShelf, sampleRate, 11000.0, 0.6, 9.0 * a);
        for (auto& x : shelf) x.setCoeffs (s);
    }
    void process (float* l, float* r, int n)
    {
        float* ch[2] = { l, r };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[c][i];
                const float hi = hpf[c].process (x);
                const float exc = std::tanh (hi * 3.0f) * 0.33f;      // harmonics from the high band
                ch[c][i] = shelf[c].process (x) + exc * 0.35f * amount;
            }
    }
    void reset() { for (auto& b : shelf) b.reset(); for (auto& b : hpf) b.reset(); }
private:
    double sampleRate = 44100.0;
    Biquad shelf[2], hpf[2];
    float amount = 0.0f, lastAmount = -1.0f;
};
} // namespace msc
