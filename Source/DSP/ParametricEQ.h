#pragma once
#include "Biquad.h"

namespace msc
{
// 3-band EQ: low shelf 120 Hz, peaking 1.5 kHz (Q 0.8), high shelf 8 kHz.
class ParametricEQ
{
public:
    static constexpr double kLowHz = 120.0, kMidHz = 1500.0, kHighHz = 8000.0, kMidQ = 0.8;

    void prepare (double sr) { sampleRate = sr; reset(); lastLow = lastMid = lastHigh = 1000.0f; setGains (0, 0, 0); }
    void setGains (float lowDb, float midDb, float highDb)
    {
        if (lowDb == lastLow && midDb == lastMid && highDb == lastHigh) return;
        lastLow = lowDb; lastMid = midDb; lastHigh = highDb;
        const auto a = makeBiquad (FilterType::LowShelf, sampleRate, kLowHz, 0.7071, lowDb);
        const auto b = makeBiquad (FilterType::Peak, sampleRate, kMidHz, kMidQ, midDb);
        const auto c = makeBiquad (FilterType::HighShelf, sampleRate, kHighHz, 0.7071, highDb);
        for (int ch = 0; ch < 2; ++ch) { f[ch][0].setCoeffs (a); f[ch][1].setCoeffs (b); f[ch][2].setCoeffs (c); }
    }
    void process (float* l, float* r, int n)
    {
        float* ch[2] = { l, r };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                ch[c][i] = f[c][2].process (f[c][1].process (f[c][0].process (ch[c][i])));
    }
    void reset() { for (auto& c : f) for (auto& b : c) b.reset(); }

    // Combined magnitude response in dB (used by the UI curve).
    static double responseDb (double sr, double freq, float lowDb, float midDb, float highDb)
    {
        const auto a = makeBiquad (FilterType::LowShelf, sr, kLowHz, 0.7071, lowDb);
        const auto b = makeBiquad (FilterType::Peak, sr, kMidHz, kMidQ, midDb);
        const auto c = makeBiquad (FilterType::HighShelf, sr, kHighHz, 0.7071, highDb);
        const double m = biquadMagnitude (a, sr, freq) * biquadMagnitude (b, sr, freq) * biquadMagnitude (c, sr, freq);
        return 20.0 * std::log10 (std::max (m, 1.0e-9));
    }
private:
    double sampleRate = 44100.0;
    Biquad f[2][3];
    float lastLow = 0, lastMid = 0, lastHigh = 0;
};
} // namespace msc
