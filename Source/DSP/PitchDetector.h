#pragma once
#include "DspUtils.h"

namespace msc
{
// Monophonic pitch detector based on the YIN algorithm (de Cheveigne & Kawahara, 2002).
// The input is decimated to ~11 kHz, analysed every ~6 ms and refined with parabolic interpolation.
class PitchDetector
{
public:
    static constexpr double kMinHz = 70.0, kMaxHz = 1000.0;

    void prepare (double sr)
    {
        sampleRate = sr;
        decim = std::max (1, (int) std::lround (sr / 11025.0));
        dsRate = sr / decim;
        minTau = std::max (2, (int) std::floor (dsRate / kMaxHz));
        maxTau = (int) std::ceil (dsRate / kMinHz) + 1;
        window = 256;
        ring.assign ((size_t) nextPow2 (window + maxTau + 8), 0.0f);
        ringMask = (int) ring.size() - 1;
        tmp.assign ((size_t) (window + maxTau + 2), 0.0f);
        d.assign ((size_t) (maxTau + 2), 0.0f);
        cmnd.assign ((size_t) (maxTau + 2), 1.0f);
        reset();
    }
    void reset()
    {
        std::fill (ring.begin(), ring.end(), 0.0f);
        acc = 0.0f; accCount = 0; widx = 0; sinceDetect = 0;
        freq = 0.0f; confidence = 0.0f; voiced = false; fresh = false;
    }

    // Feed one (mono) sample. Returns true when a new analysis result is available.
    bool push (float x)
    {
        acc += x;
        if (++accCount < decim) return false;
        const float ds = acc / (float) decim;
        acc = 0.0f; accCount = 0;
        widx = (widx + 1) & ringMask;
        ring[(size_t) widx] = ds;
        if (++sinceDetect >= 64) { sinceDetect = 0; analyse(); return true; }
        return false;
    }

    bool  isVoiced() const { return voiced; }
    float getFrequency() const { return freq; }       // Hz, valid when voiced
    float getConfidence() const { return confidence; }
    float getPeriodSamples() const { return freq > 0.0f ? (float) (sampleRate / freq) : 0.0f; }
    static float freqToMidi (float f) { return 69.0f + 12.0f * std::log2 (f / 440.0f); }
    static float midiToFreq (float m) { return 440.0f * std::pow (2.0f, (m - 69.0f) / 12.0f); }

private:
    void analyse()
    {
        const int total = window + maxTau;
        for (int i = 0; i < total; ++i)
            tmp[(size_t) i] = ring[(size_t) ((widx - (total - 1) + i) & ringMask)];

        // energy gate on the analysed window
        double e = 0.0;
        for (int i = 0; i < window; ++i) e += (double) tmp[(size_t) i] * tmp[(size_t) i];
        const double rms = std::sqrt (e / window);
        if (rms < 0.004) { voiced = false; confidence = 0.0f; return; }

        // difference function
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            double s = 0.0;
            for (int j = 0; j < window; ++j)
            {
                const double diff = (double) tmp[(size_t) j] - (double) tmp[(size_t) (j + tau)];
                s += diff * diff;
            }
            d[(size_t) tau] = (float) s;
        }
        // cumulative mean normalised difference
        cmnd[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= maxTau; ++tau)
        {
            running += d[(size_t) tau];
            cmnd[(size_t) tau] = running > 1.0e-12 ? (float) (d[(size_t) tau] * tau / running) : 1.0f;
        }
        // absolute threshold: first dip below 0.15, then follow it to its minimum
        int best = -1;
        for (int tau = minTau; tau < maxTau; ++tau)
        {
            if (cmnd[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < maxTau && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau]) ++tau;
                best = tau; break;
            }
        }
        if (best < 0)  // fall back to the global minimum if it is reasonably good
        {
            float mn = 1.0f;
            for (int tau = minTau; tau < maxTau; ++tau)
                if (cmnd[(size_t) tau] < mn) { mn = cmnd[(size_t) tau]; best = tau; }
            if (mn > 0.30f) best = -1;
        }
        if (best < 1 || best >= maxTau) { voiced = false; confidence = 0.0f; return; }

        // parabolic interpolation
        const float a = cmnd[(size_t) (best - 1)], b = cmnd[(size_t) best], c = cmnd[(size_t) (best + 1)];
        const float denom = a - 2.0f * b + c;
        const float shift = std::fabs (denom) > 1.0e-9f ? 0.5f * (a - c) / denom : 0.0f;
        const float tauF = (float) best + clampv (shift, -1.0f, 1.0f);
        const float f = (float) (dsRate / tauF);
        confidence = clampv (1.0f - b, 0.0f, 1.0f);
        voiced = confidence > 0.7f && f >= (float) kMinHz && f <= (float) kMaxHz;
        if (voiced) freq = f;
    }

    double sampleRate = 44100.0, dsRate = 11025.0;
    int decim = 4, minTau = 11, maxTau = 158, window = 256;
    std::vector<float> ring, tmp, d, cmnd;
    int ringMask = 0, widx = 0, sinceDetect = 0, accCount = 0;
    float acc = 0.0f, freq = 0.0f, confidence = 0.0f;
    bool voiced = false, fresh = false;
};
} // namespace msc
