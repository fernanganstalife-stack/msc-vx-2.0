#include "PresetManager.h"
#include "Assets.h"

namespace msc
{
PresetManager::PresetManager()
{
    for (const auto& file : assets::list ("preset_", ".json"))
    {
        const juce::var v = juce::JSON::parse (assets::text (file));
        if (auto* o = v.getDynamicObject())
        {
            Preset p;
            p.name = o->getProperty ("name").toString();
            p.description = o->getProperty ("description").toString();
            p.params = o->getProperty ("params");
            if (p.name.isNotEmpty()) presets.push_back (std::move (p));
        }
    }
    if (presets.empty()) presets.push_back ({ "Init", "Default settings", juce::var (new juce::DynamicObject()) });
}

juce::StringArray PresetManager::names() const
{
    juce::StringArray s;
    for (const auto& p : presets) s.add (p.name);
    return s;
}

void PresetManager::apply (juce::AudioProcessorValueTreeState& apvts, const Preset& preset)
{
    auto* obj = preset.params.getDynamicObject();
    if (obj == nullptr) return;

    for (const auto& nv : obj->getProperties())
    {
        auto* param = apvts.getParameter (nv.name.toString());
        if (param == nullptr) continue;

        float real = 0.0f;
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        {
            if (nv.value.isString()) real = (float) juce::jmax (0, choice->choices.indexOf (nv.value.toString()));
            else                     real = (float) (int) nv.value;
        }
        else
        {
            real = (float) (double) nv.value;     // bool vars convert to 0/1
        }
        param->setValueNotifyingHost (param->convertTo0to1 (real));
    }
}
} // namespace msc
