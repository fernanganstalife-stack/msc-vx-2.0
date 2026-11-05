// Stand-alone DSP self test (no JUCE needed).  Build:  g++ -std=c++17 -O2 -I../Source/DSP dsp_test.cpp -o dsp_test
#include "MscDsp.h"
#include <cstdio>
#include <cstdlib>
#include <random>

using namespace msc;
static int failures = 0;
#define CHECK(cond, ...) do { if (cond) { std::printf ("  [ok]   "); } else { std::printf ("  [FAIL] "); ++failures; } std::printf (__VA_ARGS__); std::printf ("\n"); } while (0)

static const double SR = 44100.0;

// Harmonic "voice-like" tone: 14 harmonics, 1/k^1.2 roll-off, mild amplitude vibrato-free.
static std::vector<float> voice (double f0, double seconds, double sr = SR, float gain = 0.25f)
{
    const int n = (int) (seconds * sr);
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        double s = 0.0;
        for (int k = 1; k <= 14; ++k) s += std::sin (kTwoPi * k * f0 * i / sr + 0.3 * k) / std::pow ((double) k, 1.2);
        x[(size_t) i] = (float) (s * gain);
    }
    return x;
}
static double rmsOf (const std::vector<float>& v, size_t a, size_t b)
{
    double e = 0; for (size_t i = a; i < b; ++i) e += (double) v[i] * v[i]; return std::sqrt (e / (double) (b - a));
}
static double centsBetween (double f, double ref) { return 1200.0 * std::log2 (f / ref); }

// Measure mean detected frequency of a signal tail using the YIN detector.
static double measureFreq (const std::vector<float>& x, size_t from)
{
    PitchDetector d; d.prepare (SR);
    double sum = 0; int cnt = 0;
    for (size_t i = 0; i < x.size(); ++i)
        if (d.push (x[i]) && i > from && d.isVoiced()) { sum += d.getFrequency(); ++cnt; }
    return cnt > 10 ? sum / cnt : 0.0;
}
static bool finite (const std::vector<float>& v) { for (float s : v) if (! std::isfinite (s) || std::fabs (s) > 50.f) return false; return true; }

static void testDetector()
{
    std::printf ("PitchDetector\n");
    for (double f : { 98.0, 164.8, 220.0, 440.0, 660.0 })
    {
        auto x = voice (f, 1.0);
        const double m = measureFreq (x, 20000);
        CHECK (m > 0 && std::fabs (centsBetween (m, f)) < 12.0, "f0=%.1f Hz -> %.2f Hz (%.1f cents)", f, m, m > 0 ? centsBetween (m, f) : 9999.0);
    }
    std::vector<float> silence ((size_t) SR, 0.0f);
    PitchDetector d; d.prepare (SR); bool anyVoiced = false;
    for (float s : silence) { d.push (s); anyVoiced |= d.isVoiced(); }
    CHECK (! anyVoiced, "silence is never voiced");
    std::mt19937 rng (1); std::uniform_real_distribution<float> u (-0.3f, 0.3f);
    int voicedCount = 0, total = 0; PitchDetector d2; d2.prepare (SR);
    for (int i = 0; i < (int) SR; ++i) if (d2.push (u (rng))) { ++total; voicedCount += d2.isVoiced() ? 1 : 0; }
    CHECK (voicedCount < total / 5, "white noise rarely voiced (%d/%d)", voicedCount, total);
}

