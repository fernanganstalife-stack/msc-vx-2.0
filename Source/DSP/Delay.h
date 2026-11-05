#pragma once
#include "DspUtils.h"

namespace msc
{
// Stereo delay with smoothed time, feedback low-pass ("tone"), optional ping-pong and tempo sync helper.
class Delay
{
public:
    static constexpr float kMaxMs = 3000.0f;

    void prepare (double sr)
    {
        sampleRate = sr;
        line[0].prepare ((int) (kMaxMs * 0.001 * sr) + 8);
        line[1].prepare ((int) (kMaxMs * 0.001 * sr) + 8);
        timeCoef = onePoleCoef (80.0f, sr);
        reset();
        setParams (350.0f, 0.35f, 0.4f, 0.6f, false);
        curSamples = tgtSamples;
    }
    void setParams (float timeMs, float feedbackIn, float mixIn, float toneIn, bool pingPongIn)
    {
        tgtSamples = clampv (timeMs, 1.0f, kMaxMs) * 0.001f * (float) sampleRate;
        feedback = clampv (feedbackIn, 0.0f, 0.95f);
        mix = mixIn;
        pingPong = pingPongIn;
        const float fc = 1500.0f * std::pow (8.0f, clampv (toneIn, 0.0f, 1.0f));   // 1.5k .. 12k
        lpCoef = std::exp (-(float) kTwoPi * std::min (fc, (float) sampleRate * 0.45f) / (float) sampleRate);
    }
    void process (float* l, float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            curSamples = tgtSamples + timeCoef * (curSamples - tgtSamples);
            const float dl = line[0].read (curSamples - 1.0f);
            const float dr = line[1].read (curSamples - 1.0f);
            lpL = flushDenormal (dl + lpCoef * (lpL - dl) * 1.0f);  // y = x + a(y - x)
            lpR = flushDenormal (dr + lpCoef * (lpR - dr) * 1.0f);
            const float inL = l[i], inR = r[i];
            if (pingPong)
            {
                line[0].push (0.5f * (inL + inR) + feedback * lpR);
                line[1].push (feedback * lpL);
            }
            else
            {
                line[0].push (inL + feedback * lpL);
                line[1].push (inR + feedback * lpR);
            }
            l[i] = inL + dl * mix;
            r[i] = inR + dr * mix;
        }
    }
    void reset() { line[0].clear(); line[1].clear(); lpL = lpR = 0.0f; }
    // Jump the smoothed delay time straight to its target (call after the first setParams()).
    void snap() { curSamples = tgtSamples; }

    // Musical time helper. division: 0=1/1 1=1/2 2=1/4 3=1/8 4=1/16 5=1/32
    static float syncedMs (double bpm, int division, bool dotted, bool triplet)
    {
        static const float beats[6] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f, 0.125f };
        float b = beats[clampv (division, 0, 5)];
        if (dotted) b *= 1.5f; else if (triplet) b *= 2.0f / 3.0f;
        return (float) (60000.0 / std::max (bpm, 20.0)) * b;
    }
private:
    double sampleRate = 44100.0;
    DelayLine line[2];
    float tgtSamples = 1000.0f, curSamples = 1000.0f, timeCoef = 0.0f;
    float feedback = 0.35f, mix = 0.4f, lpCoef = 0.0f, lpL = 0.0f, lpR = 0.0f;
    bool pingPong = false;
};
} // namespace msc
