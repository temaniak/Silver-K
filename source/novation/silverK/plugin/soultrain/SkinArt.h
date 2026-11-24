#pragma once

#include "FigmaButton.h"
#include "SoulTrainGlow.h"
#include "SoulTrainKnob.h"
#include "WaveIndicator.h"
#include "../silverImageScale.h"
#include <map>
#include <optional>
#include <stdexcept>

namespace soultrain {

inline constexpr int regularButtonHeight=34;
inline constexpr int choiceBoxHeight=36;

inline juce::Rectangle<float> rect(const juce::var& value)
{
    return value.size()==4?juce::Rectangle<float>{float(value[0]),float(value[1]),float(value[2]),float(value[3])}:juce::Rectangle<float>{};
}

inline juce::Point<float> point(const juce::var& value)
{
    return {float(value[0]),float(value[1])};
}

inline juce::Colour colour(const juce::var& values)
{
    return juce::Colour::fromRGB(uint8_t(int(values[0])),uint8_t(int(values[1])),uint8_t(int(values[2])))
        .withAlpha(juce::jlimit(0.f,1.f,float(values[3])));
}

class SkinArt {
    const std::map<int,juce::Typeface::Ptr> inter;

public:
    explicit SkinArt(const juce::File& root)
        : inter(loadInter(root.getChildFile("fonts"))),
          choiceFont(font(choiceFontSize,choiceFontWeight)),
          knob(SoulTrainKnob::Artwork::load(root.getChildFile("knob"))),
          wave(WaveIndicator::Artwork::load(root.getChildFile("wave"),choiceFont)),
          directory(root), manifest(juce::JSON::parse(root.getChildFile("sprites.json"))),
          controlStyles(juce::JSON::parse(root.getChildFile("controls.json")))
    {
        if(!manifest.isObject()||!controlStyles.isObject()) throw std::runtime_error("SoulTrain control artwork is missing");
        const auto folder=root.getChildFile("fonts");
        pixel=loadFont(folder.getChildFile("pixelmix-web.ttf"));
        size_t index=0;
        for (const auto* state:{"off","on","off-hover","on-hover"})
            indicatorArt[index++]=juce::ImageCache::getFromFile(directory.getChildFile("sprites/figma-led-"+juce::String(state)+".png"));
    }

    juce::Font font(float size,int weight=400,bool lcd=false) const
    {
        return juce::Font(lcd?pixel:inter.at(juce::jlimit(4,7,(weight+50)/100)*100)).withPointHeight(size);
    }

    juce::String style(const char* kind,const juce::String& state) const
    {
        return controlStyles[kind][juce::Identifier(state)].toString();
    }
    juce::String lamp(int height,bool lit) const
    {
        return controlStyles["lamps"][juce::Identifier(juce::String(height))][lit?"on":"off"].toString();
    }

    static juce::Rectangle<float> indicatorBounds(juce::Rectangle<float> lens)
    {
        // The original Figma SVG is 42.8 px around a 12 px lens and is padded to 43 px.
        return lens.withSizeKeepingCentre(lens.getWidth()*(43.f/12.f),lens.getHeight()*(43.f/12.f));
    }

    bool paintIndicator(juce::Graphics& g,juce::Rectangle<float> lens,bool lit,bool hovered,float opacity) const
    {
        const auto& image=indicatorArt[size_t(lit)+2u*size_t(hovered)];
        if (!image.isValid() || lens.isEmpty()) return false;
        auto target=indicatorBounds(lens);
       #if JUCE_WINDOWS
        // Align all four LED states to the same physical pixels, then apply
        // the premultiplied-alpha filter before drawing the sprite.
        const auto scale=g.getInternalContext().getPhysicalPixelScaleFactor();
        if (scale>0.f) {
            const auto w=std::max(1,juce::roundToInt(target.getWidth()*scale));
            const auto h=std::max(1,juce::roundToInt(target.getHeight()*scale));
            const auto centre=target.getCentre()*scale;
            target={juce::roundToInt(centre.x-w*.5f)/scale,juce::roundToInt(centre.y-h*.5f)/scale,
                    w/scale,h/scale};
            if (!g.clipRegionIntersects(target.getSmallestIntegerContainer())) return true;
            g.setOpacity(opacity);
            g.drawImage(indicatorScale.forSize(image,w,h),target);
            g.setOpacity(1.f);
            return true;
        }
       #endif
        if (g.clipRegionIntersects(target.getSmallestIntegerContainer())) {
            g.setOpacity(opacity);
            g.drawImage(image,target);
            g.setOpacity(1.f);
        }
        return true;
    }

