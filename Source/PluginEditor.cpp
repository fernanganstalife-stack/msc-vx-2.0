#include "PluginEditor.h"
#include "UIComponents.h"

using namespace msc;
using msc::ui::Knob;
using msc::ui::PillButton;
using msc::ui::ChoiceBox;

namespace
{
constexpr int kDesignW = 1536, kDesignH = 1024;

// Animated artwork strip: static banner + the two characters redrawn with level-driven motion and twinkling stars.
class Banner : public juce::Component
{
public:
    explicit Banner (MSCVoxProcessor& p) : proc (p)
    {
        juce::Random rng (2000);
        for (int i = 0; i < 16; ++i)
            sparkles.push_back ({ 20.0f + rng.nextFloat() * 1060.0f, 15.0f + rng.nextFloat() * 255.0f, 4.0f + rng.nextFloat() * 6.0f, rng.nextFloat() * 6.28f });
    }
    void tick()
    {
        const float p = proc.outPeak.load();
        level = std::max (p, level * 0.88f);
        time += 1.0f / 30.0f;
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        const auto& th = themeOf (*this);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        if (th.banner.isValid()) g.drawImageAt (th.banner, 0, 0);
        const float lv = juce::jlimit (0.0f, 1.0f, level * 1.5f);

        // stage glow behind the singer, pulses with the output level
        g.setGradientFill (juce::ColourGradient (th.primary.withAlpha (0.05f + 0.22f * lv), 700.0f, 110.0f,
                                                 th.primary.withAlpha (0.0f), 700.0f, 280.0f, true));
        g.fillRect (getLocalBounds());

        // characters (feathered RGBA crops) with bobbing / zoom
        drawChar (g, th.singer, 400.0f, 0.0f, 0.0f, -3.0f * (0.35f + lv) * std::sin (time * 6.0f), 1.012f + 0.02f * lv);
        drawChar (g, th.guitarist, 780.0f, 3.0f * (0.3f + lv) * std::sin (time * 4.3f + 1.0f), 0.0f, 0.0f, 1.015f + 0.015f * lv);

        // twinkling stars
        for (auto& s : sparkles)
        {
            const float a = std::max (0.0f, std::sin (time * 2.2f + s.phase));
            g.setColour (th.accent.withAlpha (0.15f + 0.75f * a * (0.5f + lv)));
            g.fillPath (makeStar ({ s.x, s.y }, 4, s.size * (0.6f + 0.6f * a), s.size * 0.18f));
        }
    }
private:
    struct Sparkle { float x, y, size, phase; };
    void drawChar (juce::Graphics& g, const juce::Image& img, float x0, float dx, float, float dy, float zoom)
    {
        if (! img.isValid()) return;
        const float w = (float) img.getWidth(), h = (float) img.getHeight();
        g.drawImageTransformed (img, juce::AffineTransform::scale (zoom, zoom, w * 0.5f, h * 0.5f).translated (x0 + dx, dy));
    }
    MSCVoxProcessor& proc;
    std::vector<Sparkle> sparkles;
    float level = 0.0f, time = 0.0f;
};
} // namespace

