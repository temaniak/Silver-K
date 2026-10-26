#include "silverEditor.h"
#include "silverBrowser.h"
#include "silverPerformance.h"
#include "BinaryData.h"
#include <cmath>
#include <set>

namespace silverK
{
namespace
{
using Spec = ksPlugin::Control;
bool validSkin(const juce::var& v)
{
    const bool studio=v["layout"].toString()=="studio";
    return v.isObject() && v["controls"].isArray() && v["effects"].isArray()
        && (studio ? ((int(v["width"])==1440 && int(v["height"])==1122) || (int(v["width"])==1280 && int(v["height"])==1000)) && v["studioRegions"].isObject()
                       && v["studioModels"]["K"].isArray() && v["studioModels"]["KS"].isArray()
                   : int(v["width"])==1280 && int(v["height"])==752);
}
const juce::Colour silver(0xffced1d5), ink(0xff171b20), blue(0xff087fe5), dark(0xff24262a), light(0xfff1f4f7);
juce::Rectangle<int> rect(const juce::var& v)
{
    auto* a=v.getArray();
    return a && a->size()==4 ? juce::Rectangle<int>(int((*a)[0]),int((*a)[1]),int((*a)[2]),int((*a)[3])) : juce::Rectangle<int>{};
}
juce::var skinData(Processor& p)
{
    int size=0;const auto* bytes=BinaryData::getNamedResource("silverk_json",size);
    auto skin=juce::JSON::parse(juce::String::fromUTF8(bytes,size));
    if(p.skinChoice=="Custom" && juce::File(p.customSkin).existsAsFile()){
      auto custom=juce::JSON::parse(juce::File(p.customSkin));
      if(validSkin(custom))skin=custom;
    }
    return skin;
}
class SilverLook final : public juce::LookAndFeel_V4
{
    juce::Image knob,grip,standBase,volumeBase,buttonOff,buttonOn;
public:
    SilverLook()
    {
        int size=0; const auto* data=BinaryData::getNamedResource("silverkknobbody_png",size);
        knob=juce::ImageFileFormat::loadFrom(data,size);
        data=BinaryData::getNamedResource("silverkknobgrip_png",size);
        grip=juce::ImageFileFormat::loadFrom(data,size);
        data=BinaryData::getNamedResource("silverkstandknobbase_png",size);
        standBase=juce::ImageFileFormat::loadFrom(data,size);
        volumeBase=standBase.createCopy();
        if(volumeBase.isValid()) {
            juce::Image::BitmapData pixels(volumeBase,juce::Image::BitmapData::readWrite);
            const float cx=volumeBase.getWidth()*.5f,cy=volumeBase.getHeight()*.5f;
            const float radius=volumeBase.getWidth()*.347f;
            for(int y=0;y<pixels.height;++y)for(int x=0;x<pixels.width;++x) {
                const float dx=x+.5f-cx,dy=y+.5f-cy;
                if(dx*dx+dy*dy>radius*radius)continue;
                const auto c=pixels.getPixelColour(x,y);const float l=c.getBrightness();
                pixels.setPixelColour(x,y,juce::Colour::fromFloatRGBA(.025f+l*.3f,.13f+l*.75f,juce::jmin(1.f,.28f+l*.9f),c.getFloatAlpha()));
            }
        }
        data=BinaryData::getNamedResource("silverkbuttonoff_png",size);buttonOff=juce::ImageFileFormat::loadFrom(data,size);
        data=BinaryData::getNamedResource("silverkbuttonon_png",size);buttonOn=juce::ImageFileFormat::loadFrom(data,size);
        setColour(juce::PopupMenu::backgroundColourId,juce::Colour(0xff202a34));
        setColour(juce::PopupMenu::textColourId,light);
        setColour(juce::PopupMenu::highlightedBackgroundColourId,blue);
        setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
        setColour(juce::ComboBox::textColourId,light);
        setColour(juce::ToggleButton::textColourId,ink);
        setColour(juce::ToggleButton::tickColourId,blue);
        setColour(juce::ToggleButton::tickDisabledColourId,ink.withAlpha(.4f));
    }
    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* field=LookAndFeel_V4::createSliderTextBox(slider);
        field->setColour(juce::Label::outlineColourId,juce::Colours::transparentBlack);
        field->setColour(juce::Label::outlineWhenEditingColourId,juce::Colours::transparentBlack);
        field->setColour(juce::TextEditor::outlineColourId,juce::Colours::transparentBlack);
        field->setColour(juce::TextEditor::focusedOutlineColourId,juce::Colours::transparentBlack);
        field->setColour(juce::TextEditor::backgroundColourId,juce::Colours::transparentBlack);
        return field;
    }
    void drawRotarySlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float start,float end,juce::Slider& slider) override
    {
        const float available=float(juce::jmin(w,h))-4;
        const float requested=slider.getProperties().contains("rotaryDiameter")?float(slider.getProperties()["rotaryDiameter"]):available;
        const float d=juce::jmin(available,requested),cx=x+w*.5f,cy=y+h*.5f;
        const float position=juce::jlimit(0.f,1.f,pos);
        const float angle=start+position*(end-start);
        if(bool(slider.getProperties()["standKnob"]) && standBase.isValid())
        {
            // This artwork never rotates: material, ribs and the upper-left light
            // stay fixed. Only the live indicator and the lit arc change.
            const float imageDiameter=d*1.15f;
            const auto& artwork=bool(slider.getProperties()["blueVolume"])?volumeBase:standBase;
            g.drawImage(artwork,cx-imageDiameter*.5f,cy-imageDiameter*.5f,imageDiameter,imageDiameter,0,0,artwork.getWidth(),artwork.getHeight());
            if(slider.getProperties()["rotaryStyle"].toString()!="classic")for(int i=0;i<21;++i)
            {
                const float a=start+(end-start)*i/20.f,r=d*.45f,dot=juce::jmax(2.f,d*.037f);
                const float dx=cx+std::sin(a)*r,dy=cy-std::cos(a)*r;
                const bool lit=float(i)/20.f<=position;
                if(lit){g.setColour(juce::Colour(0xff008bff).withAlpha(.16f));g.fillEllipse(dx-dot,dy-dot,dot*2,dot*2);}
                g.setColour(lit?juce::Colour(0xff25c3ff):juce::Colour(0xff26303b));g.fillEllipse(dx-dot*.5f,dy-dot*.5f,dot,dot);
            }
            const float markerWidth=juce::jmax(1.5f,d*.026f);
            juce::Path marker;marker.addRoundedRectangle(-markerWidth*.5f,-d*.31f,markerWidth,d*.15f,markerWidth*.5f);
            g.setColour(light);g.fillPath(marker,juce::AffineTransform::rotation(angle).translated(cx,cy));
            return;
        }
        if(slider.getProperties()["rotaryStyle"].toString()=="classic")
        {
            // The same cap size as the main knobs, with a smooth bevel instead
            // of the rubber grip and LED surround. Its lighting remains fixed.
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff737a82),cx-d*.3f,cy-d*.4f,
                juce::Colour(0xff10141a),cx+d*.3f,cy+d*.4f,false));
            g.fillEllipse(cx-d*.5f,cy-d*.5f,d,d);
            const float faceDiameter=d*.91f,cap=faceDiameter/.674f;
            if(knob.isValid())
            {
                juce::Graphics::ScopedSaveState saved(g);
                juce::Path face;face.addEllipse(cx-faceDiameter*.5f,cy-faceDiameter*.5f,faceDiameter,faceDiameter);g.reduceClipRegion(face);
                g.drawImageTransformed(knob,juce::AffineTransform::scale(cap/knob.getWidth(),cap/knob.getHeight())
                    .followedBy(juce::AffineTransform::translation(cx-cap*.5f,cy-cap*.5f)));
            }
            const float markerWidth=juce::jmax(1.5f,d*.025f);
            juce::Path marker;marker.addRoundedRectangle(-markerWidth*.5f,-d*.39f,markerWidth,d*.17f,markerWidth*.5f);
            g.setColour(light);g.fillPath(marker,juce::AffineTransform::rotation(angle).translated(cx,cy));
            return;
        }
        g.setColour(juce::Colour(0xff10151c));g.fillEllipse(cx-d*.5f,cy-d*.5f,d,d);
        for(int i=0;i<21;++i)
        {
            const float a=start+(end-start)*i/20.f,r=d*.45f;
            // Do not illuminate a step beyond the pointer by rounding upwards.
            g.setColour(float(i)/20.f<=position?juce::Colour(0xff4cb2ff):juce::Colour(0xff27303b));
            const float dot=juce::jmax(2.f,d*.036f);
            g.fillEllipse(cx+std::sin(a)*r-dot*.5f,cy-std::cos(a)*r-dot*.5f,dot,dot);
        }
        const float cap=d*.87f;
        juce::Path ring;
        ring.addEllipse(cx-cap*.455f,cy-cap*.455f,cap*.91f,cap*.91f);
        ring.addEllipse(cx-cap*.329f,cy-cap*.329f,cap*.658f,cap*.658f);
        ring.setUsingNonZeroWinding(false);
        if(grip.isValid())
        {
            juce::Graphics::ScopedSaveState saved(g);
            g.reduceClipRegion(ring);
            // Rotate neutral rubber geometry, then light it in panel space.
            g.drawImageTransformed(grip,juce::AffineTransform::scale(cap/grip.getWidth(),cap/grip.getHeight())
                .followedBy(juce::AffineTransform::translation(-cap*.5f,-cap*.5f))
                .followedBy(juce::AffineTransform::rotation(angle))
                .followedBy(juce::AffineTransform::translation(cx,cy)));
            g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(.08f),cx-cap*.4f,cy-cap*.4f,
                juce::Colours::black.withAlpha(.55f),cx+cap*.4f,cy+cap*.4f,false));
            g.fillPath(ring);
        }
        if(knob.isValid())
        {
            juce::Graphics::ScopedSaveState saved(g);
            juce::Path face;face.addEllipse(cx-cap*.337f,cy-cap*.337f,cap*.674f,cap*.674f);g.reduceClipRegion(face);
            g.drawImageTransformed(knob,juce::AffineTransform::scale(cap/knob.getWidth(),cap/knob.getHeight())
                .followedBy(juce::AffineTransform::translation(cx-cap*.5f,cy-cap*.5f)));
        }
        // The face and scene lighting stay fixed; grip and marker turn together.
        const float markerWidth=juce::jmax(1.5f,cap*.021f);
        juce::Path marker;
        marker.addRoundedRectangle(-markerWidth*.5f,-cap*.32f,markerWidth,cap*.14f,markerWidth*.5f);
        g.setColour(juce::Colour(0xfff4f5f7));
        g.fillPath(marker,juce::AffineTransform::rotation(angle).translated(cx,cy));
    }
    void drawLinearSlider(juce::Graphics& g,int x,int y,int w,int h,float pos,float,float,juce::Slider::SliderStyle style,juce::Slider& s) override
    {
        if(style!=juce::Slider::LinearVertical){LookAndFeel_V4::drawLinearSlider(g,x,y,w,h,pos,0,0,style,s);return;}
        const float cx=x+w*.5f;
        g.setColour(ink.withAlpha(.4f));
        for(int i=0;i<=10;++i){float yy=y+h*i/10.f;g.drawLine(cx-w*.32f,yy,cx+w*.32f,yy,1.f);}
        g.setColour(juce::Colour(0xff0b0d10));g.fillRoundedRectangle(cx-3,float(y),6,float(h),2);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff5b6068),cx,pos-8,juce::Colour(0xff11151b),cx,pos+9,false));
        g.fillRoundedRectangle(cx-18,pos-8,36,16,5);
        g.setColour(light.withAlpha(.8f));g.drawLine(cx-12,pos-5,cx+12,pos-5,1.5f);
    }
    static juce::Font selectorFont(const juce::Button& b)
    {
        const bool compact=bool(b.getProperties()["compact"])||(b.getParentComponent()&&bool(b.getParentComponent()->getProperties()["compact"]));
        return juce::Font(compact||bool(b.getProperties()["small"])?11.f:13.f,juce::Font::bold);
    }
    static float capWidth(const juce::Button& b)
    {
        return juce::jlimit(14.f,26.f,float(b.getWidth())-selectorFont(b).getStringWidthFloat(b.getButtonText())-10.f);
    }
    void drawCap(juce::Graphics& g,bool on,juce::Rectangle<float> r,bool enabled,bool down)
    {
        const auto& image=on?buttonOn:buttonOff;
        g.setOpacity(enabled?1.f:.4f);g.drawImage(image,r.translated(0,down?1.f:0.f),juce::RectanglePlacement::stretchToFit);g.setOpacity(1.f);
    }
    void drawButtonBackground(juce::Graphics& g,juce::Button& b,const juce::Colour&,bool hover,bool down) override
    {
        auto bounds=b.getLocalBounds().toFloat().reduced(1);
        const bool on=b.getToggleState();
        if(b.getProperties().contains("waveform"))
        {
            if(on||hover){g.setColour(blue.withAlpha(on?.13f:.07f));g.fillRoundedRectangle(bounds,3);}
            const float cx=bounds.getCentreX();
            drawCap(g,on,{cx-10,2,20,8},b.isEnabled(),down);
            const float left=4,right=float(b.getWidth()-4),top=14,bottom=24;
            juce::Path shape;const int wave=int(b.getProperties()["waveform"]);
            if(wave==0){for(int n=0;n<=24;++n){float x=left+(right-left)*n/24.f,y=(top+bottom)*.5f-std::sin(float(n)*juce::MathConstants<float>::twoPi/24.f)*(bottom-top)*.5f;if(n==0)shape.startNewSubPath(x,y);else shape.lineTo(x,y);}}
            else if(wave==1){shape.startNewSubPath(left,bottom);shape.lineTo((left+right)*.5f,top);shape.lineTo(right,bottom);}
            else if(wave==2){shape.startNewSubPath(left,bottom);shape.lineTo(right,top);shape.lineTo(right,bottom);}
            else{shape.startNewSubPath(left,bottom);shape.lineTo(left,top);shape.lineTo((left+right)*.5f,top);shape.lineTo((left+right)*.5f,bottom);shape.lineTo(right,bottom);}
            g.setColour(b.isEnabled()?ink:ink.withAlpha(.4f));g.strokePath(shape,juce::PathStrokeType(1.6f));return;
        }
        if(bool(b.getProperties()["partTab"])){
            juce::Path tab;tab.addRoundedRectangle(bounds.getX(),bounds.getY(),bounds.getWidth(),bounds.getHeight(),5,5,true,true,false,false);
            g.setGradientFill(juce::ColourGradient(on?juce::Colour(0xffe8ebef):juce::Colour(0xff666d78),0,bounds.getY(),on?juce::Colour(0xffbec3cb):juce::Colour(0xff343a43),0,bounds.getBottom(),false));g.fillPath(tab);g.setColour(juce::Colour(0xff243041));g.strokePath(tab,juce::PathStrokeType(1));if(on){g.setColour(blue);g.fillRect(bounds.removeFromBottom(3));}return;
        }
        if(bool(b.getProperties()["led"]))
        {
            if(hover){g.setColour(blue.withAlpha(.08f));g.fillRoundedRectangle(bounds,3);}
            const float width=capWidth(b);drawCap(g,on,{3,bounds.getCentreY()-width/4.8f,width,width/2.4f},b.isEnabled(),down);
            return;
        }
        const auto top=on?juce::Colour(0xff1695ee):juce::Colour(0xff434a54);
        const auto bottom=on?juce::Colour(0xff0752a7):juce::Colour(0xff171e28);
        g.setGradientFill(juce::ColourGradient(hover?top.brighter(.12f):top,0,bounds.getY(),down?bottom.darker(.2f):bottom,0,bounds.getBottom(),false));
        g.fillRoundedRectangle(bounds,4);g.setColour(juce::Colour(0xff10151c));g.drawRoundedRectangle(bounds,4,1);
        g.setColour(light.withAlpha(.22f));g.drawLine(bounds.getX()+3,bounds.getY()+2,bounds.getRight()-3,bounds.getY()+2);
    }
    void drawButtonText(juce::Graphics& g,juce::TextButton& b,bool,bool) override
    {
        if(b.getProperties().contains("waveform"))return;
        const bool compact=bool(b.getProperties()["compact"]) || (b.getParentComponent()&&bool(b.getParentComponent()->getProperties()["compact"]));
        const auto textColour=(bool(b.getProperties()["partTab"])&&b.getToggleState())?ink:(bool(b.getProperties()["led"])&&!bool(b.getProperties()["dark"])?ink:light);
        g.setColour(b.isEnabled()?textColour:textColour.withAlpha(.4f));
        g.setFont(juce::Font((compact||bool(b.getProperties()["small"]))?11.f:13.f,juce::Font::bold));
        auto area=b.getLocalBounds().reduced(3);
        if(bool(b.getProperties()["led"]))area.removeFromLeft(juce::roundToInt(capWidth(b))+5);
        g.drawFittedText(b.getButtonText(),area,juce::Justification::centred,1);
    }
    void drawComboBox(juce::Graphics& g,int w,int h,bool,int,int,int,int,juce::ComboBox& c) override
    {
        if(!bool(c.getProperties()["performancePreset"])) {
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff344150),0,0,juce::Colour(0xff1a242e),0,float(h),false));
        g.fillRoundedRectangle(0,0,float(w),float(h),3);
        g.setColour(juce::Colour(0xff657180));g.drawRoundedRectangle(.5f,.5f,float(w)-1,float(h)-1,3,1);
        }
        g.setColour(light);juce::Path arrow;arrow.addTriangle(float(w-15),h*.43f,float(w-7),h*.43f,float(w-11),h*.62f);g.fillPath(arrow);
    }
    juce::Font getComboBoxFont(juce::ComboBox& c) override{if(bool(c.getProperties()["performancePreset"]))return juce::Font(juce::Font::getDefaultMonospacedFontName(),16.f,juce::Font::plain);return juce::Font(bool(c.getProperties()["compact"])?11.5f:12.5f);}
};
struct ControlSlider final : juce::Slider
{
    bool wheelGesture=false;
    void mouseDown(const juce::MouseEvent& e) override {if(!e.mods.isPopupMenu())juce::Slider::mouseDown(e);}
    void mouseDrag(const juce::MouseEvent& e) override {if(!e.mods.isPopupMenu())juce::Slider::mouseDrag(e);}
    void mouseUp(const juce::MouseEvent& e) override {if(!e.mods.isPopupMenu())juce::Slider::mouseUp(e);}
    void mouseWheelMove(const juce::MouseEvent& event,const juce::MouseWheelDetails& wheel) override
    {
        const juce::ScopedValueSetter<bool> guard(wheelGesture,true);
        juce::Slider::mouseWheelMove(event,wheel);
    }
};
struct LfoIndicator final : juce::Label
{
    void paint(juce::Graphics& g) override
    {
        const bool on=bool(getProperties()["selected"]);
        const float y=getHeight()*.5f-6;
        g.setColour(on?juce::Colour(0xffb21718):juce::Colour(0xff26292d));g.fillEllipse(0,y,12,12);
        g.setColour(on?juce::Colour(0xffff5450):juce::Colour(0xff6b7076));g.fillEllipse(3,y+2,5,3);
        juce::Label::paint(g);
    }
};
struct Control final : juce::Component
{
    juce::RangedAudioParameter& parameter;
    const Spec& spec;
    juce::Label label;
    juce::Label valueLabel;
    ControlSlider slider;
    juce::ComboBox menu;
    juce::TextButton button;
    std::vector<std::unique_ptr<juce::TextButton>> lamps;
    juce::String kind;
    bool gesture=false;
    std::function<void(const juce::String&,bool)> report;
    Control(juce::RangedAudioParameter& p,const Spec& s,juce::String caption,juce::String type,bool isDark,
            std::function<void(const juce::String&,bool)> changed):parameter(p),spec(s),kind(type),report(std::move(changed))
    {
        setComponentID(juce::String("control:")+s.id);
        addAndMakeVisible(label);label.setText(caption,juce::dontSendNotification);
        label.setFont(juce::Font(12.5f));label.setColour(juce::Label::textColourId,isDark?light:ink);
        label.setJustificationType(juce::Justification::centred);
        setTooltipForControls(juce::String(s.group)+" / "+s.section+" / "+s.label);
        if((kind=="lamps" || kind=="waveButtons") && s.enumCount)
        {
            label.setVisible(kind=="waveButtons");
            for(int i=0;i<s.enumCount;++i)
            {
                auto b=std::make_unique<juce::TextButton>();
                juce::String text=s.enumLabels[i];
                if(juce::String(s.id)=="filter_type")text=juce::StringArray{"LP","BP","HP"}[i];
                if(juce::String(s.id)=="filter_slope")text=i?"24 dB":"12 dB";
                b->setButtonText(text);b->setComponentID(juce::String(s.id)+":"+juce::String(i));
                b->getProperties().set("led",true);b->getProperties().set("dark",isDark);
                if(kind=="waveButtons")b->getProperties().set("waveform",i);
                b->setTooltip(juce::String(s.label)+": "+s.enumLabels[i]);
                b->onClick=[this,i]{parameter.beginChangeGesture();setValue(i);parameter.endChangeGesture();notify();};
                addAndMakeVisible(*b);lamps.push_back(std::move(b));
            }
        }
        else if(kind=="led")
        {
            label.setVisible(false);addAndMakeVisible(button);button.setComponentID(s.id);button.setButtonText(caption);
            button.getProperties().set("led",true);button.getProperties().set("dark",isDark);button.setClickingTogglesState(true);
            button.onClick=[this]{parameter.beginChangeGesture();setValue(button.getToggleState()?1:0);parameter.endChangeGesture();notify();};
        }
        else if(s.enumCount || kind=="rhythm")
        {
            addAndMakeVisible(menu);menu.setComponentID(s.id);
            if(kind=="rhythm"){menu.addItem("Off",1);for(int i=1;i<=s.maximum;++i)menu.addItem(juce::String(i),i+1);}
            else for(int i=0;i<s.enumCount;++i)menu.addItem(s.enumLabels[i],i+1);
            menu.onChange=[this]{parameter.beginChangeGesture();setValue(menu.getSelectedId()-1);parameter.endChangeGesture();notify();};
        }
        else
        {
            addAndMakeVisible(slider);slider.setComponentID(s.id);slider.setRange(s.minimum,s.maximum,1);
            const bool row=kind=="row" || kind=="extraRow";
            slider.setSliderStyle(row?juce::Slider::LinearHorizontal:(kind=="fader"?juce::Slider::LinearVertical:juce::Slider::RotaryVerticalDrag));
            slider.setRotaryParameters(juce::MathConstants<float>::pi*4.f/3.f,juce::MathConstants<float>::pi*8.f/3.f,true);
            slider.setTextBoxStyle(row?juce::Slider::TextBoxRight:juce::Slider::TextBoxBelow,false,row?56:64,row?24:18);
            if(kind=="mixer")slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
            if(kind=="panelKnob" || kind=="miniKnob")
            {
                slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
                addAndMakeVisible(valueLabel);valueLabel.setComponentID(juce::String("value:")+s.id);
                valueLabel.setJustificationType(juce::Justification::centred);valueLabel.setFont(juce::Font(13.f));
                valueLabel.setColour(juce::Label::textColourId,isDark?light:ink);
                valueLabel.setColour(juce::Label::outlineWhenEditingColourId,blue);
                valueLabel.setColour(juce::TextEditor::backgroundColourId,isDark?dark:light);
                valueLabel.setColour(juce::TextEditor::textColourId,isDark?light:ink);
                valueLabel.setEditable(false,kind!="miniKnob",false);valueLabel.setTooltip(juce::String(s.label)+" — double-click to enter a value");
                valueLabel.onTextChange=[this]
                {
                    const int value=juce::jlimit(int(spec.minimum),int(spec.maximum),valueLabel.getText().getIntValue()-spec.displayOffset);
                    parameter.beginChangeGesture();setValue(value);parameter.endChangeGesture();refresh();notify();
                };
            }
            slider.setColour(juce::Slider::textBoxTextColourId,isDark?light:ink);
            slider.setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
            slider.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::transparentBlack);
            slider.setColour(juce::Slider::trackColourId,blue);
            slider.setDoubleClickReturnValue(true,s.defaultValue);slider.setPopupDisplayEnabled(false,false,nullptr);
            slider.textFromValueFunction=[this](double v){return juce::String(juce::roundToInt(v)+spec.displayOffset);};
            slider.valueFromTextFunction=[this](const juce::String& t){return t.getDoubleValue()-spec.displayOffset;};
            slider.onDragStart=[this]{gesture=true;parameter.beginChangeGesture();notify();};
            slider.onDragEnd=[this]{parameter.endChangeGesture();gesture=false;if(slider.wheelGesture)notify();else report({},false);};
            slider.onValueChange=[this]
            {
                if(!gesture)parameter.beginChangeGesture();
                setValue(juce::roundToInt(slider.getValue()));
                if(!gesture)parameter.endChangeGesture();
                notify();
            };
        }
        refresh();
        addMouseListener(this,true);
    }
    ~Control() override{if(gesture)parameter.endChangeGesture();}
    void setValue(int v){parameter.setValueNotifyingHost(parameter.convertTo0to1(float(v)));}
    void mouseExit(const juce::MouseEvent&) override{if(!gesture)report({},false);}
    void setTooltipForControls(const juce::String& t){label.setTooltip(t);slider.setTooltip(t);menu.setTooltip(t);button.setTooltip(t);}
    void notify()
    {
        const auto v=int(std::round(parameter.convertFrom0to1(parameter.getValue())));
        report(juce::String(spec.label)+"\n"+(spec.enumCount?juce::String(spec.enumLabels[juce::jlimit(0,int(spec.enumCount)-1,v)]):juce::String(v+spec.displayOffset)),gesture);
    }
    void refresh()
    {
        const auto v=int(std::round(parameter.convertFrom0to1(parameter.getValue())));
        if(kind=="lamps" || kind=="waveButtons"){for(size_t i=0;i<lamps.size();++i)lamps[i]->setToggleState(int(i)==v,juce::dontSendNotification);}
        else if(kind=="led")button.setToggleState(v!=0,juce::dontSendNotification);
        else if(spec.enumCount || kind=="rhythm")menu.setSelectedId(v+1,juce::dontSendNotification);
        else if(!slider.isMouseButtonDown())
        {
            bool editing=false;
            for(int i=0;i<slider.getNumChildComponents();++i)
                if(auto* field=dynamic_cast<juce::Label*>(slider.getChildComponent(i)))editing|=field->isBeingEdited();
            if(!editing){slider.setValue(v,juce::dontSendNotification);slider.updateText();}
        }
        if((kind=="panelKnob" || kind=="miniKnob") && !valueLabel.isBeingEdited())valueLabel.setText(juce::String(v+spec.displayOffset),juce::dontSendNotification);
    }
    void resized() override
    {
        auto area=getLocalBounds();
        if(kind=="panelKnob")
        {
            if(bool(getProperties()["compact"])){
                slider.setBounds(0,0,getWidth(),76);label.setFont(juce::Font(11.5f));label.setBorderSize(juce::BorderSize<int>(0));label.setBounds(0,76,getWidth(),16);
                valueLabel.setFont(juce::Font(12.f));valueLabel.setBounds(0,92,getWidth(),18);return;
            }
            slider.setBounds(0,0,getWidth(),84);label.setBounds(0,84,getWidth(),18);
            valueLabel.setBounds(0,102,getWidth(),20);return;
        }
        if(kind=="extraRow")
        {
            label.setFont(juce::Font(11.f));label.setJustificationType(juce::Justification::centredLeft);
            label.setBounds(0,0,getWidth(),12);slider.setBounds(0,15,getWidth(),getHeight()-15);menu.setBounds(0,15,getWidth(),juce::jmin(24,getHeight()-15));return;
        }
        if(kind=="waveButtons"){
            label.setJustificationType(juce::Justification::centredLeft);label.setBounds(area.removeFromTop(20));
            for(size_t i=0;i<lamps.size();++i)lamps[i]->setBounds(int(i)*getWidth()/int(lamps.size()),20,getWidth()/int(lamps.size())-2,28);return;
        }
        if(kind=="lamps"){for(size_t i=0;i<lamps.size();++i)lamps[i]->setBounds(int(i)*getWidth()/int(lamps.size()),0,getWidth()/int(lamps.size()),getHeight());return;}
        if(kind=="miniKnob")
        {
            label.setFont(juce::Font(11.f));label.setJustificationType(juce::Justification::centredLeft);
            label.setBounds(0,0,getWidth(),20);slider.setBounds(0,23,36,32);valueLabel.setBounds(42,23,42,32);return;
        }
        if(kind=="panelMenu")
        {
            label.setJustificationType(juce::Justification::centredLeft);label.setBounds(area.removeFromTop(20));menu.setBounds(area);return;
        }
        if(kind=="inline" || kind=="rhythm")
        {
            label.setFont(juce::Font(11.f));label.setBorderSize(juce::BorderSize<int>(0));
            label.setJustificationType(juce::Justification::centredLeft);label.setBounds(area.removeFromLeft(getProperties().contains("captionWidth")?int(getProperties()["captionWidth"]):54));
            menu.setBounds(area.reduced(0,2));return;
        }
        if(kind=="led"){button.setBounds(area);return;}
        if(kind=="row")
        {
            label.setJustificationType(juce::Justification::centredLeft);label.setBounds(area.removeFromLeft(166));
            slider.setBounds(area);menu.setBounds(area.reduced(0,5));return;
        }
        if(kind=="mixer")
        {
            label.setJustificationType(juce::Justification::centredLeft);label.setBounds(area.removeFromLeft(51));slider.setBounds(area);return;
        }
        if(kind=="compact")
        {
            label.setFont(juce::Font(11.f));label.setBounds(area.removeFromLeft(39));slider.setBounds(area);return;
        }
        label.setBounds(area.removeFromTop(22));
        if(spec.enumCount)menu.setBounds(area.removeFromTop(juce::jmin(28,area.getHeight())).reduced(2,0));
        else slider.setBounds(area);
    }
};
struct Panel final : juce::Component
{
    juce::String title;
    std::vector<std::unique_ptr<Control>> controls;
    int rowHeight=40;
    void paint(juce::Graphics& g) override
    {
        g.setColour(ink.withAlpha(.1f));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),4,1);
        g.setColour(ink);g.setFont(juce::Font(14.f,juce::Font::bold));g.drawText(title,12,6,getWidth()-24,24,juce::Justification::centredLeft);
    }
    void resized() override{int y=34;for(auto& c:controls){c->setBounds(12,y,getWidth()-24,rowHeight);y+=rowHeight;}}
};
struct LocalExtra final : juce::Component
{
    juce::TextButton back{"Back"};
    juce::Component content;
    juce::String title;
    std::vector<std::unique_ptr<Control>> controls;
    juce::Colour background=light;
    bool dark=false,compact=false;
    int columns=1,rowHeight=43,gap=7;
    std::function<void()> onClose;
    LocalExtra()
    {
        setComponentID("localExtra");addAndMakeVisible(back);addAndMakeVisible(content);
        back.setComponentID("extraBack");back.onClick=[this]{setVisible(false);if(onClose)onClose();};setVisible(false);
    }
    void paint(juce::Graphics& g) override
    {
        g.fillAll(background);g.setColour(dark?light:ink);g.setFont(juce::Font(15.f,juce::Font::bold));
        g.drawText(title,8,8,getWidth()-80,28,juce::Justification::centredLeft);
    }
    void resized() override
    {
        back.setBounds(getWidth()-62,8,54,28);content.setBounds(8,compact?42:46,getWidth()-16,getHeight()-(compact?50:54));
        const int width=(content.getWidth()-(columns-1)*12)/columns;
        for(size_t n=0;n<controls.size();++n)controls[n]->setBounds(int(n)%columns*(width+12),int(n)/columns*(rowHeight+gap),width,rowHeight);
    }
};}

