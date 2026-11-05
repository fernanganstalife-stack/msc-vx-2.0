#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Assets.h"

namespace msc
{
struct Theme
{
    juce::String name;
    juce::Colour background, panel, panelBorder, primary, accent, highlight, text, subtext, knobBody;
    juce::Image banner, singer, guitarist, deco, title;
};

// Interface themes are JSON files in /Resources/Themes (colours + artwork file names).
class ThemeManager
{
public:
    ThemeManager()
    {
        for (const auto& file : assets::list ("theme_", ".json"))
        {
            const juce::var v = juce::JSON::parse (assets::text (file));
            auto* o = v.getDynamicObject();
            if (o == nullptr) continue;
            Theme t;
            t.name        = o->getProperty ("name").toString();
            t.background  = colour (o, "background");
            t.panel       = colour (o, "panel");
            t.panelBorder = colour (o, "panelBorder");
            t.primary     = colour (o, "primary");
            t.accent      = colour (o, "accent");
            t.highlight   = colour (o, "highlight");
            t.text        = colour (o, "text");
            t.subtext     = colour (o, "subtext");
            t.knobBody    = colour (o, "knobBody");
            const juce::var imgs = o->getProperty ("images");
            t.banner    = assets::image (imgs["banner"].toString());
            t.singer    = assets::image (imgs["singer"].toString());
            t.guitarist = assets::image (imgs["guitarist"].toString());
            t.deco      = assets::image (imgs["deco"].toString());
            t.title     = assets::image (imgs["title"].toString());
            themes.push_back (std::move (t));
        }
        if (themes.empty()) themes.push_back (Theme { "Fallback", juce::Colours::black, juce::Colour (0xff180a38), juce::Colour (0xff7a4df5),
                                                        juce::Colour (0xff9b6bff), juce::Colour (0xffd9ccff), juce::Colour (0xffff70e0),
                                                        juce::Colours::white, juce::Colour (0xffb7a6ee), juce::Colour (0xff3a2370), {}, {}, {}, {}, {} });
    }
    int size() const { return (int) themes.size(); }
    const Theme& get (int i) const { return themes[(size_t) juce::jlimit (0, size() - 1, i)]; }
private:
    static juce::Colour colour (juce::DynamicObject* o, const char* key)
    {
        return juce::Colour::fromString ("ff" + o->getProperty (key).toString().removeCharacters ("#"));
    }
    std::vector<Theme> themes;
};
} // namespace msc