// ===================================================================================================== Content
class MSCVoxEditor::Content : public juce::Component, private juce::Timer
{
public:
    Content (MSCVoxProcessor& p, ThemeManager& tm, MscLookAndFeel& l, juce::Component& ed)
        : proc (p), themes (tm), laf (l), editor (ed),
          banner (p), pitch (p), eqView (p), scopeDist (p), scopeSat (p), scopeChorus (p), scopeWide (p), scopeDeEss (p), grView (p),
          meterIn (p.inPeak), meterOut (p.outPeak), presetList (p), presetInfo (p),
          swatches (tm, [this] { return proc.getThemeIndex(); }, [this] (int i) { applyTheme (i); })
    {
        auto& a = proc.apvts;

        addAndMakeVisible (banner);
        banner.setBounds (0, 55, 1100, 285);

        // ---------------------------------------------------------------- panel frames + ON switches
        panels = {
            { "AUTO-TUNE",       { 205, 340, 1135, 130 }, ids::atOn },
            { "COMPRESSOR",      { 135, 480, 265, 225 },  ids::cpOn },
            { "DE-ESSER",        { 408, 480, 247, 225 },  ids::dsOn },
            { "EQ",              { 665, 480, 245, 225 },  ids::eqOn },
            { "AIR / BRIGHTNESS",{ 920, 480, 210, 225 },  ids::airOn },
            { "DISTORTION",      { 1140, 480, 260, 225 }, ids::diOn },
            { "SATURATION",      { 135, 715, 237, 197 },  ids::saOn },
            { "REVERB",          { 385, 715, 285, 197 },  ids::rvOn },
            { "DELAY",           { 680, 715, 290, 197 },  ids::dlOn },
            { "CHORUS",          { 980, 715, 230, 197 },  ids::chOn },
            { "STEREO WIDENER",  { 1220, 715, 180, 197 }, ids::swOn },
        };
        for (auto& pd : panels)
        {
            auto& b = addPill (pd.onId, "ON");
            if (pd.onId == juce::String (ids::atOn)) b.setBounds (pd.r.getX() + 16, pd.r.getY() + 40, 62, 36);
            else                                       b.setBounds (pd.r.getX() + 10, pd.r.getY() + 8, 44, 24);
        }

        // ---------------------------------------------------------------- auto-tune
        addChoice (ids::atScale, getScaleNames()).setBounds (358, 415, 130, 34);
        addChoice (ids::atNote, getNoteNames()).setBounds (527, 415, 80, 34);
        addChoice (ids::atMode, getAutoTuneModeNames()).setBounds (917, 408, 110, 34);
        placeKnob (addKnob (ids::atAmount, "AMOUNT", 78), 697, 405);
        placeKnob (addKnob (ids::atSpeed, "RETUNE SPEED", 44), 830, 410);
        addAndMakeVisible (pitch); pitch.setBounds (1045, 365, 255, 90);

        // ---------------------------------------------------------------- compressor
        placeKnob (addKnob (ids::cpThresh, "THRESH", 44), 187, 548);
        placeKnob (addKnob (ids::cpRatio, "RATIO", 44), 266, 548);
        placeKnob (addKnob (ids::cpRelease, "RELEASE", 44), 345, 548);
        placeKnob (addKnob (ids::cpMakeup, "MAKEUP", 40), 320, 640);
        addAndMakeVisible (grView); grView.setBounds (148, 628, 125, 68);

        // ---------------------------------------------------------------- de-esser
        placeKnob (addKnob (ids::dsThresh, "THRESH", 44), 455, 548);
        placeKnob (addKnob (ids::dsFreq, "FREQ", 44), 532, 548);
        placeKnob (addKnob (ids::dsReduction, "REDUCTION", 44), 607, 548);
        placeKnob (addKnob (ids::dsAmount, "AMOUNT", 40), 600, 640);
        addAndMakeVisible (scopeDeEss); scopeDeEss.setBounds (418, 628, 140, 68);

        // ---------------------------------------------------------------- EQ
        placeKnob (addKnob (ids::eqLow, "LOW", 44), 707, 548);
        placeKnob (addKnob (ids::eqMid, "MID", 44), 785, 548);
        placeKnob (addKnob (ids::eqHigh, "HIGH", 44), 862, 548);
        addAndMakeVisible (eqView); eqView.setBounds (675, 626, 225, 72);

        // ---------------------------------------------------------------- air
        placeKnob (addKnob (ids::airAmount, "AMOUNT", 90), 1025, 575);

        // ---------------------------------------------------------------- distortion
        placeKnob (addKnob (ids::diDrive, "DRIVE", 44), 1185, 548);
        placeKnob (addKnob (ids::diTone, "TONE", 44), 1269, 548);
        placeKnob (addKnob (ids::diMix, "MIX", 44), 1353, 548);
        addAndMakeVisible (scopeDist); scopeDist.setBounds (1150, 628, 240, 68);

        // ---------------------------------------------------------------- saturation
        placeKnob (addKnob (ids::saDrive, "DRIVE", 44), 183, 780);
        placeKnob (addKnob (ids::saMix, "MIX", 44), 249, 780);
        addChoice (ids::saType, getSaturationTypeNames()).setBounds (294, 790, 72, 26);
        addAndMakeVisible (scopeSat); scopeSat.setBounds (146, 850, 218, 52);

        // ---------------------------------------------------------------- reverb
        placeKnob (addKnob (ids::rvSize, "SIZE", 40), 413, 778);
        placeKnob (addKnob (ids::rvDecay, "DECAY", 40), 466, 778);
        placeKnob (addKnob (ids::rvDamping, "DAMPING", 40), 519, 778);
        placeKnob (addKnob (ids::rvPredelay, "PRE-DELAY", 40), 576, 778);
        placeKnob (addKnob (ids::rvMix, "MIX", 40), 631, 778);
        addChoice (ids::rvType, getReverbTypeNames()).setBounds (400, 855, 140, 34);

        // ---------------------------------------------------------------- delay
        auto& dt = addKnob (ids::dlTime, "TIME", 40);
        delayTime = &dt;
        dt.setTextFunction ([this] (double ms)
        {
            auto& ap = proc.apvts;
            if (ap.getRawParameterValue (ids::dlSync)->load() < 0.5f) return juce::String ((int) std::lround (ms)) + " ms";
            auto s = getDelayDivisionNames()[(int) ap.getRawParameterValue (ids::dlDiv)->load()];
            if (ap.getRawParameterValue (ids::dlDotted)->load() > 0.5f) s << "D";
            else if (ap.getRawParameterValue (ids::dlTriplet)->load() > 0.5f) s << "T";
            return s;
        });
        placeKnob (dt, 724, 776);
        placeKnob (addKnob (ids::dlFeedback, "FEEDBACK", 40), 793, 776);
        placeKnob (addKnob (ids::dlMix, "MIX", 40), 869, 776);
        placeKnob (addKnob (ids::dlTone, "TONE", 40), 938, 776);
        addPill (ids::dlSync, "SYNC").setBounds (690, 846, 62, 22);
        addPill (ids::dlStereo, "STEREO").setBounds (758, 846, 74, 22);
        addPill (ids::dlDotted, "DOTTED").setBounds (838, 846, 66, 22);
        addPill (ids::dlTriplet, "TRIPLET").setBounds (908, 846, 56, 22);
        divisions = std::make_unique<ui::DivisionRow> (a);
        addAndMakeVisible (*divisions); divisions->setBounds (688, 874, 274, 26);

        // ---------------------------------------------------------------- chorus
        placeKnob (addKnob (ids::chRate, "RATE", 40), 1012, 778);
        placeKnob (addKnob (ids::chDepth, "DEPTH", 40), 1068, 778);
        placeKnob (addKnob (ids::chWidth, "WIDTH", 40), 1124, 778);
        placeKnob (addKnob (ids::chMix, "MIX", 40), 1180, 778);
        addAndMakeVisible (scopeChorus); scopeChorus.setBounds (990, 858, 212, 40);

        // ---------------------------------------------------------------- stereo widener
        placeKnob (addKnob (ids::swWidth, "WIDTH", 44), 1283, 780);
        placeKnob (addKnob (ids::swMix, "MIX", 44), 1349, 780);
        addAndMakeVisible (scopeWide); scopeWide.setBounds (1230, 858, 160, 40);

        // ---------------------------------------------------------------- meters + gain
        addAndMakeVisible (meterIn);  meterIn.setBounds (36, 445, 26, 250);
        addAndMakeVisible (meterOut); meterOut.setBounds (1441, 445, 26, 250);
        placeKnob (addKnob (ids::inGain, "INPUT GAIN", 56), 65, 740);
        placeKnob (addKnob (ids::outGain, "OUTPUT GAIN", 56), 1468, 740);
        placeKnob (addKnob (ids::masterMix, "WET / DRY", 46), 1468, 872);

        // ---------------------------------------------------------------- header
        addAndMakeVisible (prevBtn); prevBtn.setButtonText ("<");
        addAndMakeVisible (nextBtn); nextBtn.setButtonText (">");
        prevBtn.setBounds (562, 12, 42, 34); nextBtn.setBounds (936, 12, 42, 34);
        prevBtn.onClick = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() + proc.getNumPrograms() - 1) % proc.getNumPrograms()); };
        nextBtn.onClick = [this] { proc.setCurrentProgram ((proc.getCurrentProgram() + 1) % proc.getNumPrograms()); };
        addAndMakeVisible (presetBox);
        presetBox.addItemList (proc.presets.names(), 1);
        presetBox.setSelectedItemIndex (proc.getCurrentProgram(), juce::dontSendNotification);
        presetBox.onChange = [this] { if (presetBox.getSelectedItemIndex() >= 0) proc.setCurrentProgram (presetBox.getSelectedItemIndex()); };
        presetBox.setBounds (612, 10, 316, 38);

        addAndMakeVisible (saveBtn); saveBtn.setButtonText ("SAVE"); saveBtn.setBounds (1040, 8, 108, 42);
        addAndMakeVisible (loadBtn); loadBtn.setButtonText ("LOAD"); loadBtn.setBounds (1160, 8, 108, 42);
        addAndMakeVisible (gearBtn); gearBtn.setBounds (1296, 8, 62, 42);
        saveBtn.onClick = [this] { saveToFile(); };
        loadBtn.onClick = [this] { loadFromFile(); };
        gearBtn.onClick = [this] { showSettingsMenu(); };

        addAndMakeVisible (presetList); presetList.setBounds (1112, 100, 304, 244);
        addAndMakeVisible (presetInfo); presetInfo.setBounds (590, 936, 352, 66);

        // ---------------------------------------------------------------- bottom row
        addAndMakeVisible (themeBox);
        for (int i = 0; i < themes.size(); ++i) themeBox.addItem (themes.get (i).name, i + 1);
        themeBox.setSelectedItemIndex (juce::jlimit (0, themes.size() - 1, proc.getThemeIndex()), juce::dontSendNotification);
        themeBox.onChange = [this] { applyTheme (themeBox.getSelectedItemIndex()); };
        themeBox.setBounds (12, 940, 226, 44);
        addAndMakeVisible (swatches); swatches.setBounds (258, 934, 312, 64);

        masterKnob = std::make_unique<Knob> (a, ids::masterMix, "MASTER MIX", 54, false, false);
        addAndMakeVisible (*masterKnob); masterKnob->setBounds (970, 942, 76, 54);
        masterBar.setSliderStyle (juce::Slider::LinearHorizontal);
        masterBar.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible (masterBar); masterBar.setBounds (1066, 975, 240, 28);
        masterBarAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (a, ids::masterMix, masterBar);

        lastPreset = proc.getCurrentProgram();
        startTimerHz (30);
    }

    ~Content() override { stopTimer(); }

    void paint (juce::Graphics& g) override
    {
        const auto& th = themeOf (*this);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

        // background
        g.setGradientFill (juce::ColourGradient (th.background.brighter (0.15f), 0, 0, th.background.darker (0.6f), 0, (float) kDesignH, false));
        g.fillAll();
        juce::Random rng (77);
        for (int i = 0; i < 40; ++i)
        {
            g.setColour (th.primary.withAlpha (0.05f + rng.nextFloat() * 0.07f));
            g.fillPath (makeStar ({ rng.nextFloat() * 1536.0f, 340.0f + rng.nextFloat() * 684.0f }, 5, 8.0f + rng.nextFloat() * 22.0f, 4.0f));
        }

        // header strip
        g.setColour (th.background.darker (0.4f));
        g.fillRect (0, 0, kDesignW, 55);
        g.setColour (th.panelBorder.withAlpha (0.8f));
        g.fillRect (0, 54, kDesignW, 2);
        if (th.title.isValid()) g.drawImage (th.title, 75, 6, 230, 44, 0, 0, th.title.getWidth(), th.title.getHeight());
        g.setColour (th.text);
        g.setFont (uiFont (13.0f));
        g.drawText ("2.0", 305, 16, 40, 20, juce::Justification::centredLeft, false);

        // decorative strip on the right of the banner
        if (th.deco.isValid()) g.drawImageAt (th.deco, 1425, 55);

        // preset panel
        drawPanel (g, th, { 1108, 70, 312, 280 }, "PRESETS", nullptr);

        // module panels
        for (auto& pd : panels)
            drawPanel (g, th, pd.r, pd.title, pd.onId);

        // side meter panels
        drawPlain (g, th, { 10, 400, 118, 430 });
        drawPlain (g, th, { 1408, 400, 118, 430 });
        drawPlain (g, th, { 1408, 835, 118, 80 });
        drawPlain (g, th, { 10, 925, 240, 85 });
        drawPlain (g, th, { 250, 925, 328, 85 });
        drawPlain (g, th, { 582, 925, 366, 85 });
        drawPlain (g, th, { 958, 925, 430, 85 });

        g.setColour (th.text);
        g.setFont (uiFont (18.0f));
        g.drawText ("IN", 10, 410, 118, 24, juce::Justification::centred, false);
        g.drawText ("OUT", 1408, 410, 118, 24, juce::Justification::centred, false);
        g.setFont (uiFont (12.0f, false));
        g.setColour (th.subtext);
        for (int db = 0; db >= -48; db -= 6)
        {
            const float y = 445.0f + 250.0f * (float) (-db) / 48.0f;
            g.drawText ("- " + juce::String (-db), 66, (int) y - 8, 50, 16, juce::Justification::centredLeft, false);
            g.drawText ("- " + juce::String (-db), 1471, (int) y - 8, 50, 16, juce::Justification::centredLeft, false);
        }

        // labels of the auto-tune controls
        g.setColour (th.text);
        g.setFont (uiFont (15.0f));
        g.drawText ("SCALE", 358, 392, 130, 20, juce::Justification::centred, false);
        g.drawText ("NOTE", 527, 392, 80, 20, juce::Justification::centred, false);
        g.drawText ("MODE", 917, 385, 110, 20, juce::Justification::centred, false);
        g.setFont (uiFont (12.5f));
        g.drawText ("TYPE", 294, 772, 72, 16, juce::Justification::centred, false);
        g.drawText ("TYPE", 400, 838, 140, 16, juce::Justification::centred, false);

        g.setFont (uiFont (16.0f));
        g.drawText ("MASTER MIX", 1066, 940, 200, 22, juce::Justification::centredLeft, false);
        g.setFont (uiFont (18.0f, false));
        g.drawText (juce::String ((int) std::lround (proc.apvts.getRawParameterValue (ids::masterMix)->load())) + "%",
                    1312, 972, 70, 30, juce::Justification::centredLeft, false);
        g.setFont (uiFont (13.0f));
        g.setColour (th.subtext);
        g.drawText ("INTERFACE THEMES", 14, 926, 224, 16, juce::Justification::centred, false);
        g.setFont (uiFont (11.0f, false));
        g.drawText ("Latency: " + juce::String (proc.getLatencySamples() * 1000.0 / std::max (1.0, proc.getSampleRate()), 1) + " ms",
                    1408, 905, 118, 16, juce::Justification::centred, false);
    }

