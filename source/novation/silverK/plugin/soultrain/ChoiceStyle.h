#pragma once

#include "SoulTrainGlow.h"
#include <optional>

namespace soultrain {

inline constexpr float choiceFontSize=14.f;
inline constexpr int choiceFontWeight=500;
inline constexpr auto choiceMenuOpenProperty="soulTrainChoiceMenuOpen";
inline const juce::Colour choiceInk(0xff84ddf8);

enum class ChoiceState { normal, hover, open };
enum class ChoiceGlow { none, normal, strong };

inline ChoiceState choiceState(bool enabled,bool pressedOrOpen,bool hovered)
{
    if (!enabled) return ChoiceState::normal;
    return pressedOrOpen?ChoiceState::open:hovered?ChoiceState::hover:ChoiceState::normal;
}

inline ChoiceGlow choiceGlow(ChoiceState state)
{
    return state==ChoiceState::open?ChoiceGlow::strong:ChoiceGlow::normal;
}

inline void paintChoiceFrame(juce::Graphics& g,juce::Rectangle<float> bounds,ChoiceState state,
                             float opacity=1.f,float thickness=2.f)
{
    // The SVG overlays a 37% gradient on a solid stroke in a 50-unit viewBox.
    const auto base=juce::Colour(0xff506072);
    auto light=base.interpolatedWith(juce::Colour(0xffadceee),.37f);
    auto dark=base.interpolatedWith(juce::Colour(0xff202d41),.37f);
    if (state==ChoiceState::hover) {
        light=light.interpolatedWith(juce::Colours::white,.15f);
        dark=dark.interpolatedWith(juce::Colours::white,.15f);
    } else if (state==ChoiceState::open) {
        // The field and popup share the lighter stroke from the open reference.
        light=juce::Colour(0xff94aac1);
        dark=juce::Colour(0xff91a7be);
    }
    constexpr float insetFromTop=9.02778f/50.f;
    const juce::Point<float> upper{bounds.getX(),bounds.getY()-bounds.getHeight()*insetFromTop};
    const juce::Point<float> lower{bounds.getX(),bounds.getCentreY()};
    g.setGradientFill({light.withMultipliedAlpha(opacity),upper,
                      dark.withMultipliedAlpha(opacity),lower,false});
    g.drawRect(bounds,thickness);
}

class ChoiceCaption {
public:
    void set(const juce::Font& font,const juce::String& value,float maxWidth)
    {
        if (value==text && maxWidth==availableWidth) return;
        text=value;
        availableWidth=maxWidth;
        baseline=font.getAscent()-font.getHeight()*.5f;
        juce::GlyphArrangement glyphs;
        glyphs.addCurtailedLineOfText(font,value,0.f,0.f,maxWidth,true);
        advance=glyphs.getBoundingBox(0,-1,true).getRight();
        outline.clear();
        glyphs.createPath(outline);
        glow.reset();
    }

    void draw(juce::Graphics& g,juce::Rectangle<float> bounds,juce::Justification alignment,
              juce::Colour ink=choiceInk,float opacity=1.f,ChoiceGlow illumination=ChoiceGlow::normal) const
    {
        if (outline.isEmpty() || bounds.isEmpty()) return;
        auto x=bounds.getX();
        if (alignment.testFlags(juce::Justification::horizontallyCentred))
            x+=(bounds.getWidth()-advance)*.5f;
        else if (alignment.testFlags(juce::Justification::right))
            x+=bounds.getWidth()-advance;

        juce::Graphics::ScopedSaveState save(g);
        if (!g.reduceClipRegion(bounds.expanded(3.f,0.f).getSmallestIntegerContainer())) return;
        g.addTransform(juce::AffineTransform::translation(x,bounds.getCentreY()+baseline));
        if (illumination!=ChoiceGlow::none) {
            static const std::vector<TextShadow> shadows{{choiceInk,0.f,0.f,6.f}};
            if (!glow) glow.emplace(outline,shadows);
            glow->draw(g,opacity);
            if (illumination==ChoiceGlow::strong) glow->draw(g,opacity*.7f);
        }
        // Fill the outlines at the current device scale, including fractional zoom.
        g.setColour(ink.withMultipliedAlpha(opacity));
        g.fillPath(outline);
    }

private:
    juce::String text;
    float availableWidth=0.f,baseline=0.f,advance=0.f;
    juce::Path outline;
    mutable std::optional<CaptionGlow> glow;
};

}