    void sprite(juce::Graphics& g,const juce::String& id,juce::Point<float> origin,
                float opacity=1.f,float widthChange=0.f) const
    {
        auto* asset=findSprite(id);
        if(!asset) return;
        auto& layer=*asset;
        const auto position=origin+layer.offset;
        constexpr int endPixels=48;
        constexpr float endWidth=endPixels*.5f;
        const auto imageWidth=layer.image.getWidth();
        const auto imageHeight=layer.image.getHeight();
        const auto targetWidth=imageWidth*.5f+widthChange;
        g.setOpacity(opacity);
        if (widthChange!=0.f && imageWidth>2*endPixels && targetWidth>2*endWidth) {
            const auto targetPixels=juce::roundToInt(targetWidth*2.f);
            auto& resized=layer.resized[targetPixels];
            if (!resized.isValid()) {
                // Join at the original pixel scale so fractional draw offsets cannot expose a seam.
                resized=juce::Image(juce::Image::ARGB,targetPixels,imageHeight,true);
                juce::Graphics imageGraphics(resized);
                imageGraphics.drawImage(layer.image,0,0,endPixels,imageHeight,0,0,endPixels,imageHeight);
                imageGraphics.drawImage(layer.image,endPixels,0,targetPixels-2*endPixels,imageHeight,
                                        endPixels,0,imageWidth-2*endPixels,imageHeight);
                imageGraphics.drawImage(layer.image,targetPixels-endPixels,0,endPixels,imageHeight,
                                        imageWidth-endPixels,0,endPixels,imageHeight);
            }
            g.drawImageTransformed(resized,juce::AffineTransform::scale(targetWidth/float(targetPixels),.5f).translated(position));
        } else g.drawImageTransformed(layer.image,juce::AffineTransform::scale(.5f).translated(position));
        g.setOpacity(1.f);
    }

    juce::Rectangle<float> spriteBounds(const juce::String& id,juce::Point<float> origin) const
    {
        const auto* asset=findSprite(id);
        if(!asset) return {};
        const auto position=origin+asset->offset;
        return {position.x,position.y,asset->image.getWidth()*.5f,asset->image.getHeight()*.5f};
    }

    const juce::Font choiceFont;
    const SoulTrainKnob::Artwork knob;
    const WaveIndicator::Artwork wave;

private:
    struct Sprite { juce::Image image; juce::Point<float> offset; std::map<int,juce::Image> resized{}; };

    Sprite* findSprite(const juce::String& id) const
    {
        if(id.isEmpty()) return nullptr;
        auto item=images.find(id);
        if(item==images.end()) {
            const auto& metadata=manifest[juce::Identifier(id)];
            const auto sharedId=metadata["image"].toString();
            const auto imageId=sharedId.isEmpty()?id:sharedId;
            const auto image=juce::ImageCache::getFromFile(directory.getChildFile("sprites/"+imageId+".png"));
            if(!image.isValid()) return nullptr;
            item=images.emplace(id,Sprite{image,point(metadata["origin"])}).first;
        }
        return &item->second;
    }

    static juce::Typeface::Ptr loadFont(const juce::File& path)
    {
        juce::MemoryBlock contents;
        if(!path.loadFileAsData(contents)) throw std::runtime_error("SoulTrain font is missing: "+path.getFileName().toStdString());
        return juce::Typeface::createSystemTypefaceFor(contents.getData(),contents.getSize());
    }

    static std::map<int,juce::Typeface::Ptr> loadInter(const juce::File& folder)
    {
        std::map<int,juce::Typeface::Ptr> faces;
        for(const auto& item:{std::pair<int,const char*>{400,"Regular"},{500,"Medium"},{600,"SemiBold"},{700,"Bold"}})
            faces.emplace(item.first,loadFont(folder.getChildFile("Inter-"+juce::String(item.second)+".ttf")));
        return faces;
    }

    juce::File directory;
    juce::var manifest,controlStyles;
    mutable std::map<juce::String,Sprite> images;
    std::array<juce::Image,4> indicatorArt;
   #if JUCE_WINDOWS
    mutable silverK::ImageScaleCache indicatorScale;
   #endif
    juce::Typeface::Ptr pixel;
};

class Caption {
public:
    Caption(juce::Font f,juce::Rectangle<float> area,juce::Rectangle<float> clipArea,
            juce::Colour c,juce::String shadows={},float alpha=1.f,
            juce::Justification alignment=juce::Justification::centredLeft)
        : font(std::move(f)), bounds(area), clip(clipArea), colour(c.withMultipliedAlpha(alpha)),
          shadow(textShadows(shadows,alpha)), align(alignment) {}

