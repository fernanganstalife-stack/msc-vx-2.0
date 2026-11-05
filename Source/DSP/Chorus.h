#pragma once
#include "DspUtils.h"

namespace msc
{
// Stereo chorus: two LFO-modulated voices per channel, `width` offsets the right-channel LFO phase.
class Chorus
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        for (auto& l : line) l.prepare ((int) (0.06 * sr));
        reset();
        setParams (1.2f, 0.7f, 1.0f, 0.6f);
    }
    // rateHz, depth 0..1, width 0..1, mix 0..1
    void setParams (float rateHz, float depthIn, float widthIn, float mixIn)
    {
        inc = (float) (rateHz / sampleRate);
        depth = depthIn; width = widthIn; mix = mixIn;
    }
    void process (float* l, float* r, int n)
    {
        const float sr = (float) sampleRate;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            phase += inc; if (phase >= 1.0f) phase -= 1.0f;
            for (int c = 0; c < 2; ++c)
            {
                const float p = phase + (c == 1 ? 0.5f * width : 0.0f);
                const float s1 = std::sin ((float) kTwoPi * p);
                const float s2 = std::sin ((float) kTwoPi * (p + 0.25f));
                const float d1 = (0.011f + depth * 0.006f * s1) * sr;
                const float d2 = (0.017f + depth * 0.008f * s2) * sr;
                line[c].push (io[c][i]);
                const float wet = 0.5f * (line[c].read (d1) + line[c].read (d2));
                io[c][i] = io[c][i] + mix * (wet - io[c][i]);
            }
        }
    }
    void reset() { line[0].clear(); line[1].clear(); phase = 0.0f; }
private:
    double sampleRate = 44100.0;
    DelayLine line[2];
    float phase = 0.0f, inc = 0.0f, depth = 0.7f, width = 1.0f, mix = 0.6f;
};
} // namespace msc
