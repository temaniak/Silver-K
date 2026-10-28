#pragma once
#include <juce_graphics/juce_graphics.h>
#include <algorithm>
#include <cmath>
#include <vector>

namespace silverK
{
// Message-thread-only cache for immutable skin artwork. The pinned Windows
// software renderer samples just four source pixels, even at high quality.
// Area-filtered half-size levels prevent fine textures from aliasing.
class ImageScaleCache
{
public:
    juce::Image forDrawing(juce::Graphics& g, const juce::Image& source,
                          float width, float height)
    {
#if JUCE_WINDOWS
        return forSoftwareDrawing(g, source, width, height);
#else
        // Preserve CoreGraphics rendering on macOS.
        return source;
#endif
    }

    juce::Image forSoftwareDrawing(juce::Graphics& g, const juce::Image& source,
                                  float width, float height)
    {
        // Includes both the editor transform and the monitor/host DPI scale.
        const float scale=g.getInternalContext().getPhysicalPixelScaleFactor();
        return forSize(source, juce::roundToInt(std::ceil(width*scale)),
                       juce::roundToInt(std::ceil(height*scale)));
    }

    juce::Image forSize(const juce::Image& source, int width, int height)
    {
        if(!source.isValid() || width<=0 || height<=0)return source;
        width=juce::jmin(width,source.getWidth());
        height=juce::jmin(height,source.getHeight());
        if(width==source.getWidth() && height==source.getHeight())return source;

        auto found=std::find_if(images.begin(),images.end(),[&](const Source& s){return s.levels.front()==source;});
        if(found==images.end()){
            if(images.size()==8)images.erase(images.begin());
            images.push_back({{source},{}});found=images.end()-1;
        }
        auto& entry=*found;
        for(const auto& size:entry.sizes)
            if(size.getWidth()==width && size.getHeight()==height)return size;

        size_t level=0;
        while(entry.levels[level].getWidth()/2>=width && entry.levels[level].getHeight()/2>=height){
            if(level+1==entry.levels.size()){
                const auto previous=entry.levels[level];
                entry.levels.push_back(resize(previous,(previous.getWidth()+1)/2,(previous.getHeight()+1)/2));
            }
            ++level;
        }
        auto result=resize(entry.levels[level],width,height);
        // Bound memory during continuous editor resizing. Large half-size levels
        // remain reusable; only the small final sizes are replaced.
        if(entry.sizes.size()==8)entry.sizes.erase(entry.sizes.begin());
        entry.sizes.push_back(result);
        return result;
    }

private:
    struct Source
    {
        std::vector<juce::Image> levels, sizes;
    };
    std::vector<Source> images;

    static juce::Image resize(const juce::Image& source, int width, int height)
    {
        if(source.getWidth()==width && source.getHeight()==height)return source;
        const auto argb=source.convertedToFormat(juce::Image::ARGB);
        const juce::Image::BitmapData input(argb,juce::Image::BitmapData::readOnly);
        juce::Image result(juce::Image::ARGB,width,height,true,juce::SoftwareImageType());
        juce::Image::BitmapData output(result,juce::Image::BitmapData::writeOnly);
        const double dx=double(input.width)/width,dy=double(input.height)/height;
        for(int y=0;y<height;++y){
            const double top=y*dy,bottom=(y+1)*dy;
            auto* row=reinterpret_cast<juce::PixelARGB*>(output.getLinePointer(y));
            for(int x=0;x<width;++x){
                const double left=x*dx,right=(x+1)*dx;
                double a=0,r=0,g=0,b=0;
                for(int sy=int(top);sy<juce::jmin(input.height,int(std::ceil(bottom)));++sy){
                    const double wy=juce::jmin(bottom,double(sy+1))-juce::jmax(top,double(sy));
                    const auto* pixels=reinterpret_cast<const juce::PixelARGB*>(input.getLinePointer(sy));
                    for(int sx=int(left);sx<juce::jmin(input.width,int(std::ceil(right)));++sx){
                        const double weight=wy*(juce::jmin(right,double(sx+1))-juce::jmax(left,double(sx)));
                        const auto& p=pixels[sx];
                        a+=p.getAlpha()*weight;r+=p.getRed()*weight;
                        g+=p.getGreen()*weight;b+=p.getBlue()*weight;
                    }
                }
                const auto channel=[=](double value){return juce::uint8(juce::jlimit(0,255,juce::roundToInt(value/(dx*dy))));};
                // Average premultiplied channels together with alpha to retain
                // transparent edges without dark fringes or repeated AA loss.
                row[x].setARGB(channel(a),channel(r),channel(g),channel(b));
            }
        }
        return result;
    }
};
}
