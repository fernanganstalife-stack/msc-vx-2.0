#pragma once
#include "DspUtils.h"

namespace msc
{
enum class ReverbType { Hall = 0, Room = 1, Plate = 2, Chamber = 3 };

// Stereo Freeverb-style reverb (8 damped combs + 4 all-passes per channel) with
// RT60-based feedback, smoothed fractional comb lengths and a pre-delay.
class Reverb
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        const double scale = sr / 44100.0;
        static const int combTune[kCombs] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 };
        static const int apTune[kAps]     = { 556, 441, 341, 225 };
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int i = 0; i < kCombs; ++i)
            {
                nominal[ch][i] = (float) ((combTune[i] + (ch == 1 ? kSpread : 0)) * scale);
                combs[ch][i].prepare ((int) (nominal[ch][i] * 3.2f) + 8);
                curLen[ch][i] = nominal[ch][i];
            }
            for (int i = 0; i < kAps; ++i)
                aps[ch][i].prepare ((int) ((apTune[i] + (ch == 1 ? kSpread : 0)) * scale) + 4,
                                    (int) ((apTune[i] + (ch == 1 ? kSpread : 0)) * scale));
        }
        pre.prepare ((int) (0.25 * sr));
        lenCoef = onePoleCoef (60.0f, sr);
        reset();
        setParams (0.5f, 2.8f, 0.5f, 20.0f, 0.3f, ReverbType::Hall);
    }
    // size, damping, mix: 0..1; decaySec: RT60 seconds; preDelayMs
    void setParams (float sizeIn, float decaySec, float dampingIn, float preDelayMs, float mixIn, ReverbType t)
    {
        float sizeMul = 1.0f, decayMul = 1.0f, dampBias = 0.0f;
        apFb = 0.5f;
        switch (t)
        {
            case ReverbType::Hall:    sizeMul = 1.00f; decayMul = 1.0f; break;
            case ReverbType::Room:    sizeMul = 0.55f; decayMul = 0.55f; dampBias = 0.1f; break;
            case ReverbType::Plate:   sizeMul = 0.80f; decayMul = 0.8f;  dampBias = -0.15f; apFb = 0.65f; break;
            case ReverbType::Chamber: sizeMul = 0.70f; decayMul = 0.7f; break;
        }
        const float sz = 0.4f + sizeIn * 1.2f;                       // 0.4 .. 1.6
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < kCombs; ++i)
            {
                tgtLen[ch][i] = nominal[ch][i] * sz * sizeMul;
                const float sec = tgtLen[ch][i] / (float) sampleRate;
                const float rt = std::max (0.1f, decaySec * decayMul);
                fb[ch][i] = std::min (0.985f, std::pow (10.0f, -3.0f * sec / rt));
            }
        damp = clampv (dampingIn + dampBias, 0.0f, 0.95f) * 0.85f;
        preSamples = preDelayMs * 0.001f * (float) sampleRate;
        mix = mixIn;
    }
    void process (float* l, float* r, int n)
    {
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            pre.push (0.5f * (io[0][i] + io[1][i]));
            const float in = pre.read (preSamples) * 0.03f;
            float out[2] = { 0, 0 };
            for (int ch = 0; ch < 2; ++ch)
            {
                float acc = 0.0f;
                for (int c = 0; c < kCombs; ++c)
                {
                    curLen[ch][c] = tgtLen[ch][c] + lenCoef * (curLen[ch][c] - tgtLen[ch][c]);
                    acc += combs[ch][c].tick (in, curLen[ch][c], fb[ch][c], damp);
                }
                for (int a = 0; a < kAps; ++a) acc = aps[ch][a].tick (acc, apFb);
                out[ch] = acc;
            }
            io[0][i] += out[0] * mix * 5.0f;
            io[1][i] += out[1] * mix * 5.0f;
        }
    }
    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (auto& c : combs[ch]) c.clear();
            for (auto& a : aps[ch]) a.clear();
        }
        pre.clear();
    }
private:
    static constexpr int kCombs = 8, kAps = 4, kSpread = 23;

    struct Comb
    {
        DelayLine line; float filt = 0.0f;
        void prepare (int maxLen) { line.prepare (maxLen); }
        void clear() { line.clear(); filt = 0.0f; }
        float tick (float in, float len, float feedback, float damping)
        {
            const float o = line.read (len - 1.0f);
            filt = flushDenormal (o * (1.0f - damping) + filt * damping);
            line.push (in + filt * feedback);
            return o;
        }
    };
    struct AllPass
    {
        DelayLine line; int len = 1;
        void prepare (int maxLen, int l) { line.prepare (maxLen); len = std::max (1, l); }
        void clear() { line.clear(); }
        float tick (float in, float g)
        {
            const float d = line.read ((float) len - 1.0f);
            const float v = in + g * d;
            line.push (v);
            return d - g * v;
        }
    };

    double sampleRate = 44100.0;
    Comb combs[2][kCombs];
    AllPass aps[2][kAps];
    DelayLine pre;
    float nominal[2][kCombs] {}, tgtLen[2][kCombs] {}, curLen[2][kCombs] {}, fb[2][kCombs] {};
    float damp = 0.3f, apFb = 0.5f, preSamples = 0.0f, mix = 0.3f, lenCoef = 0.0f;
};
} // namespace msc
