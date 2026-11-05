#pragma once
#include "DspUtils.h"

namespace msc
{
// Real-time pitch-synchronous overlap-add (PSOLA-style) stereo pitch shifter.
//
// Output grains (Hann, half-width H = max(Tin, Tout)) are scheduled every Tout = Tin / ratio samples.
// Each grain is read from the input at a centre that is always an integer number of detected periods
// away from the previous grain's centre, so successive grains stay waveform-coherent. Periods are
// repeated (pitch up) or skipped (pitch down) while the overall timing is tracked in real time.
// Fixed latency: latencySamples().
class PsolaShifter
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        minPeriod = sr / 1000.0;
        maxPeriod = sr / 70.0;
        latency = (int) std::ceil (0.5 * maxPeriod) + 4;
        const int size = nextPow2 ((int) (0.35 * sr));
        mask = size - 1;
        buf[0].assign ((size_t) size, 0.0f);
        buf[1].assign ((size_t) size, 0.0f);
        reset();
    }
    void reset()
    {
        std::fill (buf[0].begin(), buf[0].end(), 0.0f);
        std::fill (buf[1].begin(), buf[1].end(), 0.0f);
        for (auto& g : grains) g.active = false;
        n = 0; first = true; eNext = 0.0; cPrev = 0.0;
    }
    int latencySamples() const { return latency; }

    // ratio = output frequency / input frequency, periodSamples = detected input period,
    // blend 0..1 crossfades between the plain delayed input (0) and the shifted signal (1).
    void process (float inL, float inR, float ratioIn, float periodSamples, float blend, float& outL, float& outR)
    {
        buf[0][(size_t) (n & mask)] = inL;
        buf[1][(size_t) (n & mask)] = inR;
        const long long t = n - latency;

        if (first) { eNext = (double) t; cPrev = (double) t; first = false; }

        const double Tin  = clampv ((double) periodSamples, minPeriod, maxPeriod);
        const double r    = clampv ((double) ratioIn, 0.5, 2.0);
        const double Tout = Tin / r;
        const double H    = std::max (Tin, Tout);

        // schedule new grains
        while (eNext - H <= (double) t)
        {
            double c = cPrev + std::round ((eNext - cPrev) / Tin) * Tin;
            const double limit = eNext + (double) latency - 3.0;      // never read the future
            while (c > limit) c -= Tin;
            while (c < eNext - 0.25 * (double) (mask + 1)) c += Tin;  // never read too far in the past
            cPrev = c;
            spawn (eNext, c, H);
            eNext += Tout;
        }

        // overlap-add active grains
        double sumW = 0.0, accL = 0.0, accR = 0.0;
        for (auto& g : grains)
        {
            if (! g.active) continue;
            const double tau = (double) t - g.e;
            if (tau > g.H) { g.active = false; continue; }
            if (tau < -g.H) continue;
            const double w = 0.5 + 0.5 * std::cos (kPi * tau / g.H);
            const double pos = g.c + tau;
            accL += w * (double) read (0, pos);
            accR += w * (double) read (1, pos);
            sumW += w;
        }
        const double norm = 1.0 / std::max (sumW, 0.5);
        const float psL = (float) (accL * norm), psR = (float) (accR * norm);
        const float dryL = buf[0][(size_t) (t & mask)], dryR = buf[1][(size_t) (t & mask)];
        outL = dryL + blend * (psL - dryL);
        outR = dryR + blend * (psR - dryR);
        ++n;
    }

private:
    struct Grain { double e = 0, c = 0, H = 1; bool active = false; };

    void spawn (double e, double c, double H)
    {
        for (auto& g : grains)
            if (! g.active) { g.e = e; g.c = c; g.H = H; g.active = true; return; }
        // pool full: replace the oldest
        size_t oldest = 0;
        for (size_t i = 1; i < kMaxGrains; ++i) if (grains[i].e < grains[oldest].e) oldest = i;
        grains[oldest].e = e; grains[oldest].c = c; grains[oldest].H = H; grains[oldest].active = true;
    }
    float read (int ch, double pos) const
    {
        const long long i = (long long) std::floor (pos);
        const float f = (float) (pos - (double) i);
        const auto& b = buf[ch];
        return hermite (b[(size_t) ((i - 1) & mask)], b[(size_t) (i & mask)],
                        b[(size_t) ((i + 1) & mask)], b[(size_t) ((i + 2) & mask)], f);
    }

    static constexpr size_t kMaxGrains = 16;
    double sampleRate = 44100.0, minPeriod = 44.1, maxPeriod = 630.0, eNext = 0.0, cPrev = 0.0;
    int latency = 320;
    long long mask = 0, n = 0;
    bool first = true;
    std::vector<float> buf[2];
    Grain grains[kMaxGrains];
};
} // namespace msc
