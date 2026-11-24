#pragma once

#include "ChoiceStyle.h"
#include <array>
#include <cmath>
#include <map>
#include <tuple>
#include <vector>

namespace soultrain {

enum class ButtonHighlight { none, hover, pressed };

inline ButtonHighlight buttonHighlightFor(bool selected,bool hovered,bool pressed)
{
    if (selected || pressed) return ButtonHighlight::pressed;
    return hovered?ButtonHighlight::hover:ButtonHighlight::none;
}

inline const juce::Colour selectedButtonInk(0xffa2c5ed);
inline constexpr auto pressedButtonTextShadow =
    "rgba(162, 197, 237, 0.85) 0px 0px 6px, rgba(162, 197, 237, 0.48) 0px 0px 12px";

inline juce::Colour buttonTextColour(ButtonHighlight state)
{
    const juce::Colour resting(0xffb6c7db);
    if (state==ButtonHighlight::pressed) return selectedButtonInk;
    return state==ButtonHighlight::hover?resting.interpolatedWith(juce::Colours::white,.15f):resting;
}

namespace figmaButton {

// Figma's first gradient-transform row maps normalised layer coordinates to the colour ramp.
inline juce::ColourGradient gradient(juce::Rectangle<float> layer,float a,float b,float c,
                                     juce::Colour start,juce::Colour end)
{
    const juce::Point<float> direction{a/layer.getWidth(),b/layer.getHeight()};
    const auto scale=1.f/(direction.x*direction.x+direction.y*direction.y);
    const auto origin=layer.getTopLeft()-direction*(c*scale);
    return {start,origin,end,origin+direction*scale,false};
}

// SmallButtonFX 107:5060 and MenuButton 116:5566 share these inset hover strokes.
inline void hover(juce::Graphics& g,juce::Rectangle<float> bounds,float opacity)
{
    const auto face=bounds.reduced(2.f);
    const auto frame=bounds.reduced(1.f);
    const auto ink=[opacity](juce::uint32 rgb,float alpha){return juce::Colour(rgb).withAlpha(alpha*opacity);};
    const auto shade=ink(0xffcfd8e2,.10f);
    g.setGradientFill({shade,face.getTopLeft(),shade.withAlpha(0.f),face.getBottomLeft(),false});
    g.fillRect(face);
    g.setGradientFill(gradient(face,0.f,-.590340f,.865832f,ink(0xff202d41,.13f),ink(0xffadceee,.13f)));
    g.drawRect(frame,2.f);
    g.setGradientFill(gradient(face,-.026211f,-.479005f,.386717f,ink(0xff171716,.59f),juce::Colours::transparentWhite));
    g.drawRect(frame,2.f);
    g.setGradientFill(gradient(face,.009742f,-.371532f,.649284f,ink(0xffc5dbf6,0.f),ink(0xffc5dbf6,.95f)));
    g.drawRect(frame,2.f);
}

inline std::vector<float> blurredCoverage(int pixels,float sigma)
{
    std::vector<float> coverage(size_t(pixels),0.f);
    const auto span=float(pixels)*.5f;
    const auto divisor=sigma*std::sqrt(2.f);
    for (int i=0;i<pixels;++i) {
        const auto sample=(float(i)+.5f)*.5f;
        coverage[size_t(i)]=.5f*(std::erf(sample/divisor)+std::erf((span-sample)/divisor));
    }
    return coverage;
}

inline const juce::Image& innerShadow(juce::Rectangle<float> face,bool upper)
{
    const auto width=juce::roundToInt(face.getWidth()*2.f);
    const auto height=juce::roundToInt(face.getHeight()*2.f);
    // SoulTrain painting runs on the GUI thread. Cache only the rendered size and button family.
    static std::map<std::tuple<int,int,bool>,juce::Image> cache;
    const auto key=std::make_tuple(width,height,upper);
    if (const auto found=cache.find(key);found!=cache.end()) return found->second;

    // Gaussian source-alpha masks from SmallButton 107:5179 and MenuButton 1:618.
    const auto narrow=upper?2.1f:1.55f;
    const auto wide=upper?3.55f:2.3f;
    const auto x1=blurredCoverage(width,narrow), y1=blurredCoverage(height,narrow);
    const auto x2=blurredCoverage(width,wide), y2=blurredCoverage(height,wide);
    juce::Image mask(juce::Image::SingleChannel,width,height,false);
    juce::Image::BitmapData pixels(mask,juce::Image::BitmapData::writeOnly);
    for (int y=0;y<height;++y) {
        auto* row=pixels.getLinePointer(y);
        for (int x=0;x<width;++x)
            row[x]=juce::uint8(juce::roundToInt(255.f*(1.f-x1[size_t(x)]*y1[size_t(y)]*x2[size_t(x)]*y2[size_t(y)])));
    }
    return cache.emplace(key,std::move(mask)).first->second;
}

inline const juce::Image& buttonSurface(int width,int height,ButtonHighlight state)
{
    static std::map<std::tuple<int,int,ButtonHighlight>,juce::Image> cache;
    const auto key=std::make_tuple(width,height,state);
    if (const auto found=cache.find(key);found!=cache.end()) return found->second;

    constexpr float rasterScale=2.f;
    juce::Image surface(juce::Image::ARGB,width*2,height*2,true);
    juce::Graphics g(surface);
    g.addTransform(juce::AffineTransform::scale(rasterScale));
    const juce::Rectangle<float> face{float(width),float(height)};
    const bool press=state==ButtonHighlight::pressed;
    const bool hover=state==ButtonHighlight::hover;
    const auto top=juce::Colour(press?0xff304d69:hover?0xff46515f:0xff39424e);
    const auto bottom=juce::Colour(press?0xff20384f:hover?0xff2c3541:0xff262d37);
    g.setGradientFill({top,face.getTopLeft(),bottom,face.getBottomLeft(),false});
    g.fillRect(face);
    if (press) {
        const auto inset=face.reduced(2.f);
        const auto& rim=innerShadow(inset,true);
        g.setColour(selectedButtonInk.withAlpha(.65f));
        g.drawImage(rim,inset,juce::RectanglePlacement::stretchToFit,true);
        g.setColour(selectedButtonInk);
        g.drawRect(face,2.f);
    } else {
        paintChoiceFrame(g,face,hover?ChoiceState::hover:ChoiceState::normal,1.f,hover?2.f:1.f);
    }
    return cache.emplace(key,std::move(surface)).first->second;
}

inline void pressed(juce::Graphics& g,juce::Rectangle<float> bounds,float opacity,bool upper)
{
    const auto shell=bounds.reduced(1.f);
    const auto inset=bounds.reduced(3.f);
    const auto ink=[opacity](juce::uint32 rgb,float alpha){return juce::Colour(rgb).withAlpha(alpha*opacity);};
    g.setColour(ink(0xff262d37,1.f));
    g.fillRect(shell);
    auto surface=gradient(shell,-.079f,.437f,.262f,ink(0xffffffff,upper?.05f:.04f),juce::Colours::transparentBlack);
    surface.addColour(.341083,ink(0xffb9b9b9,upper?.03623f:.028985f));
    g.setGradientFill(surface);
    g.fillRect(shell);
    g.setGradientFill(gradient(shell,0.f,-.590340f,.865832f,ink(0xff202d41,.13f),ink(0xffadceee,.13f)));
    g.drawRect(shell,1.f);
    if (upper) {
        const auto frost=ink(0xffcfd8e2,.10f);
        g.setGradientFill({frost,inset.getTopLeft(),frost.withAlpha(0.f),inset.getBottomLeft(),false});
        g.fillRect(inset);
        g.setGradientFill(gradient(inset,0.f,-.590340f,.865832f,ink(0xff202d41,.16f),ink(0xffadceee,.16f)));
        g.fillRect(inset);
    }

    const auto& glow=innerShadow(inset,upper);
    g.setColour(ink(0xffc5dbf6,upper?1.f:.65f));
    g.drawImageTransformed(glow,juce::AffineTransform::scale(inset.getWidth()/float(glow.getWidth()),
        inset.getHeight()/float(glow.getHeight())).translated(inset.getPosition()),true);
    g.drawRect(bounds.reduced(upper?2.f:1.f),2.f);
    // MenuButton 1:621 uses a 34% blue fill on a layer at 38%.
    g.setColour(ink(0xff0088ff,upper?.34f*.38f:.13f));
    g.fillRect(shell);
    g.setColour(ink(0xff000000,.01f));
    g.fillRect(shell);
    g.setColour(ink(0xff000000,.30f));
    g.drawRect(bounds,1.f);
}

}

inline void paintStandardButton(juce::Graphics& g,juce::Rectangle<int> bounds,ButtonHighlight state,float opacity=1.f)
{
    if (bounds.getWidth()<8 || bounds.getHeight()<8 || !g.clipRegionIntersects(bounds)) return;
    const auto inner=bounds.reduced(1);
    const auto& face=figmaButton::buttonSurface(inner.getWidth(),inner.getHeight(),state);
    g.setOpacity(opacity);
    g.drawImage(face,inner.toFloat());
    g.setColour(juce::Colours::black.withAlpha(.38f*opacity));
    g.drawRect(bounds.toFloat(),1.f);
}

inline void paintButtonHighlight(juce::Graphics& g,juce::Rectangle<float> bounds,
                                 ButtonHighlight state,float opacity=1.f,bool upper=false)
{
    if (state==ButtonHighlight::none || bounds.getWidth()<=7.f || bounds.getHeight()<=7.f ||
        !g.clipRegionIntersects(bounds.getSmallestIntegerContainer())) return;
    if (state==ButtonHighlight::hover) figmaButton::hover(g,bounds,opacity);
    else figmaButton::pressed(g,bounds,opacity,upper);
}

}
