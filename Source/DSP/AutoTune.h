#pragma once
#include "PitchDetector.h"
#include "PsolaShifter.h"

namespace msc
{
enum class AutoTuneMode { Robotic = 0, Natural = 1, Smooth = 2, Hard = 3 };

struct ScaleDef { const char* name; int count; int degrees[12]; };

inline const ScaleDef* getScaleDefs (int& numScales)
{
    static const ScaleDef defs[] = {
        { "Chromatic",      12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
        { "Major",           7, { 0, 2, 4, 5, 7, 9, 11 } },
        { "Minor",           7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "Harmonic Minor",  7, { 0, 2, 3, 5, 7, 8, 11 } },
        { "Pentatonic Maj",  5, { 0, 2, 4, 7, 9 } },
        { "Pentatonic Min",  5, { 0, 3, 5, 7, 10 } },
        { "Blues",           6, { 0, 3, 5, 6, 7, 10 } },
        { "Dorian",          7, { 0, 2, 3, 5, 7, 9, 10 } },
    };
    numScales = (int) (sizeof (defs) / sizeof (defs[0]));
    return defs;
}

// Real-time auto-tune: YIN pitch detection -> scale quantisation -> smoothed correction -> PSOLA shifter.
// The module has a fixed latency (see latencySamples()); when disabled it is a pure delay of that length.
class AutoTune
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        detector.prepare (sr);
        shifter.prepare (sr);
        defaultPeriod = (float) (sr / 200.0);
        periodSm = periodTarget = defaultPeriod;
        pCoef = onePoleCoef (15.0f, sr);
        bCoef = onePoleCoef (8.0f, sr);
        reset();
    }
    void reset()
    {
        detector.reset(); shifter.reset();
        shift = desired = 0.0f; blendSm = 0.0f; unvoicedCount = 0;
        hist[0] = hist[1] = hist[2] = 0.0f; histCount = 0; prevTarget = -1.0f;
        detMidi = tgtMidi = 0.0f;
    }
    int latencySamples() const { return shifter.latencySamples(); }

    // scaleIndex: see getScaleDefs, rootNote 0..11 (C=0), amount 0..1, retuneMs, mode
    void setParams (bool enabled, int scaleIndex, int rootNote, float amount01, float retuneMs, AutoTuneMode m)
    {
        on = enabled; scale = scaleIndex; root = ((rootNote % 12) + 12) % 12;
        amount = clampv (amount01, 0.0f, 1.0f); retune = std::max (retuneMs, 0.0f); mode = m;
    }

    void process (float* l, float* r, int n)
    {
        float tauMs;
        switch (mode)
        {
            case AutoTuneMode::Robotic: tauMs = std::max (0.5f, retune * 0.25f); break;
            case AutoTuneMode::Natural: tauMs = std::max (8.0f, retune * 1.5f); break;
            case AutoTuneMode::Smooth:  tauMs = retune * 3.0f + 25.0f; break;
            default:                    tauMs = 0.5f; break;
        }
        if (unvoicedCount > 3) tauMs = std::min (tauMs, 15.0f);
        const float coef = onePoleCoef (tauMs, sampleRate);

        for (int i = 0; i < n; ++i)
        {
            const bool fresh = detector.push (0.5f * (l[i] + r[i]));
            if (fresh) onDetection();

            shift = desired + coef * (shift - desired);
            periodSm = periodTarget + pCoef * (periodSm - periodTarget);
            const float blendTarget = (on && amount > 0.0f) ? smoothstep (0.02f, 0.08f, std::fabs (shift)) : 0.0f;
            blendSm = blendTarget + bCoef * (blendSm - blendTarget);

            const float ratio = std::exp2 (shift / 12.0f);
            float oL, oR;
            shifter.process (l[i], r[i], ratio, periodSm, blendSm, oL, oR);
            l[i] = oL; r[i] = oR;
        }
    }

    // UI / metering
    float getDetectedMidi() const { return detMidi; }   // 0 when unvoiced
    float getTargetMidi() const   { return tgtMidi; }
    float getShiftSemitones() const { return shift; }

    // Nearest allowed MIDI note (floating) for a detected MIDI pitch.
    float quantise (float midi, float hysteresisFrom) const
    {
        int num = 0;
        const ScaleDef* defs = getScaleDefs (num);
        const ScaleDef& sd = defs[clampv (scale, 0, num - 1)];
        bool allowed[12] = {};
        for (int i = 0; i < sd.count; ++i) allowed[(root + sd.degrees[i]) % 12] = true;

        if (hysteresisFrom >= 0.0f && std::fabs (midi - hysteresisFrom) < 0.65f
            && allowed[((int) std::lround (hysteresisFrom) % 12 + 12) % 12])
            return hysteresisFrom;

        float best = std::round (midi), bestDist = 1.0e9f;
        const int c = (int) std::floor (midi);
        for (int k = c - 3; k <= c + 4; ++k)
        {
            if (! allowed[((k % 12) + 12) % 12]) continue;
            const float dist = std::fabs ((float) k - midi);
            if (dist < bestDist) { bestDist = dist; best = (float) k; }
        }
        return best;
    }

private:
    void onDetection()
    {
        if (detector.isVoiced())
        {
            unvoicedCount = 0;
            const float m = PitchDetector::freqToMidi (detector.getFrequency());
            hist[2] = hist[1]; hist[1] = hist[0]; hist[0] = m;
            if (histCount < 3) ++histCount;
            const float med = histCount < 3 ? m : std::max (std::min (hist[0], hist[1]), std::min (std::max (hist[0], hist[1]), hist[2]));
            const bool hyst = (mode == AutoTuneMode::Natural || mode == AutoTuneMode::Smooth);
            const float target = quantise (med, hyst ? prevTarget : -1.0f);
            prevTarget = target;
            detMidi = med; tgtMidi = target;
            desired = on ? clampv ((target - med) * amount, -4.0f, 4.0f) : 0.0f;   // quantised corrections never exceed ~+-3 st
            periodTarget = detector.getPeriodSamples();
        }
        else
        {
            if (++unvoicedCount > 3) { desired = 0.0f; histCount = 0; prevTarget = -1.0f; detMidi = 0.0f; tgtMidi = 0.0f; }
        }
    }

    double sampleRate = 44100.0;
    PitchDetector detector;
    PsolaShifter shifter;
    bool on = true;
    int scale = 2, root = 0, unvoicedCount = 0, histCount = 0;
    float amount = 1.0f, retune = 20.0f;
    AutoTuneMode mode = AutoTuneMode::Robotic;
    float shift = 0.0f, desired = 0.0f, blendSm = 0.0f, periodSm = 220.0f, periodTarget = 220.0f, defaultPeriod = 220.0f;
    float pCoef = 0.0f, bCoef = 0.0f, hist[3] = { 0, 0, 0 }, prevTarget = -1.0f, detMidi = 0.0f, tgtMidi = 0.0f;
};
} // namespace msc