    Caption(const SkinArt& art,const juce::var& data)
        : Caption(art.font(float(data["size"]),int(data["weight"]),data["font"].toString()=="pixel"),
                  rect(data["rect"]),rect(data["clip"]),soultrain::colour(data["colour"]),
                  data["shadow"].toString(),float(data["opacity"]))
    {
        set(data["text"].toString());
    }

    Caption restyled(juce::Colour ink,const juce::String& shadows) const
    {
        Caption result(font,bounds,clip,ink,shadows,1.f,align);
        result.set(current);
        return result;
    }

    void translate(juce::Point<float> delta)
    {
        bounds+=delta;
        inkBounds+=delta;
        glyphs.moveRangeOfGlyphs(0,glyphs.getNumGlyphs(),delta.x,delta.y);
        glow.translate(delta);
    }

    void setCentredText(juce::String value,juce::Rectangle<float> area,const juce::String& shorter={})
    {
        if(shorter.isNotEmpty() && font.getStringWidthFloat(value)>area.getWidth())
            value=shorter;
        const auto advance=font.getStringWidthFloat(value);
        bounds=bounds.withX(area.getCentreX()-advance*.5f).withWidth(advance);
        set(value);
    }

    void set(const juce::String& value,std::optional<CaptionGlow>* cachedGlow=nullptr)
    {
        if(value==current) return;
        current=value;
        glyphs.clear();
        const auto advance=font.getStringWidthFloat(value);
        auto x=bounds.getX();
        if(align.testFlags(juce::Justification::horizontallyCentred)) x+=std::max(0.f,(bounds.getWidth()-advance)*.5f);
        else if(align.testFlags(juce::Justification::right)) x+=bounds.getWidth()-advance;
        const auto baseline=bounds.getCentreY()+font.getAscent()-.5f*font.getHeight();
        const auto alternate=juce::String::fromUTF8("↶↷▴▸◂");
        for(int start=0;start<current.length();) {
            const auto fallback=alternate.containsChar(current[start]);
            auto end=start+1;
            while(end<current.length()&&alternate.containsChar(current[end])==fallback) ++end;
            const auto segment=current.substring(start,end);
#if JUCE_WINDOWS
            constexpr auto alternateFace="Segoe UI Symbol";
#else
            constexpr auto alternateFace="Arial Unicode MS";
#endif
            const auto face=fallback?juce::Font(alternateFace,12.f,juce::Font::plain)
                .withPointHeight(font.getHeight()*font.getTypefacePtr()->getHeightToPointsFactor()):font;
            glyphs.addLineOfText(face,segment,x,baseline);
            x+=face.getStringWidthFloat(segment);
            start=end;
        }
        if(cachedGlow && *cachedGlow) glow=**cachedGlow;
        else {
            glow=CaptionGlow(glyphs,shadow);
            if(cachedGlow) cachedGlow->emplace(glow);
        }
        inkBounds=glyphs.getBoundingBox(0,-1,false).getUnion(glow.paintBounds()).expanded(2.f);
    }

    void drawGlow(juce::Graphics& g,float travel,float opacity) const
    {
        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::translation(-travel,0.f));
        glow.draw(g,opacity);
    }

    juce::Rectangle<int> paintBounds() const
    {
        const auto area=inkBounds.getSmallestIntegerContainer();
        return clip.isEmpty()?area:area.getIntersection(clip.getSmallestIntegerContainer());
    }

    bool clipIntersects(const juce::Graphics& g) const
    {
        return g.clipRegionIntersects(paintBounds());
    }

    void draw(juce::Graphics& g,float travel=0.f,float opacity=1.f,bool includeGlow=true) const
    {
        juce::Graphics::ScopedSaveState save(g);
        if(!clip.isEmpty()&&!g.reduceClipRegion(clip.getSmallestIntegerContainer())) return;
        g.addTransform(juce::AffineTransform::translation(-travel,0.f));
        if(includeGlow) glow.draw(g,opacity);
        g.setColour(colour.withMultipliedAlpha(opacity));
        glyphs.draw(g);
    }

    juce::Rectangle<float> bounds;
    juce::String current;

private:
    juce::Font font;
    juce::Rectangle<float> clip;
    juce::Rectangle<float> inkBounds;
    juce::Colour colour;
    std::vector<TextShadow> shadow;
    juce::Justification align;
    juce::GlyphArrangement glyphs;
    CaptionGlow glow;
};
}