struct Editor::Surface final : juce::Component, private juce::Timer
{
    Processor& processor;
    juce::var skin;
    SilverLook look;
    juce::TooltipWindow tooltip{this,750};
    juce::Component main, content;
    juce::Viewport viewport;
    juce::Label lcd,status,latency,help;
    juce::TextButton loadRom{"Load firmware"},folder{"Silver-K folder"},loadProgram{"Load program"},panic{"Panic"};
    juce::ComboBox mode,group,section,channel;
    std::array<juce::TextButton,5> pages;
    std::array<juce::TextButton,4> parts;
    juce::ComboBox firmwareSelect,skinSelect,bufferSelect,controllerSelect;
    juce::TextButton rescanRoms{"Rescan ROMs"};
    juce::TextButton diagnostics{"Performance diagnostics"};
    juce::TextButton waveRom{"Wave ROM"},factory{"KS factory data"},customSkin{"Load skin JSON"};
    juce::ToggleButton performanceMode{"Performance (4 parts)"},partActive{"Part active"};
    juce::TextButton performanceSwitch{"Performance"};
    Browser browser;
    std::unique_ptr<PerformancePanel> performancePanel;
    bool displayedK=false,displayedPerformance=false;
    unsigned displayedPart=0;
    std::array<juce::TextButton,3> oscillators;
    juce::TextButton lfoSelect{"LFO SELECT"};
    std::array<LfoIndicator,2> lfoIndicators;
    std::vector<std::unique_ptr<juce::TextButton>> effects;
    juce::ComboBox pw;
    std::array<std::array<juce::TextButton,3>,3> pwmSelectors;
    std::array<int,3> pwmSource{{0,0,0}};
    std::array<juce::TextButton,2> studioLfos;
    std::array<juce::TextButton,10> extraButtons;
    juce::TextButton fmButton{"FM"};
    juce::ComboBox effectSelect;
    LocalExtra localExtra;
    juce::TextButton previousPreset{"< Previous"},nextPreset{"Next >"};
    std::array<juce::Slider,6> unusedEffects;
    std::array<juce::Label,6> unusedEffectLabels;
    juce::String extraDomain;
    std::function<void()> resizeEditor;
    std::vector<std::unique_ptr<Control>> mainControls,effectControls;
    std::vector<std::unique_ptr<Panel>> panels;
    std::unique_ptr<juce::FileChooser> chooser;
    int page=0,osc=1,lfo=1,effect=0;
    bool displayHeld=false;
    int firmwareLoadPage=0;
    double displayUntil=0;
    explicit Surface(Processor& p):processor(p),skin(skinData(p)),browser(p)
    {
        setLookAndFeel(&look);
        performancePanel=std::make_unique<PerformancePanel>(processor,browser);addAndMakeVisible(*performancePanel);
        performancePanel->onEdit=[this](unsigned part){processor.selectPart(part);refreshModel();showPage(0);};
        performancePanel->onModeChanged=[this]{performanceSwitch.setToggleState(processor.performanceMode(),juce::dontSendNotification);for(unsigned i=0;i<4;++i){const bool available=i==0||(!processor.isKStation()&&processor.performanceMode());parts[i].setEnabled(available);parts[i].setAlpha(available?1.f:.3f);}};
        main.setComponentID("mainPanel");
        for(juce::Component* c:std::array<juce::Component*,12>{&main,&viewport,&lcd,&status,&latency,&loadRom,&folder,&loadProgram,&panic,&mode,&group,&section})addAndMakeVisible(c);
        addAndMakeVisible(channel);addAndMakeVisible(help);
        viewport.setViewedComponent(&content,false);viewport.setScrollBarsShown(true,false);viewport.setComponentID("controlViewport");
        for(auto* c:std::initializer_list<juce::Component*>{&firmwareSelect,&skinSelect,&bufferSelect,&rescanRoms,&waveRom,&factory,&customSkin,&performanceMode,&partActive,&browser})addAndMakeVisible(*c);
        firmwareSelect.setBounds(289,28,270,34);firmwareSelect.setComponentID("firmwareSelect");refreshFirmwares();
        firmwareSelect.onChange=[this]{int n=firmwareSelect.getSelectedId()-1;if(n<0 || n>=int(processor.firmwares.size()))return;startFirmwareLoad(processor.firmwares[n].file,0);};
        for(int i=0;i<4;++i){auto& b=parts[i];addAndMakeVisible(b);b.setButtonText(juce::String(i+1));b.getProperties().set("partTab",true);b.setComponentID("partTab"+juce::String(i+1));b.setBounds(579+i*58,28,50,34);b.onClick=[this,i]{performancePanel->closeNoteEditor();processor.selectPart(unsigned(i));refreshModel();};}
        performanceMode.setBounds(52,96,250,34);performanceMode.setComponentID("performanceMode");performanceMode.onClick=[this]{juce::String error;if(!processor.setPerformanceMode(performanceMode.getToggleState(),error))status.setText(error,juce::dontSendNotification);};
        addAndMakeVisible(performanceSwitch);performanceSwitch.setComponentID("performanceSwitch");performanceSwitch.getProperties().set("led",true);performanceSwitch.getProperties().set("dark",true);performanceSwitch.setClickingTogglesState(true);
        performanceSwitch.setTooltip("Enable or disable KS four-part Performance mode");
        performanceSwitch.onClick=[this]{juce::String error;if(!processor.setPerformanceMode(performanceSwitch.getToggleState(),error)){performanceSwitch.setToggleState(processor.performanceMode(),juce::dontSendNotification);showValue(error,false);}};
        partActive.setBounds(330,96,200,34);partActive.setComponentID("partActive");partActive.onClick=[this]{juce::String error;processor.setPartActive(processor.currentPart(),partActive.getToggleState(),error);};
        browser.setComponentID("patchBrowser");browser.setBounds(24,86,1232,642);
        skinSelect.setBounds(52,353,230,34);for(const auto& name:juce::StringArray{"Auto","Silver-K","KS Grey","Classic","Custom"})skinSelect.addItem(name,skinSelect.getNumItems()+1);skinSelect.setText(processor.skinChoice,juce::dontSendNotification);skinSelect.onChange=[this]{processor.skinChoice=skinSelect.getText();processor.savePreferences();skin=skinData(processor);if(resizeEditor)resizeEditor();rebuildMain();rebuildAdvanced();repaint();};
        customSkin.setBounds(298,353,190,34);customSkin.onClick=[this]{chooseAsset(3);};
        rescanRoms.setBounds(450,245,150,38);rescanRoms.onClick=[this]{juce::String error;const bool ok=processor.setRomFolder(processor.dataFolder.getChildFile("ROMs"),error);refreshFirmwares();status.setText(ok?"ROM list updated":error,juce::dontSendNotification);};rescanRoms.setComponentID("rescanRoms");
        bufferSelect.setBounds(52,401,436,34);bufferSelect.setComponentID("audioBuffer");for(const auto& name:juce::StringArray{"Audio buffer: Low latency","Audio buffer: Balanced (recommended)","Audio buffer: Safe","Audio buffer: Maximum reserve"})bufferSelect.addItem(name,bufferSelect.getNumItems()+1);bufferSelect.setSelectedId(processor.bufferProfile()+1,juce::dontSendNotification);bufferSelect.onChange=[this]{processor.setBufferProfile(bufferSelect.getSelectedId()-1);};bufferSelect.setTooltip("Internal audio buffering. The audio interface buffer is set in your DAW.");
        addAndMakeVisible(diagnostics);diagnostics.setBounds(510,401,270,34);diagnostics.setComponentID("performanceDiagnostics");
        diagnostics.setTooltip("Record optional performance measurements and save a JSON report for Windows or macOS.");
        addAndMakeVisible(controllerSelect);controllerSelect.setBounds(52,449,436,34);controllerSelect.setComponentID("ksController");
        if(ks::boardComparisonAvailable()){
            controllerSelect.addItem("KS controller: H8 (original)",1);controllerSelect.addItem("KS controller: C++ (experimental)",2);
            controllerSelect.setTooltip("Compare KS playback with H8 or C++ (OS 2.1.02). Keeps the current sound; stops an active diagnostic measurement. K-Station is unchanged.");
        }else{
            controllerSelect.addItem("KS controller: C++",2);
            controllerSelect.setTooltip("KS uses the C++ controller. K-Station is unchanged.");
        }
        refreshControllerSelection();
        controllerSelect.onChange=[this]{
            juce::String error;if(!processor.setKSControllerMode(controllerSelect.getSelectedId()-1,error))
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"KS controller",error);
            refreshControllerSelection();
        };
        diagnostics.onClick=[this]{
            juce::PopupMenu menu;const bool ready=processor.firmwareReady()&&!processor.isFirmwareLoading();
            menu.addItem(1,"Start new summary measurement",ready);menu.addItem(2,"Start new detailed measurement",ready);
            menu.addItem(3,"Stop measurement",processor.performanceDiagnosticsMode()!=0);menu.addSeparator();menu.addItem(4,"Save performance report...");
            const juce::Component::SafePointer<Surface> self(this);
            menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&diagnostics),[self](int result){
                if(!self)return;
                if(result>=1&&result<=3){self->processor.setPerformanceDiagnostics(result==3?0:result);return;}
                if(result!=4)return;
                self->chooser=std::make_unique<juce::FileChooser>("Save performance report",self->processor.dataFolder.getChildFile("silver-k-performance.json"),"*.json");
                self->chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[self](const juce::FileChooser& c){
                    if(!self || c.getResult()==juce::File{})return;
                    const bool ok=c.getResult().replaceWithText(self->processor.performanceReport());
                    self->status.setText(ok?"Performance report saved":"Cannot save performance report",juce::dontSendNotification);
                });
            });
        };
        waveRom.setBounds(52,295,180,38);waveRom.onClick=[this]{chooseAsset(1);};factory.setBounds(246,295,190,38);factory.onClick=[this]{chooseAsset(2);};
        auto pageRect=juce::Rectangle<int>(850,28,74,34);
        for(int i=0;i<5;++i)
        {
            auto& b=pages[size_t(i)];addAndMakeVisible(b);b.setButtonText(juce::StringArray{"Main","Extra","Perf","Patches","Setup"}[i]);
            b.setComponentID("page"+b.getButtonText());b.setBounds(pageRect.translated(i*(pageRect.getWidth()+6),0));
            b.onClick=[this,i]{showPage(i);};
        }
        auto selections=skin["selectors"];
        auto oscillatorRect=rect(selections["osc"]),lfoRect=rect(selections["lfo"]);
        for(int i=0;i<3;++i)
        {
            auto& b=oscillators[size_t(i)];main.addAndMakeVisible(b);b.setButtonText("OSC "+juce::String(i+1));b.setComponentID("oscSelect"+juce::String(i+1));
            b.getProperties().set("led",true);b.setBounds(oscillatorRect.getX()+i*oscillatorRect.getWidth()/3,oscillatorRect.getY(),oscillatorRect.getWidth()/3,oscillatorRect.getHeight());
            b.onClick=[this,i]{osc=i+1;rebuildMain();};
        }
        main.addAndMakeVisible(lfoSelect);lfoSelect.setComponentID("lfoSelect");lfoSelect.setBounds(lfoRect);
        lfoSelect.onClick=[this]{lfo=lfo==1?2:1;rebuildMain();};
        for(int i=0;i<2;++i)
        {
            auto& indicator=lfoIndicators[size_t(i)];main.addAndMakeVisible(indicator);
            indicator.setComponentID("lfoIndicator"+juce::String(i+1));indicator.setText(juce::String(i+1),juce::dontSendNotification);
            indicator.setColour(juce::Label::textColourId,ink);indicator.setFont(juce::Font(12.f));indicator.setJustificationType(juce::Justification::centredRight);
            indicator.setInterceptsMouseClicks(false,false);indicator.setBounds(rect(selections["lfoIndicators"][i]));
        }
        main.addAndMakeVisible(pw);pw.setBounds(rect(selections["pw"]));pw.setComponentID("pwSource");
        pw.addItem("PW position",1);pw.addItem("LFO 2 PWM",2);pw.addItem("Env PWM",3);pw.setSelectedId(1,juce::dontSendNotification);
        pw.setTooltip("Choose which existing pulse-width control to edit. All three retain their values.");pw.onChange=[this]{rebuildMain();};
        const auto fxRect=rect(selections["fx"]);const auto* fxArray=skin["effects"].getArray();
        for(int i=0;i<fxArray->size();++i)
        {
            auto b=std::make_unique<juce::TextButton>((*fxArray)[i]["name"].toString());main.addAndMakeVisible(*b);
            b->getProperties().set("small",true);b->setComponentID("effect"+(*fxArray)[i]["name"].toString());
            b->setBounds(fxRect.getX()+i*fxRect.getWidth()/fxArray->size(),fxRect.getY(),fxRect.getWidth()/fxArray->size()-2,fxRect.getHeight());
            b->setTooltip("Edit "+b->getButtonText()+". Other effects keep processing.");b->onClick=[this,i]{effect=i;rebuildEffects();};effects.push_back(std::move(b));
        }
        lcd.setColour(juce::Label::backgroundColourId,juce::Colour(0xff0752ba));lcd.setColour(juce::Label::textColourId,light);
        lcd.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(),16.f,juce::Font::plain));lcd.setJustificationType(juce::Justification::centredLeft);
        lcd.setComponentID("programDisplay");showProgram();lcd.setBorderSize(juce::BorderSize<int>(8));lcd.setBounds(rect(skin["lcd"]));
        loadProgram.setBounds(rect(skin["actions"]["loadProgram"]));panic.setBounds(rect(skin["actions"]["panic"]));
        loadRom.onClick=[this]{selectFile(true);};folder.onClick=[this]{chooseAsset(0);};
        loadProgram.setButtonText("Patch browser");loadProgram.onClick=[this]{showPage(3);};panic.onClick=[this]{for(uint8_t c=0;c<16;++c){processor.sendShortMidi(0xb0|c,64,0);processor.sendShortMidi(0xb0|c,123,0);processor.sendShortMidi(0xb0|c,120,0);}showValue("Panic\nAll notes off",false);};
        mode.setComponentID("controlMode");group.setComponentID("controlGroup");section.setComponentID("controlSection");
        mode.addItem("All sections",1);mode.addItem("Single section",2);mode.setSelectedId(1,juce::dontSendNotification);
        juce::StringArray groups;for(const auto& s:processor.visibleControls())groups.addIfNotAlreadyThere(s.group);
        for(int i=0;i<groups.size();++i)group.addItem(groups[i],i+1);group.setSelectedId(2,juce::dontSendNotification);
        mode.onChange=[this]{rebuildSections();};group.onChange=[this]{rebuildSections();};section.onChange=[this]{rebuildAdvanced();};
        auto advanced=skin["advanced"];mode.setBounds(rect(advanced["mode"]));group.setBounds(rect(advanced["group"]));section.setBounds(rect(advanced["section"]));viewport.setBounds(rect(advanced["viewport"]));
        auto setup=skin["setup"];status.setBounds(rect(setup["status"]));status.setFont(juce::Font(17.f));status.setColour(juce::Label::textColourId,ink);
        status.setText(processor.status(),juce::dontSendNotification);latency.setBounds(rect(setup["latency"]));latency.setColour(juce::Label::textColourId,ink);
        loadRom.setBounds(rect(setup["firmware"]));folder.setBounds(rect(setup["folder"]));channel.setBounds(510,353,270,34);channel.setComponentID("controlChannel");
        for(int i=1;i<=16;++i)channel.addItem("Editor MIDI channel "+juce::String(i),i);channel.setSelectedId(int(processor.editorChannel())+1,juce::dontSendNotification);channel.onChange=[this]{processor.setEditorChannel(uint8_t(channel.getSelectedId()-1));};
        help.setBounds(52,494,1120,84);help.setColour(juce::Label::textColourId,ink);help.setFont(juce::Font(15.f));
        help.setText("Silver-K folder contains ROMs and Patches. KS synthetic data is created in ROMs.\nWithout a wave ROM, sampled oscillators are silent. The zero bank adapter is disabled for a real ROM.\nPatch folders, model, tags, favourites and compatibility are available on Patches.",juce::dontSendNotification);
        const juce::StringArray domains{"osc1","osc2","osc3","mixer","filter","envelopes","lfo","voice","arp","fx"};
        for(size_t i=0;i<extraButtons.size();++i)
        {
            auto& b=extraButtons[i];main.addAndMakeVisible(b);b.setButtonText("Extra");
            b.setComponentID("extra:"+domains[int(i)]);b.getProperties().set("small",true);
            b.onClick=[this,domain=domains[int(i)]]{openExtra(domain);};
        }
        main.addAndMakeVisible(fmButton);fmButton.setComponentID("fmSettings");fmButton.getProperties().set("small",true);
        fmButton.onClick=[this]{openExtra("fm");};
        for(int o=0;o<3;++o)for(int n=0;n<3;++n)
        {
            auto& b=pwmSelectors[size_t(o)][size_t(n)];main.addAndMakeVisible(b);
            b.setButtonText(juce::StringArray{"PW Position","LFO 2 PWM","Env PWM"}[n]);
            b.setComponentID("pwmSource:"+juce::String(o+1)+":"+juce::String(n));
            b.getProperties().set("led",true);b.getProperties().set("small",true);
            b.onClick=[this,o,n]{pwmSource[size_t(o)]=n;rebuildMain();};
        }
        for(int n=0;n<2;++n)
        {
            auto& b=studioLfos[size_t(n)];main.addAndMakeVisible(b);b.setButtonText(juce::String(n+1));
            b.setComponentID("lfoSelect"+juce::String(n+1));b.getProperties().set("led",true);
            b.onClick=[this,n]{lfo=n+1;rebuildMain();};
        }
        main.addAndMakeVisible(effectSelect);effectSelect.setComponentID("effectSelect");
        if(auto* array=skin["effects"].getArray())for(auto config:*array)effectSelect.addItem(config["name"].toString(),effectSelect.getNumItems()+1);
        effectSelect.setSelectedId(effect+1,juce::dontSendNotification);
        effectSelect.onChange=[this]{effect=juce::jmax(0,effectSelect.getSelectedId()-1);rebuildEffects();};
        main.addChildComponent(localExtra);localExtra.onClose=[this]{updateExtraVisibility();repaint();};
        addAndMakeVisible(previousPreset);addAndMakeVisible(nextPreset);
        previousPreset.setComponentID("previousPreset");nextPreset.setComponentID("nextPreset");
        previousPreset.getProperties().set("small",true);nextPreset.getProperties().set("small",true);
        previousPreset.onClick=[this]{stepPreset(-1);};nextPreset.onClick=[this]{stepPreset(1);};
        for(size_t n=0;n<unusedEffects.size();++n){auto& knob=unusedEffects[n];main.addChildComponent(knob);knob.setComponentID("unusedEffect"+juce::String(int(n)));knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);knob.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);knob.setRotaryParameters(juce::MathConstants<float>::pi*4.f/3.f,juce::MathConstants<float>::pi*8.f/3.f,true);knob.getProperties().set("rotaryDiameter",80);knob.setEnabled(false);knob.setAlpha(.22f);auto& label=unusedEffectLabels[n];main.addChildComponent(label);label.setText("-",juce::dontSendNotification);label.setJustificationType(juce::Justification::centred);label.setAlpha(.25f);}
        processor.refreshControls();main.setBounds(0,0,int(skin["width"]),int(skin["height"]));rebuildMain();rebuildSections();showPage(processor.firmwareReady()?0:4);timerCallback();startTimerHz(20);
    }
    ~Surface() override{stopTimer();performancePanel.reset();viewport.setViewedComponent(nullptr,false);setLookAndFeel(nullptr);}
    void showProgram()
    {
        displayHeld=false;displayUntil=0;
        auto name=processor.currentName();
        if(name.isEmpty()||name=="default")name="Init Program";
        lcd.setText("Preset\n"+name,juce::dontSendNotification);
    }
    void showValue(const juce::String& message,bool held)
    {
        if(message.isEmpty()){showProgram();return;}
        displayHeld=held;displayUntil=juce::Time::getMillisecondCounterHiRes()+900;
        lcd.setText(message,juce::dontSendNotification);
    }
    void addControl(std::vector<std::unique_ptr<Control>>& list,juce::Component& parent,juce::String id,juce::String caption,juce::String kind,bool isDark,juce::Rectangle<int> bounds)
    {
        const auto* s=processor.spec(id);
        if(!s)for(const auto& candidate:processor.visibleControls())if(semanticId(candidate.id)==semanticId(id)){s=processor.spec(candidate.id);break;}
        if(!s)return;
        if(auto* parameter=processor.uiParameter(s->id))
        {
            auto c=std::make_unique<Control>(*parameter,*s,caption.isEmpty()?s->label:caption,kind,isDark && !greyLower(),[this](const juce::String& message,bool held){showValue(message,held);});
            // Text boxes are created before attachment; rebuild them with the
            // skin's editable-label styling, including its transparent frame.
            c->setLookAndFeel(&look);
            c->getProperties().set("compact",compactSkin());c->menu.getProperties().set("compact",compactSkin());
            c->slider.getProperties().set("standKnob",compactSkin());
            c->slider.getProperties().set("rotaryStyle",(kind=="classic" || kind=="miniKnob")?"classic":"led");
            c->slider.getProperties().set("rotaryDiameter",skin[(kind=="classic" || kind=="miniKnob")?"classicDiameter":(id=="cutoff"?"cutoffDiameter":"knobDiameter")]);
            if(kind=="inline" || kind=="rhythm")c->getProperties().set("captionWidth",compactSkin()?(id.startsWith("lfo")?32:50):54);
            c->slider.getProperties().set("blueVolume",studioSkin() && semanticId(s->id)=="volume");
            parent.addAndMakeVisible(*c);c->setBounds(bounds);list.push_back(std::move(c));
        }
    }
    void rebuildMain()
    {
        main.setBounds(0,0,int(skin["width"]),int(skin["height"]));
        const bool studio=studioSkin();
        main.getProperties().set("compact",compactSkin());performanceSwitch.getProperties().set("compact",compactSkin());
        if(compactSkin()){
            firmwareSelect.setBounds(rect(skin["studioHeader"]["model"]));
            for(int i=0;i<4;++i)parts[size_t(i)].setBounds(rect(skin["studioHeader"]["parts"][i]));
            performanceSwitch.setBounds(rect(skin["studioHeader"]["performance"]));
        }
        performanceSwitch.setVisible(compactSkin());
        performanceSwitch.setEnabled(!processor.isKStation()&&processor.firmwareReady());performanceSwitch.setToggleState(processor.performanceMode(),juce::dontSendNotification);
        for(unsigned i=0;i<4;++i){const bool available=i==0||(!processor.isKStation()&&processor.performanceMode());parts[i].setEnabled(available);parts[i].setAlpha(available?1.f:.3f);parts[i].setToggleState(i==processor.currentPart(),juce::dontSendNotification);}
        for(auto& b:oscillators)b.setVisible(!studio);lfoSelect.setVisible(!studio);pw.setVisible(!studio);
        for(auto& b:lfoIndicators)b.setVisible(!studio);for(auto& b:effects)b->setVisible(!studio);
        for(auto& row:pwmSelectors)for(auto& b:row)b.setVisible(studio);
        for(auto& b:studioLfos)b.setVisible(studio && !processor.isKStation());for(auto& b:extraButtons)b.setVisible(studio);
        fmButton.setVisible(studio);effectSelect.setVisible(studio);if(!studio)localExtra.setVisible(false);
        int nav=0;for(size_t i=0;i<pages.size();++i)
        {
            const bool visible=!studio || i!=1;pages[i].setVisible(visible);
            if(visible){if(compactSkin())pages[i].setBounds(rect(skin["studioHeader"]["pages"][nav++]));else pages[i].setBounds(studio?juce::Rectangle<int>(1011+nav++*102,28,94,34):juce::Rectangle<int>(850+int(i)*80,28,74,34));}
        }
        browser.setBounds(24,86,int(skin["width"])-48,int(skin["height"])-110);
        performancePanel->setBounds(13,83,int(skin["width"])-26,int(skin["height"])-96);
        lcd.setBounds(rect(skin["lcd"]));loadProgram.setBounds(rect(skin["actions"]["loadProgram"]));panic.setBounds(rect(skin["actions"]["panic"]));
        previousPreset.setBounds(rect(skin["actions"]["previous"]));nextPreset.setBounds(rect(skin["actions"]["next"]));
        previousPreset.setVisible(studio&&page==0);nextPreset.setVisible(studio&&page==0);
        if(studio){rebuildStudioMain();return;}
        showProgram();
        mainControls.clear();
        const juce::String sources[]{"pulse_width","lfo2_pwm","env_pwm"};
        for(auto item:*skin["controls"].getArray())
        {
            auto id=item["id"].toString().replace("{osc}",juce::String(osc)).replace("{lfo}",juce::String(lfo));
            auto caption=item["label"].toString();
            if(id.contains("{pw}")){id=id.replace("{pw}",sources[juce::jmax(0,pw.getSelectedId()-1)]);caption=pw.getText();}
            addControl(mainControls,main,id,caption,item["kind"].toString(),bool(item["dark"]),rect(item["rect"]));
        }
        for(int i=0;i<3;++i)oscillators[size_t(i)].setToggleState(osc==i+1,juce::dontSendNotification);
        lfoSelect.setTooltip("Currently editing LFO "+juce::String(lfo)+". Click to select LFO "+juce::String(lfo==1?2:1)+".");
        for(int i=0;i<2;++i)
        {
            auto& indicator=lfoIndicators[size_t(i)];indicator.getProperties().set("selected",lfo==i+1);
            indicator.setTitle("LFO "+juce::String(i+1)+(lfo==i+1?" selected":" inactive"));indicator.repaint();
        }
        if(!processor.isKStation()){
          addControl(mainControls,main,"filter_type","Type","menu",false,{522,466,140,44});
          for(auto& c:mainControls)if(juce::String(c->spec.id)=="filter_slope")c->setBounds(680,466,145,44);
        }
        rebuildEffects();main.setEnabled(processor.firmwareReady());
    }
    bool studioSkin()const{return skin["layout"].toString()=="studio";}
    bool compactSkin()const{return studioSkin() && bool(skin["compact"]);}
    juce::Rectangle<int> studioRegion(const juce::String& name)const{return rect(skin["studioRegions"][juce::Identifier(name)]);}
    std::vector<Spec> extraSpecs(const juce::String& domain)const
    {
        std::set<juce::String> exposed;
        for(const auto& c:mainControls)exposed.insert(semanticId(c->spec.id));
        for(const auto& c:effectControls)exposed.insert(semanticId(c->spec.id));
        for(int o=1;o<=3;++o)for(const auto* suffix:{"pulse_width","lfo2_pwm","env_pwm"})exposed.insert("osc"+juce::String(o)+"_"+suffix);
        std::vector<Spec> result;
        const auto fxName=skin["effects"][effect]["name"].toString();
        for(const auto& s:processor.visibleControls())
        {
            const juce::String id=s.id,g=s.group,sectionName=s.section;
            bool include=false;
            if(domain.startsWith("osc"))
                include=sectionName=="Oscillator "+domain.substring(3)
                    || id=="osc_key_phase" || (g=="Modulation" && id.contains("_osc_"));
            else if(domain=="mixer")include=g=="Mixer" || sectionName=="External input";
            else if(domain=="filter")include=g=="Filter" || (g=="Modulation" && id.contains("_filter_"));
            else if(domain=="envelopes")include=(g=="Envelopes" && sectionName!="FM") || (g=="Modulation" && id.endsWith("_amplitude"));
            else if(domain=="lfo")include=processor.isKStation()?(id.startsWith("lfo1_") || id.startsWith("lfo2_")):id.startsWith("lfo"+juce::String(lfo)+"_");
            else if(domain=="voice")include=g=="Voice" && sectionName!="Glide";
            else if(domain=="arp")include=g=="Arpeggiator" || (g=="Voice" && sectionName=="Glide");
            else if(domain=="fm")include=id.startsWith("fm_");
            else if(domain=="fx")include=g=="Effects" && (sectionName==fxName || (fxName=="Chorus" && sectionName=="Chorus / Phaser"));
            if(include && !exposed.count(semanticId(id)))result.push_back(s);
        }
        return result;
    }
    void stepPreset(int direction)
    {
        if(processor.isFirmwareLoading())return;
        juce::String error;
        if(browser.stepPatch(direction,&error)){processor.refreshControls();showProgram();}
        else showValue(error,false);
    }
    void updateExtraVisibility()
    {
        const bool open=localExtra.isVisible(),isMain=page==0;
        const auto bounds=localExtra.getBounds();
        for(auto& c:mainControls)c->setVisible(!open || !bounds.intersects(c->getBounds()));
        for(auto& c:effectControls)c->setVisible(!open || !bounds.intersects(c->getBounds()));
        for(auto& row:pwmSelectors)for(auto& b:row)b.setVisible(studioSkin() && (!open || !bounds.intersects(b.getBounds())));
        for(auto& b:studioLfos)b.setVisible(studioSkin()&&!processor.isKStation() && (!open || !bounds.intersects(b.getBounds())));
        const juce::StringArray domains{"osc1","osc2","osc3","mixer","filter","envelopes","lfo","voice","arp","fx"};
        for(size_t n=0;n<extraButtons.size();++n)extraButtons[n].setVisible(studioSkin() && !extraSpecs(domains[int(n)]).empty() && (!open || extraDomain!=domains[int(n)]));
        const bool programVisible=isMain && (!open || extraDomain!="voice");
        lcd.setVisible(programVisible);loadProgram.setVisible(programVisible);panic.setVisible(programVisible);
        previousPreset.setVisible(programVisible&&studioSkin());nextPreset.setVisible(programVisible&&studioSkin());
        repaint();
    }
    void openExtra(const juce::String& domain)
    {
        if(!studioSkin())return;
        extraDomain=domain;localExtra.controls.clear();
        const auto names=juce::StringArray{"osc1","osc2","osc3","mixer","filter","envelopes","lfo","voice","arp","fx","fm"};
        const auto regions=juce::StringArray{"Oscillator 1","Oscillator 2","Oscillator 3","Mixer","Filter","Envelopes","LFO","Program","Volume / Arp / Glide","Effects","FM"};
        const int n=names.indexOf(domain);if(n<0)return;
        localExtra.title=domain=="fm"?"FM settings":domain=="voice"?"Options":regions[n];
        auto bounds=studioRegion(regions[n]);
        if(domain.startsWith("osc") || domain=="fm"){
            const int row=domain=="fm"?2:domain.substring(3).getIntValue();
            if(row>1){bounds.setY(bounds.getY()+18);bounds.setHeight(bounds.getHeight()-18);}
            if(row<3)bounds.setHeight(bounds.getHeight()-(compactSkin()?30:39));
        }
        localExtra.dark=bounds.getY()>=int(skin["split"]) && !greyLower();
        localExtra.compact=compactSkin();
        localExtra.background=localExtra.dark?juce::Colour(0xff292c32):juce::Colour(0xffe0e5ea);
        localExtra.columns=(domain.startsWith("osc") || domain=="filter" || domain=="fm" || domain=="voice" || domain=="arp")?3:(domain=="lfo"&&processor.isKStation()?2:1);
        localExtra.rowHeight=(domain.startsWith("osc") || domain=="filter" || domain=="fm" || domain=="lfo")?43:55;
        localExtra.gap=7;
        if(compactSkin()&&(domain.startsWith("osc")||domain=="fm"||domain=="filter"||domain=="lfo")){localExtra.rowHeight=39;localExtra.gap=4;}
        if(compactSkin()&&domain=="mixer")localExtra.rowHeight=43;
        for(const auto& spec:extraSpecs(domain))
        {
            juce::String caption=spec.label;
            if(juce::String(spec.group)=="Modulation" || domain=="envelopes")caption=juce::String(spec.section)+" / "+spec.label;
            if(compactSkin() && domain.startsWith("osc"))caption=caption.replace("Oscillator pitch","Pitch").replace("Oscillator LFO1","LFO 1");
            if(domain=="lfo" && processor.isKStation())caption=juce::String(spec.id).substring(0,4).toUpperCase()+" / "+caption;
            const auto kind=(domain=="voice" || domain=="arp") && !spec.enumCount?"miniKnob":"extraRow";
            addControl(localExtra.controls,localExtra.content,spec.id,caption,kind,localExtra.dark,{});
        }
        localExtra.setBounds(bounds);localExtra.resized();localExtra.setVisible(!localExtra.controls.empty());localExtra.toFront(false);updateExtraVisibility();
    }
    void rebuildStudioMain()
    {
        showProgram();mainControls.clear();
        const juce::String sources[]{"pulse_width","lfo2_pwm","env_pwm"};
        for(auto item:*skin["studioModels"][processor.isKStation()?"K":"KS"].getArray())
        {
            auto id=item["id"].toString().replace("{lfo}",juce::String(lfo));
            if(id.contains("{pw}")){const int o=id.substring(3,4).getIntValue()-1;id=id.replace("{pw}",sources[pwmSource[size_t(juce::jlimit(0,2,o))]]);}
            auto kind=item["kind"].toString();if(kind=="menu")kind="panelMenu";
            addControl(mainControls,main,id,item["label"].toString(),kind,bool(item["dark"]),rect(item["rect"]));
        }
        for(int o=0;o<3;++o)for(int n=0;n<3;++n){auto& b=pwmSelectors[size_t(o)][size_t(n)];b.setBounds(rect(skin["pwmSelectors"][o*3+n]));b.setToggleState(n==pwmSource[size_t(o)],juce::dontSendNotification);}
        for(int n=0;n<2;++n){auto& b=studioLfos[size_t(n)];b.setBounds(rect(skin["lfoSelectors"][n]));b.setToggleState(lfo==n+1,juce::dontSendNotification);b.setVisible(!processor.isKStation());}
        const juce::StringArray domains{"osc1","osc2","osc3","mixer","filter","envelopes","lfo","voice","arp","fx"};
        const juce::StringArray regions{"Oscillator 1","Oscillator 2","Oscillator 3","Mixer","Filter","Envelopes","LFO","Program","Volume / Arp / Glide","Effects"};
        rebuildEffects();
        for(size_t n=0;n<extraButtons.size();++n){const auto r=studioRegion(regions[int(n)]);auto& b=extraButtons[n];b.setBounds(r.getRight()-72,r.getY()+8,62,28);b.setButtonText(n==7?"Options":"Extra");b.setVisible(!extraSpecs(domains[int(n)]).empty());}
        fmButton.setBounds(rect(skin["fmButton"]));effectSelect.setBounds(compactSkin()?rect(skin["effectSelect"]):juce::Rectangle<int>(1199,887,218,28));
        if(localExtra.isVisible())openExtra(extraDomain);else updateExtraVisibility();
        main.setEnabled(processor.firmwareReady());
    }
    void rebuildEffects()
    {
        showProgram();effectControls.clear();auto config=skin["effects"][effect];int i=0;
        for(auto id:*config["controls"].getArray()){
            addControl(effectControls,main,id.toString(),config["labels"][i].toString(),studioSkin()?"panelKnob":"classic",true,rect(skin["effectSlots"][i]));++i;
        }
        const int count=config["menus"].getArray()->size();
        for(int n=0;n<count;++n){auto id=config["menus"][n].toString();auto area=rect(skin["effectMenuArea"]);const int width=count?(area.getWidth()-(count-1)*12)/count:0;
            const auto caption=id=="chorus_start"?"Sync start":id.endsWith("sync")?"Sync":id.endsWith("start")?"Start":id.endsWith("type")?"Type":id=="delay_ratio"?"Ratio":"Sibilance";
            const auto bounds=studioSkin()?juce::Rectangle<int>(area.getX()+n*(width+12),area.getY(),width,area.getHeight()):rect(skin["effectMenus"][n]);
            addControl(effectControls,main,id,caption,studioSkin()?"inline":"row",true,bounds);
            if(studioSkin() && !effectControls.empty()){auto& c=*effectControls.back();c.getProperties().set("captionWidth",compactSkin()?52:60);c.resized();}
        }
        if(!studioSkin())for(auto& c:effectControls)if(c->spec.enumCount){c->label.setVisible(false);c->menu.setBounds(c->getLocalBounds());}
        for(size_t n=0;n<effects.size();++n)effects[n]->setToggleState(int(n)==effect,juce::dontSendNotification);
        for(size_t n=0;n<unusedEffects.size();++n){const bool visible=studioSkin()&&int(n)>=i;auto r=rect(skin["effectSlots"][int(n)]);const int knobHeight=compactSkin()?76:84;unusedEffects[n].getProperties().set("standKnob",compactSkin());unusedEffects[n].getProperties().set("rotaryDiameter",int(skin["knobDiameter"]));unusedEffects[n].setBounds(r.withHeight(knobHeight));unusedEffectLabels[n].setBounds(r.withY(r.getY()+knobHeight).withHeight(36));unusedEffectLabels[n].setColour(juce::Label::textColourId,greyLower()?ink:light);unusedEffects[n].setVisible(visible);unusedEffectLabels[n].setVisible(visible);}
        if(studioSkin()){extraButtons.back().setVisible(!extraSpecs("fx").empty());if(localExtra.isVisible()&&extraDomain=="fx")openExtra("fx");updateExtraVisibility();}
    }
    void showPage(int selected)
    {
        showProgram();
        page=selected;
        for(size_t i=0;i<pages.size();++i)pages[i].setToggleState(int(i)==page,juce::dontSendNotification);
        const bool isMain=page==0,isAdvanced=page==1,isSetup=page==4;
        main.setVisible(isMain);lcd.setVisible(isMain);loadProgram.setVisible(isMain);panic.setVisible(isMain);previousPreset.setVisible(isMain&&studioSkin());nextPreset.setVisible(isMain&&studioSkin());
        viewport.setVisible(isAdvanced);mode.setVisible(isAdvanced);group.setVisible(page==1);section.setVisible(page==1&&mode.getSelectedId()==2);mode.setVisible(page==1);browser.setVisible(page==3);performanceMode.setVisible(false);partActive.setVisible(false);performancePanel->setVisible(page==2);if(page!=2)performancePanel->closeNoteEditor();
        viewport.setBounds(page==2?juce::Rectangle<int>(24,146,int(skin["width"])-48,int(skin["height"])-170):rect(skin["advanced"]["viewport"]));
        for(juce::Component* c:std::array<juce::Component*,6>{&status,&latency,&loadRom,&folder,&channel,&help})c->setVisible(isSetup);
        rescanRoms.setVisible(isSetup);bufferSelect.setVisible(isSetup);waveRom.setVisible(isSetup);factory.setVisible(isSetup);skinSelect.setVisible(isSetup);customSkin.setVisible(isSetup);
        diagnostics.setVisible(isSetup);controllerSelect.setVisible(isSetup);refreshControllerSelection();
        loadRom.setBounds(52,245,200,38);folder.setBounds(266,245,170,38);
        if(page==1)rebuildSections();if(page==2)performancePanel->refresh();
        if(studioSkin())updateExtraVisibility();repaint();
    }
    void rebuildSections()
    {
        const auto selected=section.getText();section.clear(juce::dontSendNotification);juce::StringArray names;
        for(const auto& s:processor.visibleControls())if(group.getText()==s.group)names.addIfNotAlreadyThere(s.section);
        for(int i=0;i<names.size();++i)section.addItem(names[i],i+1);
        section.setSelectedId(juce::jmax(0,names.indexOf(selected))+1,juce::dontSendNotification);
        section.setVisible(page==1&&mode.getSelectedId()==2);rebuildAdvanced();
    }
    void rebuildAdvanced()
    {
        panels.clear();
        const auto layout=skin["advanced"];const int rowHeight=int(layout["rowHeight"]),columnWidth=int(layout["columnWidth"]),gap=int(layout["gap"]);
        for(const auto& s:processor.visibleControls())
        {
            if(page==2){if(juce::String(s.group)!="Part" && juce::String(s.id)!="volume")continue;}else if(juce::String(s.group)=="Part" || group.getText()!=s.group||(mode.getSelectedId()==2&&section.getText()!=s.section))continue;
            Panel* panel=nullptr;for(auto& p:panels)if(p->title==s.section){panel=p.get();break;}
            if(!panel){auto p=std::make_unique<Panel>();p->title=s.section;p->rowHeight=rowHeight;panel=p.get();content.addAndMakeVisible(*p);panels.push_back(std::move(p));}
            addControl(panel->controls,*panel,s.id,s.label,"row",false,{});
        }
        std::array<int,3> ys{0,0,0};
        for(auto& p:panels){const auto col=size_t(std::min_element(ys.begin(),ys.end())-ys.begin());const int h=42+rowHeight*int(p->controls.size());p->setBounds(int(col)*(columnWidth+gap),ys[col],columnWidth,h);ys[col]+=h+gap;}
        content.setSize(viewport.getWidth()-16,juce::jmax(viewport.getHeight(),*std::max_element(ys.begin(),ys.end())));viewport.setViewPosition(0,0);content.setEnabled(processor.firmwareReady());
    }
    void paint(juce::Graphics& g) override
    {
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffeef0f2),0,0,processor.skinChoice=="Classic"?juce::Colour(0xffd9d4c6):silver,0,float(getHeight()),false));g.fillAll();
        const auto header=rect(skin["header"]).toFloat();g.setGradientFill(juce::ColourGradient(processor.skinChoice=="Classic"?juce::Colour(0xff438276):juce::Colour(0xff148de3),0,header.getY(),processor.skinChoice=="Classic"?juce::Colour(0xff1c5048):juce::Colour(0xff075293),0,header.getBottom(),false));g.fillRoundedRectangle(header,3);
        g.setColour(light);g.setFont(juce::Font(compactSkin()?32.f:36.f,juce::Font::bold));g.drawText(skin["name"].toString(),studioSkin()?rect(skin["title"]):juce::Rectangle<int>(40,21,250,45),juce::Justification::centredLeft);
        if(page==0)
        {
            if(studioSkin())
            {
                for(auto sectionData:*skin["sections"].getArray()){
                    const auto* a=sectionData.getArray();const auto r=juce::Rectangle<int>(int((*a)[0]),int((*a)[1]),int((*a)[2]),int((*a)[3]));const auto title=(*a)[4].toString();
                    const bool low=title=="Volume / Arp / Glide"||title=="Program"||title=="Effects",black=low&&!greyLower();
                    g.setGradientFill(juce::ColourGradient(black?juce::Colour(0xff292c32):juce::Colour(0xffeef1f5),r.getX(),r.getY(),black?juce::Colour(0xff292c32):juce::Colour(0xffe4eaf0),r.getRight(),r.getBottom(),false));g.fillRoundedRectangle(r.toFloat(),3);
                    g.setColour((black?light:ink).withAlpha(.23f));g.drawRoundedRectangle(r.toFloat().reduced(.5f),3,1);
                    g.setFont(juce::Font(16.f,juce::Font::bold));g.setColour(black?light:ink);
                    if(title=="Oscillators"){
                        for(int o=0;o<3;++o){if(!(localExtra.isVisible()&&(extraDomain=="osc"+juce::String(o+1)||(extraDomain=="fm"&&o==1))))g.drawText("Oscillator "+juce::String(o+1),r.getX()+10,r.getY()+9+(compactSkin()?224:256)*o,200,22,juce::Justification::centredLeft);if(o){
                            const float y=float(r.getY()+1+(compactSkin()?224:256)*o-(compactSkin()?12:21));
                            auto gap=fmButton.getBounds().expanded(6,0);
                            if(o==1)for(const auto& c:mainControls)if(semanticId(c->spec.id)=="osc_sync"){gap=c->getBounds().expanded(6,0);break;}
                            g.setColour(ink.withAlpha(.2f));g.drawLine(float(r.getX()+8),y,float(gap.getX()),y);g.drawLine(float(gap.getRight()),y,float(r.getRight()-8),y);g.setColour(ink);
                        }}
                    }else if(title=="Envelopes"){
                        if(!(localExtra.isVisible()&&extraDomain=="envelopes")){g.drawText("Mod Env",r.getX()+10,r.getY()+8,r.getWidth()-90,22,juce::Justification::centredLeft);g.drawText("Amp Env",r.getX()+10,r.getY()+r.getHeight()/2+8,r.getWidth()-20,22,juce::Justification::centredLeft);g.setColour(ink.withAlpha(.2f));g.drawLine(float(r.getX()+8),float(r.getCentreY()),float(r.getRight()-8),float(r.getCentreY()));}
                    }else g.drawText(title,r.getX()+10,r.getY()+8,r.getWidth()-85,22,juce::Justification::centredLeft);
                }
                if(processor.isKStation()){g.setColour(ink);g.setFont(juce::Font(12.f));const auto f=studioRegion("Filter");g.drawText("Low-pass",f.getX()+12,f.getY()+44,160,22,juce::Justification::centredLeft);}
                if(processor.isKStation()&&!compactSkin())for(int n=0;n<2;++n){g.setColour(ink);g.setFont(juce::Font(12.f,juce::Font::bold));g.drawText("LFO "+juce::String(n+1),1141+n*140,541,126,14,juce::Justification::centred);}
            }
            else
            {
                const int split=int(skin["split"]);g.setGradientFill(juce::ColourGradient(greyLower()?juce::Colour(0xffc4c7cc):juce::Colour(0xff303237),0,float(split),greyLower()?juce::Colour(0xffaeb2b9):juce::Colour(0xff1d2025),0,float(getHeight()),false));g.fillRect(18,split,getWidth()-36,getHeight()-split-16);
                for(auto sectionData:*skin["sections"].getArray()){const auto* a=sectionData.getArray();const int x=int((*a)[0]),y=int((*a)[1]),w=int((*a)[2]),h=int((*a)[3]);const bool low=y>=split;g.setColour(low&&!greyLower()?light:ink);g.setFont(juce::Font(16.f,juce::Font::bold));g.drawText((*a)[4].toString(),x+5,y+4,w-10,24,juce::Justification::centredLeft);g.setColour((low&&!greyLower()?light:ink).withAlpha(.35f));g.drawLine(float(x+w+6),float(y),float(x+w+6),float(y+h),1);}
            }
            g.setColour(juce::Colour(0xff0b1320));g.fillRoundedRectangle(rect(skin["lcd"]).toFloat().expanded(5),3);
        }
        else if(page==4)
        {
            g.setColour(ink);g.setFont(juce::Font(22.f,juce::Font::bold));g.drawText("Instrument setup",52,94,900,34,juce::Justification::centredLeft);
        }
        g.setColour(ink.withAlpha(.4f));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),5,1);
    }
    void timerCallback() override
    {
        refreshControllerSelection();
        diagnostics.setButtonText(processor.performanceDiagnosticsMode()?"Diagnostics: recording":"Performance diagnostics");
        if(processor.isFirmwareLoading()){
            juce::String error;
            if(!processor.finishFirmwareLoad(error)){setEnabled(false);lcd.setText("Loading firmware...",juce::dontSendNotification);status.setText("Loading firmware...",juce::dontSendNotification);return;}
        }
        if(lastFirmwareRevision!=processor.firmwareLoadRevision()){
            lastFirmwareRevision=processor.firmwareLoadRevision();setEnabled(true);refreshModel();showPage(processor.firmwareReady()?firmwareLoadPage:4);
        }
        if(!displayHeld&&juce::Time::getMillisecondCounterHiRes()>=displayUntil)showProgram();
        processor.refreshControls();
        performancePanel->refresh();
        if(displayedK!=processor.isKStation() || displayedPart!=processor.currentPart() || displayedPerformance!=processor.performanceMode())refreshModel();
        performanceMode.setEnabled(!processor.isKStation() && processor.firmwareReady());performanceMode.setToggleState(processor.performanceMode(),juce::dontSendNotification);
        performanceSwitch.setEnabled(!processor.isKStation() && processor.firmwareReady());performanceSwitch.setToggleState(processor.performanceMode(),juce::dontSendNotification);
        partActive.setEnabled(!processor.isKStation() && processor.performanceMode());partActive.setToggleState((processor.activeParts()&(1u<<processor.currentPart()))!=0,juce::dontSendNotification);
        for(unsigned i=0;i<4;++i){const bool available=i==0 || (!processor.isKStation() && processor.performanceMode());parts[i].setEnabled(available);parts[i].setAlpha(available?1.f:.3f);parts[i].setToggleState(i==processor.currentPart(),juce::dontSendNotification);}
        for(auto& c:mainControls)c->refresh();for(auto& c:effectControls)c->refresh();for(auto& p:panels)for(auto& c:p->controls)c->refresh();
        for(auto& c:localExtra.controls)c->refresh();
        const bool ready=processor.firmwareReady();main.setEnabled(ready);content.setEnabled(ready);loadProgram.setEnabled(true);
        previousPreset.setEnabled(ready);nextPreset.setEnabled(ready);channel.setSelectedId(int(processor.editorChannel())+1,juce::dontSendNotification);
        help.setText(processor.dataFolder.getFullPathName()+"\nROMs: firmware, KS Data and Wave ROM. Patches: automatically scanned patch banks.\nThe audio interface buffer is configured in your DAW.",juce::dontSendNotification);bufferSelect.setSelectedId(processor.bufferProfile()+1,juce::dontSendNotification);const auto rate=processor.getSampleRate();latency.setText(rate>0?"Plugin latency: "+juce::String(1000.*processor.getLatencySamples()/rate,1)+" ms":"",juce::dontSendNotification);
    }
    uint64_t lastFirmwareRevision=0;
    void refreshControllerSelection(){
        const int mode=processor.ksControllerMode();
        // Reflect developer-only legacy environment/project selections without
        // offering an extra experimental mode in the normal H8/C++ selector.
        if(mode==2 && controllerSelect.getNumItems()==2){controllerSelect.addItem("KS controller: MIDI hybrid (legacy)",3);controllerSelect.setItemEnabled(3,false);}
        controllerSelect.setSelectedId(mode+1,juce::dontSendNotification);
        controllerSelect.setEnabled(ks::boardComparisonAvailable()&&!processor.isKStation()&&!processor.isFirmwareLoading());
    }
    bool greyLower()const{return studioSkin()?!processor.isKStation():processor.skinChoice=="KS Grey" || (processor.skinChoice=="Auto"&&!processor.isKStation());}
    void refreshFirmwares(){if(processor.isFirmwareLoading())return;firmwareSelect.clear(juce::dontSendNotification);for(size_t i=0;i<processor.firmwares.size();++i){const auto& f=processor.firmwares[i];firmwareSelect.addItem(juce::String(f.kStation?"K-Station: ":"KS: ")+f.file.getFileName(),int(i)+1);if(f.file==processor.osFile)firmwareSelect.setSelectedId(int(i)+1,juce::dontSendNotification);}if(!firmwareSelect.getSelectedId())firmwareSelect.setText(processor.osFile.existsAsFile()?juce::String(processor.isKStation()?"K-Station: ":"KS: ")+processor.osFile.getFileName():"Choose firmware",juce::dontSendNotification);}
    void startFirmwareLoad(const juce::File& file,int targetPage){
        if(!processor.beginFirmwareLoad(file))return;
        performancePanel->closeNoteEditor();firmwareLoadPage=targetPage;setEnabled(false);
        lcd.setText("Loading firmware...",juce::dontSendNotification);status.setText("Loading firmware...",juce::dontSendNotification);
    }
    void refreshModel(){displayedK=processor.isKStation();displayedPart=processor.currentPart();displayedPerformance=processor.performanceMode();group.clear(juce::dontSendNotification);juce::StringArray names;for(const auto& c:processor.visibleControls())if(juce::String(c.group)!="Part")names.addIfNotAlreadyThere(c.group);for(int i=0;i<names.size();++i)group.addItem(names[i],i+1);group.setSelectedId(2,juce::dontSendNotification);status.setText(processor.status(),juce::dontSendNotification);rebuildMain();rebuildSections();refreshFirmwares();repaint();}
    void selectFile(bool){chooseAsset(4);}
    void chooseAsset(int role){const bool directory=role==0;chooser=std::make_unique<juce::FileChooser>(directory?"Select Silver-K folder":role==1?"Select KS wave ROM":role==2?"Select KS factory data":role==3?"Select skin JSON":"Load firmware",directory?processor.dataFolder:processor.romFolder,directory?"*":role==3?"*.json":role==2?"*.bin;*.mid;*.midi":"*.bin;*.syx;*.zip;*.os;*.OS");const juce::Component::SafePointer<Surface> self(this);chooser->launchAsync(juce::FileBrowserComponent::openMode|(directory?juce::FileBrowserComponent::canSelectDirectories:juce::FileBrowserComponent::canSelectFiles),[self,role](const juce::FileChooser& c){if(!self || self->processor.isFirmwareLoading())return;auto file=c.getResult();if(file==juce::File{})return;juce::String error;bool ok=true;if(role==0)ok=self->processor.setDataFolder(file,error);else if(role==1)ok=self->processor.setWaveRom(file,error);else if(role==2)ok=self->processor.setFlashRom(file,error);else if(role==3){auto data=juce::JSON::parse(file);if(!validSkin(data)){ok=false;error="Expected a Silver-K compact (1280 x 1000), studio (1440 x 1122), or legacy (1280 x 752) JSON skin";}else{self->processor.customSkin=file.getFullPathName();self->processor.skinChoice="Custom";self->skin=skinData(self->processor);if(self->resizeEditor)self->resizeEditor();self->skinSelect.setText("Custom",juce::dontSendNotification);self->rebuildMain();self->rebuildAdvanced();self->repaint();}}else{self->startFirmwareLoad(file,0);return;}
        if(ok)self->processor.savePreferences();
        if(ok&&(role==1||role==2)&&self->processor.osFile.existsAsFile()&&!self->processor.isKStation()){self->startFirmwareLoad(self->processor.osFile,4);return;}
        self->status.setText(ok?self->processor.status():error,juce::dontSendNotification);self->refreshFirmwares();if(ok&&role==4){self->refreshModel();self->showPage(0);}else self->showPage(4);});}

};
Editor::Editor(Processor& p):AudioProcessorEditor(p),m_processor(p),m_surface(std::make_unique<Surface>(p))
{
    addAndMakeVisible(*m_surface);setResizable(true,true);
    addMouseListener(this,true);
    m_surface->resizeEditor=[this]
    {
        const int width=int(m_surface->skin["width"]),height=int(m_surface->skin["height"]);
        const double scale=m_processor.editorScale.load();
        setResizeLimits(width/2,height/2,width*3/2,height*3/2);
        if(auto* c=getConstrainer())c->setFixedAspectRatio(double(width)/height);
        setUiScale(scale);
    };
    m_surface->resizeEditor();
}
Editor::~Editor(){removeMouseListener(this);m_processor.savePreferences();}
double Editor::getUiScale() const{return double(getWidth())/int(m_surface->skin["width"]);}
void Editor::setUiScale(double scale)
{
    if(!std::isfinite(scale)||scale<.5||scale>1.5)return;
    const int width=int(m_surface->skin["width"]),height=int(m_surface->skin["height"]);
    setSize(juce::roundToInt(width*scale),juce::roundToInt(height*scale));
    resized();
}
void Editor::resized()
{
    if(!m_surface)return;const int width=int(m_surface->skin["width"]),height=int(m_surface->skin["height"]);
    m_surface->setBounds(0,0,width,height);m_surface->setTransform(juce::AffineTransform::scale(float(getWidth())/width,float(getHeight())/height));
    const double scale=double(getWidth())/width;
    if(scale>=.5&&scale<=1.5)m_processor.editorScale.store(scale);
}
void Editor::mouseDown(const juce::MouseEvent& event)
{
    if(event.mods.isPopupMenu()&&!m_sizeMenuOpen)showSizeMenu();
}
void Editor::showSizeMenu()
{
    m_sizeMenuOpen=true;
    juce::PopupMenu sizes,menu;
    for(int percent:{50,75,100})sizes.addItem(percent,juce::String(percent)+"%",true,std::abs(getUiScale()*100-percent)<.1);
    sizes.addSeparator();sizes.addItem(1,"Custom…");menu.addSubMenu("Size",sizes);
    const juce::Component::SafePointer<Editor> self(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea({juce::Desktop::getMousePosition(),{1,1}}),[self](int result){
        if(!self)return;
        self->m_sizeMenuOpen=false;
        if(result==1)self->showCustomSize();else if(result>=50)self->setUiScale(result/100.);
    });
}
void Editor::showCustomSize()
{
    if(m_sizeDialog)return;
    m_sizeDialog=std::make_unique<juce::AlertWindow>("Plugin size","Scale: 50–150%",juce::MessageBoxIconType::NoIcon,this);
    m_sizeDialog->addTextEditor("percent",juce::String(getUiScale()*100,1),"Size (%)");
    m_sizeDialog->addButton("Apply",1,juce::KeyPress(juce::KeyPress::returnKey));
    m_sizeDialog->addButton("Cancel",0,juce::KeyPress(juce::KeyPress::escapeKey));
    const juce::Component::SafePointer<Editor> self(this);
    m_sizeDialog->enterModalState(true,juce::ModalCallbackFunction::create([self](int result){
        if(!self)return;
        const auto text=self->m_sizeDialog->getTextEditorContents("percent").trim().replaceCharacter(',','.');
        const double scale=text.getDoubleValue()/100.;
        self->m_sizeDialog.reset();
        if(result==1){
            if(text.containsOnly("0123456789.")&&text.retainCharacters(".").length()<=1&&std::isfinite(scale)&&scale>=.5&&scale<=1.5)self->setUiScale(scale);
            else juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Plugin size","Enter a size between 50 and 150 percent.");
        }
    }));
}
}
