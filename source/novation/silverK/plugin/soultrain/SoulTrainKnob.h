#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <optional>
#include <vector>

namespace soultrain
{
class SoulTrainKnob final : public juce::Slider
{
public:
    // JUCE draws the moving marker and the partially lit end of the ring.
    struct Layer
    {
        juce::Image pixels;
        juce::Point<float> origin;
    };

    struct Artwork
    {
        static Artwork load(const juce::File& directory);

        float width = 0;
        float height = 0;
        float pixelScale = 1;
        juce::Rectangle<float> dial;
        Layer body;
        Layer focusedBody;
        std::vector<Layer> glow;
       #if JUCE_WINDOWS
        struct GlowFrame
        {
            size_t first;
            size_t end;
            Layer layer;
        };
        std::vector<GlowFrame> glowFrames;
       #endif
    };

    struct Range
    {
        int minimum;
        int maximum;
        int offset;
        int defaultValue;

        bool isBipolar() const noexcept { return minimum + offset < 0 && maximum + offset > 0; }
    };

    SoulTrainKnob(const Artwork&, Range, const juce::String& label);
    juce::String getTextFromValue(double value) override;
    double getValueFromText(const juce::String&) override;

    double angleDegrees() const noexcept;
    double glowLevel(size_t dotIndex) const noexcept;

    void paint(juce::Graphics&) override;
    bool hitTest(int x, int y) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;
    void focusGained(FocusChangeType) override;
    void focusLost(FocusChangeType) override;

private:
    double position() const noexcept;
    double glowLevel(size_t dotIndex,double relative) const noexcept;
    float scale() const noexcept;
    juce::AffineTransform artworkTransform() const noexcept;
    void drawLayer(juce::Graphics&, const Layer&, float opacity) const;
    void setByUser(double);

    const Artwork& art;
    const Range range;
    juce::Path marker;
    bool keyboardFocus = false;
    float previousDragY = 0;
    double dragValue = 0;
    std::optional<ScopedDragNotification> drag;
};
}
