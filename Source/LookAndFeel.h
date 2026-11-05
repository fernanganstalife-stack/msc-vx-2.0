#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ThemeManager.h"

namespace msc
{
inline juce::Font uiFont (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

inline juce::Path makeStar (juce::Point<float> c, int points, float outer, float inner)
{
    juce::Path p;
    p.addStar (c, points, inner, outer);
    return p;
}

// Custom look: neon rotary knobs, pill toggles, dark combo boxes, bar slider. Colours come from the active Theme.
class MscLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit MscLookAndFeel (const Theme* t) { setTheme (t); }

    void setTheme (const Theme* t)
    {
        theme = t;
        setColour (juce::ComboBox::textColourId, theme->text);
        setColour (juce::ComboBox::backgroundColourId, theme->panel.darker (0.4f));
        setColour (juce::ComboBox::outlineColourId, theme->panelBorder);
        setColour (juce::PopupMenu::backgroundColourId, theme->background.brighter (0.05f));
        setColour (juce::PopupMenu::textColourId, theme->text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme->primary.darker (0.3f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextButton::textColourOffId, theme->text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::AlertWindow::backgroundColourId, theme->background.brighter (0.05f));
        setColour (juce::AlertWindow::textColourId, theme->text);
        setColour (juce::AlertWindow::outlineColourId, theme->panelBorder);
    }
    const Theme& getTheme() const { return *theme; }

    // ------------------------------------------------------------------ rotary knob
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto& th = *theme;
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const bool enabled = s.isEnabled();

        // glow
        if (enabled && pos > 0.001f)
        {
            g.setColour (th.primary.withAlpha (0.10f + 0.12f * pos));
            g.fillEllipse (bounds.expanded (3.0f));
        }
        // outer ring
        g.setColour (th.panelBorder.withAlpha (0.55f));
        g.drawEllipse (bounds, 1.5f);

        // value arc
        const float arcR = radius - 3.0f;
        juce::Path track, value;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (th.panel.brighter (0.35f));
        g.strokePath (track, juce::PathStrokeType (2.5f));
        g.setColour (enabled ? th.primary : th.primary.withAlpha (0.3f));
        g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // body
        const float bodyR = radius * 0.72f;
        juce::ColourGradient grad (th.knobBody.brighter (0.9f), c.x - bodyR * 0.4f, c.y - bodyR * 0.5f,
                                   th.knobBody.darker (0.9f), c.x + bodyR * 0.6f, c.y + bodyR * 0.8f, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour (th.accent.withAlpha (0.35f));
        g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

        // pointer
        const float s1 = std::sin (angle), c1 = std::cos (angle);
        g.setColour (enabled ? th.accent : th.accent.withAlpha (0.3f));
        g.drawLine (c.x + s1 * bodyR * 0.25f, c.y - c1 * bodyR * 0.25f,
                    c.x + s1 * bodyR * 0.92f, c.y - c1 * bodyR * 0.92f, std::max (2.0f, radius * 0.09f));
    }

    // ------------------------------------------------------------------ pills (ON, SYNC, DOTTED ...)
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool down) override
    {
        const auto& th = *theme;
        const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        const float corner = r.getHeight() * 0.35f;

        if (on)
        {
            g.setColour (th.primary.withAlpha (0.25f));
            g.fillRoundedRectangle (r.expanded (1.5f), corner + 1.5f);
            g.setGradientFill (juce::ColourGradient (th.primary.brighter (0.25f), r.getX(), r.getY(),
                                                     th.primary.darker (0.55f), r.getX(), r.getBottom(), false));
        }
        else
            g.setColour (th.panel.darker (0.5f));
        g.fillRoundedRectangle (r, corner);
        g.setColour ((on ? th.accent : th.panelBorder).withAlpha (highlighted || down ? 1.0f : 0.75f));
        g.drawRoundedRectangle (r, corner, 1.4f);

        g.setColour (on ? juce::Colours::white : th.subtext.withAlpha (0.8f));
        g.setFont (uiFont (std::min (15.0f, r.getHeight() * 0.62f)));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, false);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down) override
    {
        const auto& th = *theme;
        const auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        g.setGradientFill (juce::ColourGradient (th.panel.brighter (down ? 0.1f : (highlighted ? 0.5f : 0.3f)), r.getX(), r.getY(),
                                                 th.panel.darker (0.6f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (th.panelBorder.withAlpha (highlighted ? 1.0f : 0.8f));
        g.drawRoundedRectangle (r, 6.0f, 1.6f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return uiFont (std::min (20.0f, (float) h * 0.5f)); }

    // ------------------------------------------------------------------ combo box
    void drawComboBox (juce::Graphics& g, int w, int h, bool down, int, int, int, int, juce::ComboBox& box) override
    {
        const auto& th = *theme;
        const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
        g.setGradientFill (juce::ColourGradient (th.panel.darker (0.2f), 0, 0, th.panel.darker (0.8f), 0, (float) h, false));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (th.panelBorder.withAlpha (down || box.isMouseOver() ? 1.0f : 0.8f));
        g.drawRoundedRectangle (r, 6.0f, 1.5f);
        juce::Path arrow;
        const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 5.0f, ay - 3.0f, ax + 5.0f, ay - 3.0f, ax, ay + 4.0f);
        g.setColour (th.accent);
        g.fillPath (arrow);
    }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return uiFont (16.0f); }
    juce::Font getPopupMenuFont() override { return uiFont (16.0f); }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    // ------------------------------------------------------------------ horizontal bar slider (master mix)
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override
    {
        const auto& th = *theme;
        const auto track = juce::Rectangle<float> ((float) x, (float) y + h * 0.5f - 9.0f, (float) w, 18.0f);
        g.setColour (th.background.darker (0.5f));
        g.fillRoundedRectangle (track, 4.0f);
        g.setColour (th.panelBorder);
        g.drawRoundedRectangle (track, 4.0f, 1.5f);
        auto fill = track.reduced (3.0f);
        fill.setRight (std::max (fill.getX() + 2.0f, sliderPos));
        g.setGradientFill (juce::ColourGradient (th.primary.brighter (0.2f), fill.getX(), 0, th.highlight, fill.getRight(), 0, false));
        g.fillRoundedRectangle (fill, 3.0f);
        g.setColour (th.accent);
        g.fillRoundedRectangle (sliderPos - 5.0f, track.getY() - 3.0f, 10.0f, track.getHeight() + 6.0f, 3.0f);
    }

private:
    const Theme* theme = nullptr;
};

inline const Theme& themeOf (const juce::Component& c)
{
    return static_cast<MscLookAndFeel&> (c.getLookAndFeel()).getTheme();
}
} // namespace msc