private:
    struct PanelDef { juce::String title; juce::Rectangle<int> r; const char* onId; };

    // ------------------------------------------------------------------ helpers
    Knob& addKnob (const char* id, const juce::String& label, int dia)
    {
        knobs.push_back (std::make_unique<Knob> (proc.apvts, id, label, dia));
        addAndMakeVisible (*knobs.back());
        return *knobs.back();
    }
    static void placeKnob (Knob& k, int cx, int cy)
    {
        const int w = std::max (k.diameter(), 80);
        k.setBounds (cx - w / 2, cy - k.diameter() / 2, w, k.diameter() + 32);
    }
    PillButton& addPill (const char* id, const juce::String& text)
    {
        pills.push_back (std::make_unique<PillButton> (proc.apvts, id, text));
        addAndMakeVisible (*pills.back());
        return *pills.back();
    }
    ChoiceBox& addChoice (const char* id, const juce::StringArray& items)
    {
        choices.push_back (std::make_unique<ChoiceBox> (proc.apvts, id, items));
        addAndMakeVisible (*choices.back());
        return *choices.back();
    }

    static void drawPlain (juce::Graphics& g, const Theme& th, juce::Rectangle<int> r)
    {
        const auto f = r.toFloat().reduced (1.0f);
        g.setColour (th.panel.withAlpha (0.88f));
        g.fillRoundedRectangle (f, 10.0f);
        g.setColour (th.panelBorder.withAlpha (0.85f));
        g.drawRoundedRectangle (f, 10.0f, 1.8f);
    }
    static void drawPanel (juce::Graphics& g, const Theme& th, juce::Rectangle<int> r, const juce::String& title, const char* onId)
    {
        const auto f = r.toFloat().reduced (1.0f);
        g.setGradientFill (juce::ColourGradient (th.panel.brighter (0.15f), f.getX(), f.getY(), th.panel.darker (0.5f), f.getX(), f.getBottom(), false));
        g.fillRoundedRectangle (f, 10.0f);
        g.setColour (th.primary.withAlpha (0.18f));
        g.drawRoundedRectangle (f.expanded (2.0f), 11.0f, 3.0f);
        g.setColour (th.panelBorder);
        g.drawRoundedRectangle (f, 10.0f, 1.8f);

        const bool big = onId != nullptr && juce::String (onId) == ids::atOn;
        const float tx = big ? (float) r.getX() + 160.0f : (float) r.getX() + (onId != nullptr ? 64.0f : 16.0f);
        const float ty = big ? (float) r.getY() + 30.0f : (float) r.getY() + 20.0f;
        g.setColour (th.accent);
        g.fillPath (makeStar ({ tx - 10.0f, ty }, 5, 8.0f, 3.5f));
        g.setColour (th.text);
        g.setFont (uiFont (big ? 24.0f : 17.0f));
        g.drawText (title, (int) tx + 4, (int) ty - 14, 220, 28, juce::Justification::centredLeft, false);
    }

    void applyTheme (int i)
    {
        i = juce::jlimit (0, themes.size() - 1, i);
        laf.setTheme (&themes.get (i));
        proc.setThemeIndex (i);
        themeBox.setSelectedItemIndex (i, juce::dontSendNotification);
        editor.sendLookAndFeelChange();
        repaint();
    }

    void saveToFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Save MSC VOXITY preset",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mscvox");
        juce::Component::SafePointer<Content> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr) return;
                                  auto f = fc.getResult();
                                  if (f != juce::File()) f.withFileExtension ("mscvox").replaceWithText (safe->proc.stateToXmlString());
                              });
    }
    void loadFromFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Load MSC VOXITY preset",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.mscvox");
        juce::Component::SafePointer<Content> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe == nullptr) return;
                                  auto f = fc.getResult();
                                  if (f.existsAsFile() && safe->proc.loadStateFromXmlString (f.loadFileAsString()))
                                      safe->applyTheme (safe->proc.getThemeIndex());
                              });
    }
    void showSettingsMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Reload current preset");
        m.addItem (2, "About MSC VOXITY 2.0");
        juce::Component::SafePointer<Content> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&gearBtn), [safe] (int r)
        {
            if (safe == nullptr) return;
            if (r == 1) safe->proc.setCurrentProgram (safe->proc.getCurrentProgram());
            if (r == 2)
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "MSC VOXITY 2.0",
                    "Vocal processor: real-time auto-tune (YIN pitch detection + PSOLA shifter), compressor, de-esser, EQ, "
                    "air, distortion, saturation, chorus, delay, reverb and stereo widener.\n\n"
                    "Plugin latency: " + juce::String (safe->proc.getLatencySamples() * 1000.0 / std::max (1.0, safe->proc.getSampleRate()), 1)
                    + " ms (reported to the host).");
        });
    }

    void timerCallback() override
    {
        banner.tick(); pitch.tick(); grView.tick();
        meterIn.tick(); meterOut.tick();
        scopeDist.repaint(); scopeSat.repaint(); scopeChorus.repaint(); scopeWide.repaint(); scopeDeEss.repaint();
        eqView.repaint();
        if (delayTime != nullptr) delayTime->repaint();
        if (divisions != nullptr) divisions->repaint();
        const int cur = proc.getCurrentProgram();
        if (cur != lastPreset)
        {
            lastPreset = cur;
            presetBox.setSelectedItemIndex (cur, juce::dontSendNotification);
            presetList.repaint(); presetInfo.repaint();
        }
        const int th = proc.getThemeIndex();
        if (th != themeBox.getSelectedItemIndex()) applyTheme (th);
        // master mix % text lives in paint(); refresh that small area
        repaint (1312, 972, 70, 30);
    }

    // ------------------------------------------------------------------ members
    MSCVoxProcessor& proc;
    ThemeManager& themes;
    MscLookAndFeel& laf;
    juce::Component& editor;

    std::vector<PanelDef> panels;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<PillButton>> pills;
    std::vector<std::unique_ptr<ChoiceBox>> choices;
    Knob* delayTime = nullptr;
    std::unique_ptr<ui::DivisionRow> divisions;
    std::unique_ptr<Knob> masterKnob;
    juce::Slider masterBar;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterBarAttachment;

    Banner banner;
    ui::PitchDisplay pitch;
    ui::EqDisplay eqView;
    ui::ScopeDisplay scopeDist, scopeSat, scopeChorus, scopeWide, scopeDeEss;
    ui::GrDisplay grView;
    ui::LevelMeter meterIn, meterOut;
    ui::PresetList presetList;
    ui::PresetInfo presetInfo;
    ui::ThemeSwatches swatches;

    juce::TextButton prevBtn, nextBtn, saveBtn, loadBtn;
    ui::GearButton gearBtn;
    juce::ComboBox presetBox, themeBox;
    std::unique_ptr<juce::FileChooser> chooser;
    int lastPreset = 7;
};

// ===================================================================================================== Editor
MSCVoxEditor::MSCVoxEditor (MSCVoxProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      laf (&themes.get (p.getThemeIndex()))
{
    setLookAndFeel (&laf);
    content = std::make_unique<Content> (proc, themes, laf, *this);
    addAndMakeVisible (*content);

    constrainer.setFixedAspectRatio ((double) kDesignW / (double) kDesignH);
    constrainer.setSizeLimits (768, 512, 2304, 1536);
    setConstrainer (&constrainer);
    setResizable (true, true);
    setSize (1152, 768);
}

MSCVoxEditor::~MSCVoxEditor()
{
    content.reset();
    setLookAndFeel (nullptr);
}

void MSCVoxEditor::resized()
{
    if (content == nullptr) return;
    content->setBounds (0, 0, kDesignW, kDesignH);
    content->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kDesignW));
}