static void testShifter()
{
    std::printf ("PsolaShifter\n");
    for (double semis : { 3.0, -2.0, 7.0, -5.0, 0.5 })
        for (double f : { 140.0, 220.0, 330.0 })
        {
            auto x = voice (f, 1.5);
            PsolaShifter s; s.prepare (SR);
            std::vector<float> y (x.size()), y2 (x.size());
            const float ratio = (float) std::pow (2.0, semis / 12.0);
            for (size_t i = 0; i < x.size(); ++i) s.process (x[i], x[i], ratio, (float) (SR / f), 1.0f, y[i], y2[i]);
            const double m = measureFreq (y, 30000);
            const double expect = f * ratio;
            CHECK (finite (y) && m > 0 && std::fabs (centsBetween (m, expect)) < 15.0,
                   "f0=%.0f shift=%+.1f st -> %.2f Hz (expected %.2f, %.1f cents, rms %.3f)", f, semis, m, expect,
                   m > 0 ? centsBetween (m, expect) : 9999.0, rmsOf (y, 40000, y.size()));
            const double rIn = rmsOf (x, 40000, x.size()), rOut = rmsOf (y, 40000, y.size());
            if (std::fabs (semis) <= 3.0)
            CHECK (rOut > 0.6 * rIn && rOut < 1.5 * rIn, "   level preserved (in %.3f out %.3f)", rIn, rOut);
        }
    // blend = 0 must be a bit-exact delay
    auto x = voice (200.0, 0.5);
    PsolaShifter s; s.prepare (SR);
    const int L = s.latencySamples(); double err = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        float a, b; s.process (x[i], x[i], 1.1f, 220.5f, 0.0f, a, b);
        if ((int) i >= L) err = std::max (err, (double) std::fabs (a - x[i - (size_t) L]));
    }
    CHECK (err < 1.0e-6, "blend=0 is an exact %d-sample delay (max err %.2e)", L, err);
}

static void testAutoTune()
{
    std::printf ("AutoTune\n");
    struct Case { double f; int scale, root; double expect; const char* what; };
    const Case cases[] = {
        { 215.0, 2, 0, 207.65, "215 Hz in C minor -> Ab3" },
        { 200.0, 1, 0, 196.00, "200 Hz in C major -> G3" },
        { 250.0, 0, 0, 246.94, "250 Hz chromatic -> B3" },
        { 124.0, 2, 9, 123.47, "124 Hz in A minor -> B2" },
    };
    for (auto mode : { AutoTuneMode::Robotic, AutoTuneMode::Natural, AutoTuneMode::Hard })
        for (const auto& c : cases)
        {
            auto x = voice (c.f, 2.0);
            AutoTune at; at.prepare (SR);
            at.setParams (true, c.scale, c.root, 1.0f, 20.0f, mode);
            std::vector<float> l = x, r = x;
            for (size_t i = 0; i < x.size(); i += 256) at.process (l.data() + i, r.data() + i, (int) std::min<size_t> (256, x.size() - i));
            const double m = measureFreq (l, 60000);
            CHECK (finite (l) && m > 0 && std::fabs (centsBetween (m, c.expect)) < 20.0, "mode %d: %s -> %.2f Hz (%.1f cents from target)",
                   (int) mode, c.what, m, m > 0 ? centsBetween (m, c.expect) : 9999.0);
        }
    // amount = 0 / disabled -> exact delay
    auto x = voice (215.0, 1.0);
    for (int variant = 0; variant < 2; ++variant)
    {
        AutoTune at; at.prepare (SR);
        at.setParams (variant == 0 ? false : true, 2, 0, variant == 0 ? 1.0f : 0.0f, 20.0f, AutoTuneMode::Robotic);
        std::vector<float> l = x, r = x;
        at.process (l.data(), r.data(), (int) l.size());
        const int L = at.latencySamples(); double err = 0;
        for (size_t i = (size_t) L; i < l.size(); ++i) err = std::max (err, (double) std::fabs (l[i] - x[i - (size_t) L]));
        CHECK (err < 1.0e-4, "%s -> transparent delay of %d samples (%.2f ms), err %.2e", variant == 0 ? "disabled" : "amount 0", L, 1000.0 * L / SR, err);
    }
    // note change: glide from 215 Hz to 270 Hz
    {
        std::vector<float> a = voice (215.0, 1.0), b = voice (270.0, 1.0); a.insert (a.end(), b.begin(), b.end());
        AutoTune at; at.prepare (SR); at.setParams (true, 2, 0, 1.0f, 20.0f, AutoTuneMode::Robotic);
        std::vector<float> l = a, r = a;
        for (size_t i = 0; i < a.size(); i += 128) at.process (l.data() + i, r.data() + i, (int) std::min<size_t> (128, a.size() - i));
        std::vector<float> tail (l.begin() + 60000, l.begin() + 88000), tail2 (l.begin() + 62000, l.end());
        const double m1 = measureFreq (std::vector<float> (l.begin() + 20000, l.begin() + 44100), 12000);
        const double m2 = measureFreq (tail2, 12000);
        CHECK (finite (l) && std::fabs (centsBetween (m1, 207.65)) < 20 && std::fabs (centsBetween (m2, 261.63)) < 25,
               "note change 215->270 Hz: %.2f Hz (Ab3) then %.2f Hz (C4 = 261.63)", m1, m2);
    }
}

