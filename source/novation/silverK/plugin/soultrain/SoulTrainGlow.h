#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>
#include <regex>
#include <vector>

namespace soultrain {

struct TextShadow {
    juce::Colour colour;
    float x, y, radius;
};

inline std::vector<TextShadow> textShadows(const juce::String& css, float opacity = 1.f)
{
    static const std::regex format(
        R"(rgba?\((\d+),\s*(\d+),\s*(\d+)(?:,\s*([\d.]+))?\)\s*(-?[\d.]+)px\s*(-?[\d.]+)px(?:\s*([\d.]+)px)?)");
    std::vector<TextShadow> shadows;
    const auto rule=css.toStdString();
    for(std::sregex_iterator i(rule.begin(),rule.end(),format),end;i!=end;++i) {
        const auto& m=*i;
        const auto rgb=juce::Colour::fromRGB(uint8_t(std::stoi(m[1])),uint8_t(std::stoi(m[2])),uint8_t(std::stoi(m[3])));
        const auto alpha=m[4].matched?std::stof(m[4]):1.f;
        const auto blur=m[7].matched?std::stof(m[7]):0.f;
        shadows.push_back({rgb.withAlpha(juce::jlimit(0.f,1.f,alpha*opacity)),
                           std::stof(m[5]),std::stof(m[6]),blur});
    }
    return shadows;
}

inline std::vector<float> blurAlpha(const std::vector<float>& in,int width,int height,float sigma)
{
    if(sigma<.01f) return in;
    const auto radius=int(std::ceil(3*sigma));
    std::vector<float> weights(size_t(2*radius+1));
    float sum=0;
    for(int n=-radius;n<=radius;++n) {
        const auto weight=std::exp(-float(n*n)/(2*sigma*sigma));
        weights[size_t(n+radius)]=weight;
        sum+=weight;
    }
    for(auto& weight:weights) weight/=sum;
    std::vector<float> row(size_t(width),0.f),result(in.size(),0.f);
    // Spread nonzero coverage across the same kernel. Transparent padding
    // contributes nothing, including in the vertical pass on empty rows.
    for(int y=0;y<height;++y) {
        const auto* source=in.data()+size_t(y)*size_t(width);
        if(std::none_of(source,source+width,[](float alpha){return alpha!=0.f;})) continue;
        std::fill(row.begin(),row.end(),0.f);
        for(int x=0;x<width;++x) {
            const auto alpha=source[x];
            if(alpha==0.f) continue;
            for(int n=std::max(-radius,-x);n<=std::min(radius,width-1-x);++n)
                row[size_t(x+n)]+=alpha*weights[size_t(radius-n)];
        }
        for(int n=std::max(-radius,-y);n<=std::min(radius,height-1-y);++n) {
            const auto weight=weights[size_t(radius-n)];
            auto* target=result.data()+size_t(y+n)*size_t(width);
            for(int x=0;x<width;++x) target[x]+=row[size_t(x)]*weight;
        }
    }
    return result;
}

class CaptionGlow {
public:
    CaptionGlow() = default;

    CaptionGlow(const juce::GlyphArrangement& glyphs,const std::vector<TextShadow>& shadows)
        : CaptionGlow(glyphs.getBoundingBox(0,-1,false),shadows,[&](juce::Graphics& g){glyphs.draw(g);})
    {}

    CaptionGlow(const juce::Path& outline,const std::vector<TextShadow>& shadows)
        : CaptionGlow(outline.getBounds(),shadows,[&](juce::Graphics& g){g.fillPath(outline);})
    {}

    void translate(juce::Point<float> delta) { origin+=delta; }

    juce::Rectangle<float> paintBounds() const
    {
        juce::Rectangle<float> bounds;
        for (const auto& layer:layers)
            bounds=bounds.getUnion(layer.image.getBounds().toFloat()/float(scale)
                +origin+juce::Point<float>{layer.shadow.x,layer.shadow.y});
        return bounds;
    }

    void draw(juce::Graphics& g,float opacity=1.f) const
    {
        for(const auto& layer:layers) {
            g.setColour(layer.shadow.colour.withMultipliedAlpha(opacity));
            g.drawImageTransformed(layer.image,juce::AffineTransform::scale(1.f/scale)
                .translated(origin+juce::Point<float>{layer.shadow.x,layer.shadow.y}),true);
        }
    }

private:
    template <typename DrawMask>
    CaptionGlow(juce::Rectangle<float> bounds,const std::vector<TextShadow>& shadows,DrawMask drawMask)
    {
        if(shadows.empty() || bounds.isEmpty()) return;
        float margin=2.f;
        for(const auto& s:shadows) margin=std::max(margin,1.5f*s.radius+2.f);
        const auto area=bounds.expanded(margin).getSmallestIntegerContainer();
        origin=area.getPosition().toFloat();
        const auto width=scale*area.getWidth(),height=scale*area.getHeight();
        juce::Image mask(juce::Image::SingleChannel,width,height,true);
        {
            juce::Graphics g(mask);
            g.addTransform(juce::AffineTransform::translation(-origin).scaled(float(scale)));
            g.setColour(juce::Colours::white);
            drawMask(g);
        }
        const juce::Image::BitmapData ink(mask,juce::Image::BitmapData::readOnly);
        std::vector<float> coverage(size_t(width)*size_t(height));
        for(int y=0;y<height;++y)
            for(int x=0;x<width;++x)
                coverage[size_t(y*width+x)]=ink.getLinePointer(y)[x]/255.f;

        for(auto i=shadows.rbegin();i!=shadows.rend();++i) {
            // CSS blur radius corresponds to twice the Gaussian deviation.
            Layer layer{juce::Image(juce::Image::SingleChannel,width,height,true),*i};
            const auto blurred=blurAlpha(coverage,width,height,i->radius*scale*.5f);
            juce::Image::BitmapData alpha(layer.image,juce::Image::BitmapData::writeOnly);
            for(int y=0;y<height;++y)
                for(int x=0;x<width;++x)
                    alpha.getLinePointer(y)[x]=uint8_t(juce::roundToInt(255.f*juce::jlimit(0.f,1.f,blurred[size_t(y*width+x)])));
            layers.push_back(std::move(layer));
        }
    }

    struct Layer { juce::Image image; TextShadow shadow; };
    static constexpr int scale=2;
    juce::Point<float> origin;
    std::vector<Layer> layers;

};
}
