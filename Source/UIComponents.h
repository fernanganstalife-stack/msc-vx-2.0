#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "LookAndFeel.h"

namespace msc::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

// ------------------------------------------------------------------------------------------ Knob
class Knob : public juce::Component
{
public:
    Knob (APVTS& apvts, const char* id, const juce::String& label, int diameter, bool showLabel = true, bool showValue = true)
        : text (label), dia (diameter), drawLabel (showLabel), drawValue (showValue)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.setMouseDragSensitivity (170);
        addAndMakeVisible (slider);
        attachment = std::make_unique<APVTS::SliderAttachment> (apvts, id, slider);
        slider.onValueChange = [this] { repaint(); };
    }
    int diameter() const { return dia; }
    void setTextFunction (std::function<juce::String (double)> fn) { slider.textFromValueFunction = std::move (fn); repaint(); }

    void resized() override { slider.setBounds ((getWidth() - dia) / 2, 0, dia, dia); }
    void paint (juce::Graphics& g) override
    {
        const auto& th = themeOf (*this);
        auto r = getLocalBounds();
        if (drawLabel)
        {
            g.setColour (th.text);
            g.setFont (uiFont (12.5f));
            g.drawText (text, r.getX(), dia + 1, r.getWidth(), 14, juce::Justification::centred, false);
        }
        if (drawValue)
        {
            g.setColour (th.subtext);
            g.setFont (uiFont (12.5f, false));
            g.drawText (slider.getTextFromValue (slider.getValue()), r.getX(), dia + (drawLabel ? 15 : 1), r.getWidth(), 14,
                        juce::Justification::centred, false);
        }
    }
private:
    juce::Slider slider;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    juce::String text;
    int dia;
    bool drawLabel, drawValue;
};

