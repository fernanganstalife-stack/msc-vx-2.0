#pragma once
// Small helpers shared by every DSP module. No JUCE dependency on purpose:
// the whole DSP layer is plain C++17 so it can be unit-tested anywhere.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace msc
{
constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g)  { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

template <typename T> inline T clampv (T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }

inline float flushDenormal (float x) { return std::fabs (x) < 1.0e-20f ? 0.0f : x; }

inline float smoothstep (float edge0, float edge1, float x)
{
    float t = clampv ((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline int nextPow2 (int v)
{
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

// One-pole smoothing coefficient for a time constant in milliseconds.
inline float onePoleCoef (float ms, double sampleRate)
{
    if (ms <= 0.0f) return 0.0f;
    return std::exp (-1.0f / (0.001f * ms * (float) sampleRate));
}

// Parameter smoother (one pole).
class Smoother
{
public:
    void prepare (double sr, float ms) { coef = onePoleCoef (ms, sr); }
    void reset (float v) { z = v; }
    float next (float target) { z = target + coef * (z - target); return z; }
    float value() const { return z; }
private:
    float coef = 0.0f, z = 0.0f;
};

// Catmull-Rom / Hermite interpolation for ring buffers.
inline float hermite (float y0, float y1, float y2, float y3, float f)
{
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * f + c2) * f + c1) * f + c0;
}

// Simple circular delay line with fractional (linear) reads.
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        size = nextPow2 (maxSamples + 4);
        mask = size - 1;
        buf.assign ((size_t) size, 0.0f);
        pos = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; }
    void push (float x) { pos = (pos + 1) & mask; buf[(size_t) pos] = x; }
    // delay >= 0 samples (0 == last pushed sample)
    float read (float delay) const
    {
        const float d = clampv (delay, 0.0f, (float) (size - 4));
        const int   i = (int) d;
        const float f = d - (float) i;
        const float a = buf[(size_t) ((pos - i) & mask)];
        const float b = buf[(size_t) ((pos - i - 1) & mask)];
        return a + f * (b - a);
    }
    int capacity() const { return size - 4; }
private:
    std::vector<float> buf;
    int size = 0, mask = 0, pos = 0;
};
} // namespace msc