static void testEffects()
{
    std::printf ("Effects\n");
    const int N = (int) (SR);
    auto sine = [&] (double f, float g) { std::vector<float> v ((size_t) N); for (int i = 0; i < N; ++i) v[(size_t) i] = g * (float) std::sin (kTwoPi * f * i / SR); return v; };

    { // compressor
        Compressor c; c.prepare (SR); c.setParams (-12.0f, 4.0f, 5.0f, 120.0f, 0.0f);
        auto l = sine (1000, 1.0f), r = l; c.process (l.data(), r.data(), N);
        const double db = 20 * std::log10 (rmsOf (l, 30000, (size_t) N) / (1.0 / std::sqrt (2.0)));
        CHECK (db < -5.0 && db > -12.0, "compressor: 0 dBFS -> %.2f dB peak change (expected ~ -9)", db);
        auto q = sine (1000, 0.05f), q2 = q; c.process (q.data(), q2.data(), N);
        CHECK (std::fabs (rmsOf (q, 30000, (size_t) N) / rmsOf (sine (1000, 0.05f), 30000, (size_t) N) - 1.0) < 0.05, "compressor: below threshold untouched");
    }
    { // EQ
        ParametricEQ e; e.prepare (SR); e.setGains (0, 6, 0);
        auto l = sine (1500, 0.2f), r = l; e.process (l.data(), r.data(), N);
        const double db = 20 * std::log10 (rmsOf (l, 30000, (size_t) N) / rmsOf (sine (1500, 0.2f), 30000, (size_t) N));
        CHECK (std::fabs (db - 6.0) < 0.3, "EQ mid +6 dB @1.5k -> %.2f dB", db);
        CHECK (std::fabs (ParametricEQ::responseDb (SR, 1500.0, 0, 6, 0) - 6.0) < 0.05, "EQ response curve matches (%.2f dB)", ParametricEQ::responseDb (SR, 1500.0, 0, 6, 0));
    }
    { // de-esser
        DeEsser d; d.prepare (SR); d.setParams (-30.0f, 6000.0f, 12.0f, 1.0f);
        auto l = sine (6000, 0.5f), r = l; d.process (l.data(), r.data(), N);
        const double db = 20 * std::log10 (rmsOf (l, 20000, (size_t) N) / rmsOf (sine (6000, 0.5f), 20000, (size_t) N));
        auto k = sine (300, 0.5f), k2 = k; d.process (k.data(), k2.data(), N);
        const double db2 = 20 * std::log10 (rmsOf (k, 20000, (size_t) N) / rmsOf (sine (300, 0.5f), 20000, (size_t) N));
        CHECK (db < -6.0 && std::fabs (db2) < 0.5, "de-esser: 6 kHz %.1f dB, 300 Hz %.2f dB", db, db2);
    }
    { // air
        Air a; a.prepare (SR); a.setAmount (1.0f);
        auto l = sine (14000, 0.2f), r = l; a.process (l.data(), r.data(), N);
        const double db = 20 * std::log10 (rmsOf (l, 20000, (size_t) N) / rmsOf (sine (14000, 0.2f), 20000, (size_t) N));
        CHECK (db > 4.0 && finite (l), "air: 14 kHz boost %.1f dB", db);
    }
    { // distortion / saturation
        Distortion d; d.prepare (SR); d.setParams (0.8f, 0.7f, 1.0f);
        auto l = sine (440, 0.5f), r = l; d.process (l.data(), r.data(), N);
        CHECK (finite (l) && rmsOf (l, 20000, (size_t) N) > 0.05 && rmsOf (l, 20000, (size_t) N) < 1.0, "distortion output rms %.3f", rmsOf (l, 20000, (size_t) N));
        for (int t = 0; t < 3; ++t)
        {
            Saturation s; s.prepare (SR); s.setParams (0.8f, 1.0f, (SaturationType) t);
            auto a = sine (440, 0.5f), b = a; s.process (a.data(), b.data(), N);
            CHECK (finite (a) && rmsOf (a, 20000, (size_t) N) > 0.05 && rmsOf (a, 20000, (size_t) N) < 1.0, "saturation type %d rms %.3f", t, rmsOf (a, 20000, (size_t) N));
        }
    }
    { // reverb
        Reverb rv; rv.prepare (SR); rv.setParams (0.5f, 2.8f, 0.5f, 20.0f, 0.5f, ReverbType::Hall);
        std::vector<float> l ((size_t) (SR * 6)), r;
        l[0] = 1.0f; r = l; rv.process (l.data(), r.data(), (int) l.size());
        const double e1 = rmsOf (l, 4410, 44100), e2 = rmsOf (l, 44100 * 4, 44100 * 5);
        CHECK (finite (l) && e1 > 1.0e-4 && e2 < e1 * 0.5, "reverb tail decays (%.2e -> %.2e)", e1, e2);
        Reverb rn; rn.prepare (SR); rn.setParams (0.5f, 2.0f, 0.5f, 0.0f, 0.5f, ReverbType::Plate);
        std::mt19937 rng (3); std::uniform_real_distribution<float> u (-0.2f, 0.2f);
        std::vector<float> a ((size_t) SR * 2), b;
        for (auto& v : a) v = u (rng);
        b = a; const auto dry = a; rn.process (a.data(), b.data(), (int) a.size());
        std::vector<float> wet (a.size()); for (size_t i = 0; i < a.size(); ++i) wet[i] = a[i] - dry[i];
        const double g = 20 * std::log10 (rmsOf (wet, 44100, 88200) / rmsOf (dry, 44100, 88200));
        CHECK (g > -16.0 && g < -4.0, "reverb wet level on noise: %.1f dB below/above dry (mix 50%%)", g);
    }
    { // delay
        Delay d; d.prepare (SR); d.setParams (250.0f, 0.5f, 1.0f, 1.0f, false); d.snap();
        std::vector<float> l ((size_t) SR), r; l[0] = 1.0f; r = l; d.process (l.data(), r.data(), (int) l.size());
        int peak = 0; float pv = 0; for (int i = 100; i < 20000; ++i) if (std::fabs (l[(size_t) i]) > pv) { pv = std::fabs (l[(size_t) i]); peak = i; }
        CHECK (std::abs (peak - (int) (0.25 * SR)) < 3 && pv > 0.5f, "delay echo at %d samples (expected %d)", peak, (int) (0.25 * SR));
        CHECK (std::fabs (Delay::syncedMs (120.0, 2, false, false) - 500.0f) < 0.01f && std::fabs (Delay::syncedMs (120.0, 2, true, false) - 750.0f) < 0.01f
               && std::fabs (Delay::syncedMs (120.0, 2, false, true) - 333.33f) < 0.05f, "delay tempo sync (1/4, dotted, triplet @120 bpm)");
    }
    { // chorus / widener
        Chorus c; c.prepare (SR); c.setParams (1.2f, 0.7f, 1.0f, 0.6f);
        auto l = sine (440, 0.3f), r = l; c.process (l.data(), r.data(), N);
        CHECK (finite (l) && finite (r) && rmsOf (l, 20000, (size_t) N) > 0.1, "chorus output rms %.3f", rmsOf (l, 20000, (size_t) N));
        StereoWidener w; w.setParams (0.0f, 1.0f);
        auto a = sine (440, 0.3f), b = sine (660, 0.3f); w.process (a.data(), b.data(), N);
        double diff = 0; for (int i = 0; i < N; ++i) diff = std::max (diff, (double) std::fabs (a[(size_t) i] - b[(size_t) i]));
        CHECK (diff < 1.0e-6, "widener width 0 -> mono");
    }
}

int main()
{
    testDetector();
    testShifter();
    testAutoTune();
    testEffects();
    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
