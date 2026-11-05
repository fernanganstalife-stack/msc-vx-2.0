#pragma once
#include "DspUtils.h"
#include <complex>

namespace msc
{
enum class FilterType { LowPass, HighPass, BandPass, LowShelf, HighShelf, Peak };

struct BiquadCoeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
};

// RBJ audio-EQ-cookbook designs.
inline BiquadCoeffs makeBiquad (FilterType type, double sr, double freq, double q, double gainDb = 0.0)
{
    freq = clampv (freq, 10.0, sr * 0.49);
    q    = std::max (q, 0.05);
    const double w0 = kTwoPi * freq / sr;
    const double c = std::cos (w0), s = std::sin (w0);
    const double alpha = s / (2.0 * q);
    const double A = std::pow (10.0, gainDb / 40.0);
    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

    switch (type)
    {
        case FilterType::LowPass:
            b0 = (1 - c) / 2; b1 = 1 - c; b2 = (1 - c) / 2;
            a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; break;
        case FilterType::HighPass:
            b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2;
            a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; break;
        case FilterType::BandPass:
            b0 = alpha; b1 = 0; b2 = -alpha;
            a0 = 1 + alpha; a1 = -2 * c; a2 = 1 - alpha; break;
        case FilterType::Peak:
            b0 = 1 + alpha * A; b1 = -2 * c; b2 = 1 - alpha * A;
            a0 = 1 + alpha / A; a1 = -2 * c; a2 = 1 - alpha / A; break;
        case FilterType::LowShelf:
        {
            const double beta = 2.0 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1) - (A - 1) * c + beta);
            b1 = 2 * A * ((A - 1) - (A + 1) * c);
            b2 = A * ((A + 1) - (A - 1) * c - beta);
            a0 = (A + 1) + (A - 1) * c + beta;
            a1 = -2 * ((A - 1) + (A + 1) * c);
            a2 = (A + 1) + (A - 1) * c - beta; break;
        }
        case FilterType::HighShelf:
        {
            const double beta = 2.0 * std::sqrt (A) * alpha;
            b0 = A * ((A + 1) + (A - 1) * c + beta);
            b1 = -2 * A * ((A - 1) + (A + 1) * c);
            b2 = A * ((A + 1) + (A - 1) * c - beta);
            a0 = (A + 1) - (A - 1) * c + beta;
            a1 = 2 * ((A - 1) - (A + 1) * c);
            a2 = (A + 1) - (A - 1) * c - beta; break;
        }
    }
    BiquadCoeffs r;
    r.b0 = b0 / a0; r.b1 = b1 / a0; r.b2 = b2 / a0; r.a1 = a1 / a0; r.a2 = a2 / a0;
    return r;
}

// Magnitude response in linear gain at `freq` Hz.
inline double biquadMagnitude (const BiquadCoeffs& c, double sr, double freq)
{
    const double w = kTwoPi * freq / sr;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    const std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2;
    const std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2;
    return std::abs (num / den);
}

// Transposed direct form II, one channel.
class Biquad
{
public:
    void setCoeffs (const BiquadCoeffs& c) { k = c; }
    const BiquadCoeffs& coeffs() const { return k; }
    void reset() { z1 = z2 = 0.0; }
    float process (float x)
    {
        const double in = (double) x;
        const double out = k.b0 * in + z1;
        z1 = k.b1 * in - k.a1 * out + z2;
        z2 = k.b2 * in - k.a2 * out;
        return (float) out;
    }
private:
    BiquadCoeffs k;
    double z1 = 0.0, z2 = 0.0;
};
} // namespace msc
