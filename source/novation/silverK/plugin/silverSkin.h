#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>

namespace silverK
{
// UI-only data. Packages are read and validated before replacing an editor.
struct SkinPackage
{
    juce::String id, name;
    juce::File manifest;
    juce::var layout, theme;
    std::map<juce::String, juce::Image> images;
    juce::Typeface::Ptr typeface;
    static SkinPackage builtin();
    static bool load(const juce::File&, SkinPackage&, juce::String& error);
    static std::vector<std::pair<juce::String, juce::File>> discover(const juce::File&);
    static bool exportTemplate(const juce::File& skinsFolder, juce::File& result, juce::String& error);
};

class SkinLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit SkinLookAndFeel(const SkinPackage&);
    juce::Colour colour(juce::uint32 original) const;
    juce::Font font(float height, int style=juce::Font::plain) const;
    const juce::Image& image(const juce::String&) const;
    bool isFlat() const { return package.theme["renderStyle"].toString()=="flat"; }
    const SkinPackage& package;
    juce::Typeface::Ptr getTypefaceForFont(const juce::Font&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getSliderPopupFont(juce::Slider&) override;
    bool drawKnobStrip(juce::Graphics&, juce::Slider&, juce::Rectangle<float>, float) const;
};

juce::Colour skinColour(const juce::Component&, juce::uint32);
inline juce::Colour skinColour(const juce::Component& c, juce::Colour colour)
{ return skinColour(c, colour.getARGB()); }
void setSkinColour(juce::Component&, int colourId, juce::Colour);
juce::Font skinFont(const juce::Component&, float height, int style=juce::Font::plain);
// Reapply geometry after native layout; styling is applied once to each new
// component. Does not change values, enabled states or model-dependent visibility.
void applySkin(juce::Component&, bool includeRoot=true);
bool paintSkinBackground(const juce::Component&, juce::Graphics&);
bool skinDrawPanels(const juce::Component&);
bool skinIsFlat(const juce::Component&);
class SkinDecoration final : public juce::Component
{
public:
    explicit SkinDecoration(juce::var description);
    void paint(juce::Graphics&) override;
    juce::var description;
};
}