// ------------------------------------------------------------------------------------------ pill toggle
class PillButton : public juce::ToggleButton
{
public:
    PillButton (APVTS& apvts, const char* id, const juce::String& label)
    {
        setButtonText (label);
        attachment = std::make_unique<APVTS::ButtonAttachment> (apvts, id, *this);
    }
private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

// ------------------------------------------------------------------------------------------ choice combo
class ChoiceBox : public juce::ComboBox
{
public:
    ChoiceBox (APVTS& apvts, const char* id, const juce::StringArray& items)
    {
        addItemList (items, 1);
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (apvts, id, *this);
    }
private:
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

// ------------------------------------------------------------------------------------------ delay division row
class DivisionRow : public juce::Component
{
public:
    explicit DivisionRow (APVTS& a)
        : apvts (a), names (getDelayDivisionNames()),
          attachment (*a.getParameter (ids::dlDiv), [this] (float v) { selected = (int) std::lround (v); repaint(); })
    {
        attachment.sendInitialUpdate();
    }
    void paint (juce::Graphics& g) override
    {
        const auto& th = themeOf (*this);
        const bool active = apvts.getRawParameterValue (ids::dlSync)->load() > 0.5f;
        const float cw = (float) getWidth() / (float) names.size();
        for (int i = 0; i < names.size(); ++i)
        {
            auto cell = juce::Rectangle<float> (cw * (float) i, 0.0f, cw, (float) getHeight()).reduced (1.5f);
            const bool on = i == selected;
            g.setColour (on ? th.primary.withAlpha (active ? 0.9f : 0.4f) : th.panel.darker (0.5f));
            g.fillRoundedRectangle (cell, 4.0f);
            g.setColour (th.panelBorder.withAlpha (active ? 0.9f : 0.4f));
            g.drawRoundedRectangle (cell, 4.0f, 1.2f);
            g.setColour ((on ? juce::Colours::white : th.subtext).withAlpha (active ? 1.0f : 0.5f));
            g.setFont (uiFont (13.0f));
            g.drawText (names[i], cell.toNearestInt(), juce::Justification::centred, false);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int idx = juce::jlimit (0, names.size() - 1, (int) ((float) e.x / ((float) getWidth() / (float) names.size())));
        attachment.setValueAsCompleteGesture ((float) idx);
    }
private:
    APVTS& apvts;
    juce::StringArray names;
    juce::ParameterAttachment attachment;
    int selected = 2;
};

// ------------------------------------------------------------------------------------------ level meter
class LevelMeter : public juce::Component
{
public:
    LevelMeter (const std::atomic<float>& src) : source (src) {}
    void tick()
    {
        const float p = source.load();
        level = std::max (p, level * 0.80f);
        hold = std::max (p, hold * 0.985f);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const auto& th = themeOf (*this);
        constexpr int segs = 24;
        const float segH = (float) getHeight() / (float) segs;
        const float db = msc::gainToDb (level), dbHold = msc::gainToDb (hold);
        g.setColour (th.background.darker (0.6f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 3.0f);
        for (int i = 0; i < segs; ++i)                       // i = 0 at the bottom
        {
            const float segDb = -48.0f + 2.0f * (float) i;
            juce::Colour c = segDb >= -6.0f ? juce::Colour (0xffff3b3b) : (segDb >= -18.0f ? juce::Colour (0xffffd23b) : juce::Colour (0xff3bff5a));
            const bool lit = db >= segDb;
            const auto r = juce::Rectangle<float> (2.0f, (float) getHeight() - segH * (float) (i + 1) + 1.0f, (float) getWidth() - 4.0f, segH - 2.0f);
            g.setColour (lit ? c : c.withAlpha (0.13f));
            g.fillRect (r);
        }
        if (dbHold > -48.0f)
        {
            const float y = (float) getHeight() * (1.0f - (juce::jlimit (-48.0f, 0.0f, dbHold) + 48.0f) / 48.0f);
            g.setColour (th.accent);
            g.fillRect (2.0f, y, (float) getWidth() - 4.0f, 2.0f);
        }
    }
private:
    const std::atomic<float>& source;
    float level = 0.0f, hold = 0.0f;
};

// ------------------------------------------------------------------------------------------ pitch display
class PitchDisplay : public juce::Component
{
public:
    explicit PitchDisplay (MSCVoxProcessor& p) : proc (p) { det.assign (kN, 0.0f); tgt.assign (kN, 0.0f); }
    void tick()
    {
        const float d = proc.detectedMidi.load(), t = proc.targetMidi.load();
        det.erase (det.begin()); det.push_back (d);
        tgt.erase (tgt.begin()); tgt.push_back (t);
        if (d > 0.0f) centre += 0.08f * (d - centre);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const auto& th = themeOf (*this);
        const auto r = getLocalBounds().toFloat();
        g.setColour (th.background.darker (0.5f));
        g.fillRoundedRectangle (r, 6.0f);
        const float range = 6.5f, h = r.getHeight();
        auto yOf = [&] (float midi) { return r.getCentreY() - (midi - centre) / range * (h * 0.5f); };
        const int base = (int) std::lround (centre);
        for (int k = -8; k <= 8; ++k)
        {
            const float y = yOf ((float) (base + k));
            if (y < r.getY() || y > r.getBottom()) continue;
            g.setColour (th.panelBorder.withAlpha (((base + k) % 12 == 0) ? 0.45f : 0.16f));
            g.drawHorizontalLine ((int) y, r.getX() + 2.0f, r.getRight() - 2.0f);
        }
        auto draw = [&] (const std::vector<float>& v, juce::Colour c, float thick)
        {
            juce::Path p; bool pen = false;
            for (int i = 0; i < kN; ++i)
            {
                if (v[(size_t) i] <= 0.0f) { pen = false; continue; }
                const float x = r.getX() + 3.0f + (r.getWidth() - 6.0f) * (float) i / (float) (kN - 1);
                const float y = juce::jlimit (r.getY() + 2.0f, r.getBottom() - 2.0f, yOf (v[(size_t) i]));
                if (! pen) { p.startNewSubPath (x, y); pen = true; } else p.lineTo (x, y);
            }
            g.setColour (c); g.strokePath (p, juce::PathStrokeType (thick));
        };
        draw (tgt, th.highlight, 2.0f);
        draw (det, th.accent, 1.6f);
        g.setColour (th.panelBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.4f);
    }
private:
    static constexpr int kN = 120;
    MSCVoxProcessor& proc;
    std::vector<float> det, tgt;
    float centre = 60.0f;
};

// ------------------------------------------------------------------------------------------ EQ curve
class EqDisplay : public juce::Component
{
public:
    explicit EqDisplay (MSCVoxProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        const auto& th = themeOf (*this);
        const auto r = getLocalBounds().toFloat();
        g.setColour (th.background.darker (0.5f));
        g.fillRoundedRectangle (r, 5.0f);
        const float lo = proc.apvts.getRawParameterValue (ids::eqLow)->load();
        const float mi = proc.apvts.getRawParameterValue (ids::eqMid)->load();
        const float hi = proc.apvts.getRawParameterValue (ids::eqHigh)->load();
        const bool on = proc.apvts.getRawParameterValue (ids::eqOn)->load() > 0.5f;
        auto xOf = [&] (double f) { return r.getX() + r.getWidth() * (float) (std::log (f / 20.0) / std::log (1000.0)); };
        auto yOf = [&] (double db) { return r.getCentreY() - (float) db / 14.0f * r.getHeight() * 0.5f; };
        g.setColour (th.panelBorder.withAlpha (0.25f));
        g.drawHorizontalLine ((int) r.getCentreY(), r.getX(), r.getRight());
        for (double f : { 100.0, 1000.0, 10000.0 }) g.drawVerticalLine ((int) xOf (f), r.getY(), r.getBottom());
        juce::Path curve, fill;
        for (int px = 0; px <= (int) r.getWidth(); px += 2)
        {
            const double f = 20.0 * std::pow (1000.0, (double) px / (double) r.getWidth());
            const float y = yOf (on ? ParametricEQ_responseDb (f, lo, mi, hi) : 0.0);
            if (px == 0) { curve.startNewSubPath (r.getX(), y); fill.startNewSubPath (r.getX(), r.getBottom()); fill.lineTo (r.getX(), y); }
            else { curve.lineTo (r.getX() + (float) px, y); fill.lineTo (r.getX() + (float) px, y); }
        }
        fill.lineTo (r.getRight(), r.getBottom()); fill.closeSubPath();
        g.setColour (th.primary.withAlpha (0.18f)); g.fillPath (fill);
        g.setColour (th.accent); g.strokePath (curve, juce::PathStrokeType (2.0f));
        const double nodeF[3] = { msc::ParametricEQ::kLowHz, msc::ParametricEQ::kMidHz, msc::ParametricEQ::kHighHz };
        const float nodeDb[3] = { lo, mi, hi };
        for (int i = 0; i < 3; ++i)
        {
            g.setColour (th.highlight);
            g.fillEllipse (xOf (nodeF[i]) - 4.0f, yOf (on ? nodeDb[i] : 0.0) - 4.0f, 8.0f, 8.0f);
        }
        g.setColour (th.panelBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.2f);
    }
private:
    static double ParametricEQ_responseDb (double f, float lo, float mi, float hi) { return msc::ParametricEQ::responseDb (44100.0, f, lo, mi, hi); }
    MSCVoxProcessor& proc;
};

// ------------------------------------------------------------------------------------------ live output waveform
class ScopeDisplay : public juce::Component
{
public:
    explicit ScopeDisplay (MSCVoxProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        const auto& th = themeOf (*this);
        const auto r = getLocalBounds().toFloat();
        g.setColour (th.background.darker (0.5f));
        g.fillRoundedRectangle (r, 5.0f);
        const int pos = proc.scopePos.load();
        const int bars = 120;
        const float bw = (r.getWidth() - 6.0f) / (float) bars;
        for (int i = 0; i < bars; ++i)
        {
            const int idx = (pos + 1 + (i * MSCVoxProcessor::kScopeSize) / bars) % MSCVoxProcessor::kScopeSize;
            const float v = juce::jlimit (-1.0f, 1.0f, proc.scope[(size_t) idx].load() * 2.5f);
            const float hgt = std::max (1.5f, std::fabs (v) * r.getHeight() * 0.9f);
            g.setColour (th.primary.withAlpha (0.45f + 0.5f * std::fabs (v)));
            g.fillRect (r.getX() + 3.0f + bw * (float) i, r.getCentreY() - hgt * 0.5f, std::max (1.0f, bw - 1.0f), hgt);
        }
        g.setColour (th.panelBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.2f);
    }
private:
    MSCVoxProcessor& proc;
};

// ------------------------------------------------------------------------------------------ compressor gain-reduction history
class GrDisplay : public juce::Component
{
public:
    explicit GrDisplay (MSCVoxProcessor& p) : proc (p) { hist.assign (kN, 0.0f); }
    void tick() { hist.erase (hist.begin()); hist.push_back (proc.compGainReductionDb.load()); repaint(); }
    void paint (juce::Graphics& g) override
    {
        const auto& th = themeOf (*this);
        const auto r = getLocalBounds().toFloat();
        g.setColour (th.background.darker (0.5f));
        g.fillRoundedRectangle (r, 5.0f);
        const float bw = (r.getWidth() - 6.0f) / (float) kN;
        for (int i = 0; i < kN; ++i)
        {
            const float h = juce::jlimit (0.0f, 1.0f, -hist[(size_t) i] / 18.0f) * (r.getHeight() - 4.0f);
            g.setColour (th.highlight.withAlpha (0.8f));
            g.fillRect (r.getX() + 3.0f + bw * (float) i, r.getY() + 2.0f, std::max (1.0f, bw), h);
        }
        g.setColour (th.subtext);
        g.setFont (uiFont (11.0f));
        g.drawText ("GAIN REDUCTION", getLocalBounds().withTrimmedTop (getHeight() - 14), juce::Justification::centred, false);
        g.setColour (th.panelBorder);
        g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.2f);
    }
private:
    static constexpr int kN = 70;
    MSCVoxProcessor& proc;
    std::vector<float> hist;
};

// ------------------------------------------------------------------------------------------ preset list
class PresetList : public juce::Component
{
public:
    explicit PresetList (MSCVoxProcessor& p) : proc (p) {}
    static constexpr int rowH = 28;
    void paint (juce::Graphics& g) override
    {
        const auto& th = themeOf (*this);
        g.setColour (th.background.darker (0.5f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
        const int cur = proc.getCurrentProgram();
        for (int i = 0; i < proc.getNumPrograms(); ++i)
        {
            auto row = juce::Rectangle<int> (3, 3 + i * rowH, getWidth() - 6, rowH - 2);
            if (i == cur)
            {
                g.setGradientFill (juce::ColourGradient (th.primary, 0, (float) row.getY(), th.primary.darker (0.6f), 0, (float) row.getBottom(), false));
                g.fillRoundedRectangle (row.toFloat(), 4.0f);
                g.setColour (th.accent);
                g.fillPath (makeStar ({ (float) row.getRight() - 16.0f, (float) row.getCentreY() }, 5, 8.0f, 3.5f));
            }
            g.setColour (i == cur ? juce::Colours::white : th.text);
            g.setFont (uiFont (16.0f));
            g.drawText (proc.getProgramName (i).toUpperCase(), row.withTrimmedLeft (10), juce::Justification::centredLeft, true);
            g.setColour (th.panelBorder.withAlpha (0.25f));
            g.drawHorizontalLine (row.getBottom() + 1, 4.0f, (float) getWidth() - 4.0f);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int row = (e.y - 3) / rowH;
        if (row >= 0 && row < proc.getNumPrograms()) { proc.setCurrentProgram (row); repaint(); }
    }
private:
    MSCVoxProcessor& proc;
};

// ------------------------------------------------------------------------------------------ preset info
class PresetInfo : public juce::Component
{
public:
    explicit PresetInfo (MSCVoxProcessor& p) : proc (p) {}
    void paint (juce::Graphics& g) override
    {
        const auto& th = themeOf (*this);
        auto r = getLocalBounds().reduced (14, 6);
        g.setColour (th.text);
        g.setFont (uiFont (16.0f));
        g.drawText ("PRESET INFO:", r.removeFromTop (22), juce::Justification::centredLeft, false);
        const auto& pr = proc.presets.get (proc.getCurrentProgram());
        g.setColour (th.accent);
        g.setFont (uiFont (15.0f));
        g.drawText (pr.name, r.removeFromTop (20), juce::Justification::centredLeft, true);
        g.setColour (th.subtext);
        g.setFont (uiFont (11.0f, false));
        g.drawFittedText (pr.description, r, juce::Justification::topLeft, 2);
    }
private:
    MSCVoxProcessor& proc;
};

// ------------------------------------------------------------------------------------------ theme thumbnails
class ThemeSwatches : public juce::Component
{
public:
    ThemeSwatches (const ThemeManager& tm, std::function<int()> getCurrent, std::function<void (int)> onPick)
        : themes (tm), current (std::move (getCurrent)), pick (std::move (onPick)) {}
    void paint (juce::Graphics& g) override
    {
        const auto& th = themeOf (*this);
        for (int i = 0; i < themes.size(); ++i)
        {
            const auto cell = cellRect (i);
            const auto& img = themes.get (i).banner;
            if (img.isValid())
                g.drawImage (img, cell.getX(), cell.getY(), cell.getWidth(), cell.getHeight(), 620, 0, 120, 151);
            g.setColour (i == current() ? th.accent : themes.get (i).panelBorder.withAlpha (0.8f));
            g.drawRoundedRectangle (cell.toFloat().reduced (0.5f), 5.0f, i == current() ? 3.0f : 1.5f);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < themes.size(); ++i)
            if (cellRect (i).contains (e.getPosition())) { pick (i); repaint(); return; }
    }
private:
    juce::Rectangle<int> cellRect (int i) const { return { i * 52, 2, 46, getHeight() - 4 }; }
    const ThemeManager& themes;
    std::function<int()> current;
    std::function<void (int)> pick;
};

// ------------------------------------------------------------------------------------------ gear icon button
class GearButton : public juce::Button
{
public:
    GearButton() : juce::Button ("settings") {}
    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto& th = themeOf (*this);
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setGradientFill (juce::ColourGradient (th.panel.brighter (down ? 0.1f : (highlighted ? 0.5f : 0.3f)), r.getX(), r.getY(),
                                                 th.panel.darker (0.6f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (th.panelBorder);
        g.drawRoundedRectangle (r, 6.0f, 1.6f);
        const auto c = r.getCentre();
        juce::Path gear = makeStar (c, 8, std::min (r.getHeight() * 0.38f, 20.0f), std::min (r.getHeight() * 0.28f, 14.0f));
        gear.addEllipse (c.x - 6.0f, c.y - 6.0f, 12.0f, 12.0f);
        gear.setUsingNonZeroWinding (false);
        g.setColour (th.text);
        g.fillPath (gear);
    }
};
} // namespace msc::ui
