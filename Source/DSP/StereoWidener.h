#pragma once
#include "DspUtils.h"

namespace msc
{
// Mid/side widener. width: 0..2 (1 = unchanged), mix 0..1.
class StereoWidener
{
public:
    void setParams (float widthIn, float mixIn) { width = widthIn; mix = mixIn; }
    void process (float* l, float* r, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            const float s = 0.5f * (l[i] - r[i]) * width;
            l[i] += mix * ((m + s) - l[i]);
            r[i] += mix * ((m - s) - r[i]);
        }
    }
private:
    float width = 1.2f, mix = 1.0f;
};
} // namespace msc
