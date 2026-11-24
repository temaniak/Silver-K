#pragma once

#include "ChoiceStyle.h"
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <vector>

namespace soultrain
{
class WaveIndicator final : public juce::Component
{
public:
    struct Choice
    {
        int value;
        juce::String name;
        juce::String title;
        ChoiceCaption caption;
    };
    using Choices = std::vector<Choice>;

    struct Layer
    {
        juce::Image image;
        juce::Point<float> origin;
    };
    struct Glyph
    {
        int value;
        Layer art;
        float advance;
    };
    struct Artwork
    {
        static Artwork load(const juce::File& directory,const juce::Font& font);
        const Choices& choicesFor(const juce::String& model) const;

        float width = 0, height = 0, pixelScale = 1;
        juce::Rectangle<float> face, trigger;
        std::array<juce::Rectangle<float>,2> arrows;
        juce::Point<float> label;
        float lineHeight = 20.f;
        Layer frame;
        std::vector<Glyph> glyphs;
        std::map<juce::String,Choices> models;
    };

    WaveIndicator(const Artwork&, const Choices&, unsigned oscillator, int value);
    ~WaveIndicator() override;

    int value() const noexcept;
    int selectedChoice() const noexcept { return int(selected); }
    juce::String readout() const;
    bool setWaveform(int value);
    void selectWaveform(size_t index);
    void setMenuOpen(bool);
    juce::Rectangle<float> faceBounds() const noexcept {return art.face.transformedBy(transform());}
    std::function<void(int)> onWaveformChange;
    std::function<void(juce::Component&,const Choices&,int)> onMenuRequested;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool hitTest(int, int) override;
    void focusOfChildComponentChanged(FocusChangeType) override;
    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    class Input;

    juce::AffineTransform transform() const noexcept;
    bool handleKey(const juce::KeyPress&, bool stepButton);
    void step(int, bool wrap);
    void requestMenu();
    void refreshCaption();
    void drawLayer(juce::Graphics&, const Layer&) const;

    const Artwork& art;
    const Choices& choices;
    size_t selected = 0;
    std::unique_ptr<Input> trigger, previous, next;
    const Glyph* glyph = nullptr;
    bool menuOpen = false, keyboardFocus = false;
    juce::String typeahead;
    uint32_t lastTyped = 0;
};
}
