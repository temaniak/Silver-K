#include "silverSoulTrainJuce.h"
#include "silverSoulTrainResources.h"
#include "soultrain/DrumSequencer.h"
#include "soultrain/SkinArt.h"

#include "silverPatch.h"
#include "silverProcessor.h"

#if JUCE_WINDOWS
#include <juce_opengl/juce_opengl.h>
#include <atomic>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <list>
#include <map>
#include <optional>
#include <stdexcept>
#include <utility>

namespace silverK {
namespace {

constexpr int width=1486, height=942;
constexpr auto firmwareSwitchCaption="On firmware change";
constexpr auto drumVoiceEditCaption="Drum Edit";
constexpr auto effectsMasterCaption="All effects On/Off";
constexpr float setupSynthX=60.f,setupDataX=536.328125f,originalSetupDataX=1011.65625f;
using String=juce::String;
using Rect=juce::Rectangle<float>;
const auto exportSysExCaption=String::fromUTF8("Export .syx…");
const auto exportSilverKCaption=String::fromUTF8("Export .silver-k…");
const auto saveCurrentCaption=String::fromUTF8("Save current…");
constexpr juce::uint32 browserTextColour=0xff101011;

void styleTagInput(juce::TextEditor& input) {
    const juce::Colour ink(browserTextColour);
    input.setColour(juce::TextEditor::textColourId,ink);
    input.setColour(juce::TextEditor::highlightedTextColourId,ink);
    input.setColour(juce::TextEditor::highlightColourId,juce::Colour(0xffbacbdc));
    input.setColour(juce::CaretComponent::caretColourId,ink);
}

#if JUCE_WINDOWS
constexpr auto rendererCaption="Graphics renderer";

struct SkinPageState {
    String layout;
    std::vector<int> values;
    juce::StringArray labels;
    juce::var browser;

    bool operator==(const SkinPageState& other) const {
        return layout==other.layout && values==other.values && labels==other.labels &&
               browser.getDynamicObject()==other.browser.getDynamicObject();
    }
};

// The parent's transform scales these rasters without invalidating their
// pixels. Keep a larger raster ready for the next increase in window size.
class SkinFrame final : public juce::CachedComponentImage, private juce::Timer {
public:
    using ReadPage=std::function<std::optional<SkinPageState>()>;
    using ReadLiveAreas=std::function<juce::RectangleList<int>()>;
    using ReplaceLayout=std::function<std::optional<SkinPageState>(juce::RectangleList<int>&)>;

    SkinFrame(juce::Component& component,ReadPage readPage,ReadLiveAreas readLiveAreas)
        : owner(component),pageState(std::move(readPage)),liveAreas(std::move(readLiveAreas)) {}

    void pageChanged() {
        stopTimer();
        current={};
        larger={};
        largerReady=false;
        paintedState.reset();
        rememberUpdate=false;
    }

    bool canRetainFrame(const std::optional<SkinPageState>& state) const {
        return current.image.isValid() && state && paintedState==state;
    }

    bool hasRendered() const { return current.image.isValid(); }

    void replaceRegion(juce::RectangleList<int> areas,ReplaceLayout rebuild) {
        std::optional<SkinPageState> next;
        {
            // Adding and removing JUCE children requests repaints. The caller
            // supplies their actual changed regions after the layout is set.
            const juce::ScopedValueSetter<bool> rebuilding(ignoreLayoutRepaints,true);
            next=rebuild(areas);
        }
        if (!next) {
            pageChanged();
            owner.repaint();
            return;
        }

        if (restorePage(*next)) {
            rememberUpdate=false;
            for (const auto& area:current.dirty) areas.add(area);
            auto* parent=owner.getParentComponent();
            for (const auto& area:areas) {
                if (larger.image.isValid()) larger.dirty.add(area);
                parent->repaint(parent->getLocalArea(&owner,area));
            }
            return;
        }
        paintedState=std::move(next);
        rememberUpdate=true;
        for (const auto& area:areas) owner.repaint(area);
    }

    void paint(juce::Graphics& g) override {
        const auto sizes=pixelBounds(g);
        viewport=sizes.viewport;
        if (largerSize!=sizes.larger) {
            largerSize=sizes.larger;
            larger={};
            largerReady=false;
            stopTimer();
        }

        if (!current.image.isValid())
            openPage(viewport);

        auto& visible=largerReady && !covers(current.image,viewport)?larger:current;
        const bool fullRedraw=visible.dirty.containsRectangle(owner.getLocalBounds());
        const bool patched=std::exchange(rememberUpdate,false);
        const auto state=fullRedraw?pageState():patched?paintedState:std::nullopt;
        drawDirty(visible,visible.dirty);
        if (fullRedraw || patched) {
            if (state && state==pageState() && visible.dirty.isEmpty()) {
                paintedState=state;
                rememberPage(*state,visible.image);
            } else {
                paintedState.reset();
            }
        }
        g.setOpacity(owner.getAlpha());
        g.drawImage(visible.image,owner.getLocalBounds().toFloat());

        if (!covers(current.image,largerSize) && !largerReady && owner.isShowing() && !isTimerRunning())
            startTimer(40);
    }

    bool invalidateAll() override {
        if (ignoreLayoutRepaints) return false;
        current.dirty=owner.getLocalBounds();
        larger.dirty=owner.getLocalBounds();
        largerReady=false;
        paintedState.reset();
        return true;
    }
    bool invalidate(const juce::Rectangle<int>& area) override {
        if (ignoreLayoutRepaints) return false;
        current.dirty.add(area);
        if (larger.image.isValid()) larger.dirty.add(area);
        return true;
    }
    void releaseResources() override {
        pageChanged();
        remembered.clear();
        largerSize={};
    }

private:
    struct Raster {
        juce::Image image;
        juce::RectangleList<int> dirty;
    };

    struct SavedPage {
        SkinPageState state;
        juce::Image image;
        juce::RectangleList<int> refresh;
    };

    struct PixelBounds {
        juce::Rectangle<int> viewport,larger;
    };

    PixelBounds pixelBounds(juce::Graphics& g) const {
        const auto transform=owner.getParentComponent()->getTransform();
        const auto zoom=std::sqrt(transform.mat00*transform.mat11);
        const auto stretch=std::sqrt(transform.mat00/transform.mat11);
        const auto scale=g.getInternalContext().getPhysicalPixelScaleFactor();
        // Remove the editor zoom before rounding the monitor DPI. The larger
        // target must stay the same through the drag on the same display.
        const auto dpi=std::round(96.f*scale/zoom)/96.f;
        const auto pixels=[&](float x,float y) {
            return juce::Rectangle<int>(juce::roundToInt(owner.getWidth()*juce::jlimit(.5f,2.f,x)),
                                        juce::roundToInt(owner.getHeight()*juce::jlimit(.5f,2.f,y)));
        };
        const auto view=pixels(scale*stretch,scale/stretch);
        return {view,view.getUnion(pixels(dpi*1.5f,dpi*1.5f))};
    }

    static bool covers(const juce::Image& image,juce::Rectangle<int> area) {
        return image.isValid() && image.getBounds().contains(area);
    }

    Raster createRaster(juce::Rectangle<int> size) const {
        Raster raster;
        raster.image=juce::Image(juce::Image::RGB,size.getWidth(),size.getHeight(),false);
        raster.dirty=owner.getLocalBounds();
        return raster;
    }

    bool restorePage(const SkinPageState& state) {
        const auto saved=std::find_if(remembered.begin(),remembered.end(),[&](const SavedPage& page) {
            return page.state==state && covers(page.image,viewport);
        });
        if (saved==remembered.end()) return false;
        current={saved->image,saved->refresh};
        paintedState=state;
        for (const auto& area:liveAreas()) current.dirty.add(area);
        remembered.splice(remembered.begin(),remembered,saved);
        return true;
    }

    void openPage(juce::Rectangle<int> size) {
        const auto state=pageState();
        if (state && restorePage(*state)) return;
        current=createRaster(size);
        drawDirty(current,current.dirty);
        if (state && state==pageState() && current.dirty.isEmpty()) {
            paintedState=state;
            rememberPage(*state,current.image);
        }
    }

    void rememberPage(const SkinPageState& state,const juce::Image& image) {
        remembered.remove_if([&](const SavedPage& page){return page.state.layout==state.layout;});
        remembered.push_front({state,image,liveAreas()});
        constexpr int pixelBudget=8000000;
        int pixels=0;
        for (auto page=remembered.begin();page!=remembered.end();++page) {
            pixels+=page->image.getWidth()*page->image.getHeight();
            if (pixels>pixelBudget) {
                remembered.erase(page,remembered.end());
                break;
            }
        }
    }

    void drawDirty(Raster& raster,juce::RectangleList<int> areas) {
        if (areas.isEmpty()) return;
        raster.dirty.subtract(areas);
        raster.image.duplicateIfShared();
        const auto scale=juce::AffineTransform::scale(float(raster.image.getWidth())/owner.getWidth(),
                                                      float(raster.image.getHeight())/owner.getHeight());
        juce::Graphics g(raster.image);
        // Clip in physical pixels so adjacent translucent draws share no
        // fractional clip edge when a strip or a control is redrawn.
        if (!areas.containsRectangle(owner.getLocalBounds())) {
            juce::RectangleList<int> pixels;
            for (const auto& area:areas)
                pixels.add(area.toFloat().transformedBy(scale).getSmallestIntegerContainer());
            if (pixels.getNumRectangles()==1) g.reduceClipRegion(pixels.getRectangle(0));
            else g.reduceClipRegion(pixels);
        }
        g.addTransform(scale);
        owner.paintEntireComponent(g,true);
    }

    void timerCallback() override {
        if (!owner.isShowing()) {
            stopTimer();
            return;
        }

        if (!larger.image.isValid()) {
            larger=createRaster(largerSize);
            startTimer(12);
        }
        // Yield between strips so the pre-render does not hold up input.
        if (!larger.dirty.isEmpty()) {
            const auto area=larger.dirty.getRectangle(0);
            juce::RectangleList<int> strip;
            strip.add(area.withHeight(std::min(40,area.getHeight())));
            drawDirty(larger,std::move(strip));
        }
        if (larger.dirty.isEmpty()) {
            largerReady=true;
            stopTimer();
            if (!covers(current.image,viewport)) owner.getParentComponent()->repaint();
        }
    }

    juce::Component& owner;
    ReadPage pageState;
    ReadLiveAreas liveAreas;
    Raster current,larger;
    juce::Rectangle<int> viewport,largerSize;
    std::list<SavedPage> remembered;
    std::optional<SkinPageState> paintedState;
    bool ignoreLayoutRepaints=false,rememberUpdate=false;
    bool largerReady=false;
};
#endif

bool browserActionCaption(const String& name) {
    return name==exportSysExCaption || name=="Save tags" || name=="Remove source";
}

juce::File resources() {
    static constexpr char moduleAnchor=0;
    return soulTrainResources(&moduleAnchor,"SoulTrainJuce");
}

juce::var message(std::initializer_list<std::pair<String, juce::var>> values) {
    auto* object=new juce::DynamicObject;
    for (const auto& [name,value]:values)
        object->setProperty(name,value);
    return juce::var(object);
}

Rect rectangle(const juce::var& value) {
    return value.size()==4 ? Rect(float(value[0]),float(value[1]),float(value[2]),float(value[3])) : Rect{};
}

juce::Colour cssColour(const String& css, juce::Colour fallback) {
    if (!css.startsWith("rgb(") && !css.startsWith("rgba(")) return fallback;
    auto fields=juce::StringArray::fromTokens(css.fromFirstOccurrenceOf("(",false,false).upToFirstOccurrenceOf(")",false,false),", ",{});
    fields.removeEmptyStrings();
    if (fields.size()<3) return fallback;
    const auto colour=juce::Colour::fromRGB(uint8_t(fields[0].getIntValue()),uint8_t(fields[1].getIntValue()),uint8_t(fields[2].getIntValue()));
    return fields.size()==4 ? colour.withAlpha(float(fields[3].getDoubleValue())) : colour;
}

class HitButton;
class HitSlider;

struct Widget {
    explicit Widget(const juce::var& v)
        : role(v["role"].toString()), type(v["type"].toString()), id(v["param"].toString()),
          name(v["label"].toString()), baseline(v["text"].toString()), value(v["value"].toString()),
          group(v["group"].toString()), css(v["className"].toString()), bounds(rectangle(v["rect"])),
          caption(rectangle(v["captionRect"])), thumb(rectangle(v["thumb"])),
          readout(rectangle(v["readout"])), lampArt(rectangle(v["lampArt"])),
          waveRect(rectangle(v["waveRect"])), trackRect(rectangle(v["trackRect"])),thumbRect(rectangle(v["thumbRect"])),
          art(v["art"].toString()), visualType(v["visualType"].toString()),
          trackArt(v["trackArt"].toString()),thumbArt(v["thumbArt"].toString()), placeholder(v["placeholder"].toString()),
          artAnchor(v["artAnchor"].size()==2?soultrain::point(v["artAnchor"]):bounds.getPosition()),
          part(unsigned(juce::jlimit(0,4,int(v["part"])))), option(int(v["choice"])),
          textColour(cssColour(v["style"]["colour"].toString(),juce::Colour(0xff84ddf8))),
          fontSize(float(v["style"]["fontSize"])),fontWeight(int(v["style"]["weight"])),
          selected(bool(v["selected"])), disabled(bool(v["disabled"])) {
        artWidth=bounds.getWidth();
        // The layout stores positions within the group, including lamps that edit independent flags.
        if (led() && (id.startsWith("lfo1_") || id.startsWith("lfo2_"))) {
            const auto flag=id.substring(5);
            if (flag=="phase" || flag=="keysync" || flag=="unipolar") option=-1;
        }
        if (led() && (id=="external_trigger" || id=="external_to_fx")) option=-1;
    }

    bool dial() const { return role=="slider"; }
    bool range() const { return type=="range"; }
    bool picker() const { return role=="combobox" || role=="select"; }
    bool choiceControl() const { return picker() && name!="Select firmware"; }
    bool textInput() const { return role=="input" && type!="range" && type!="checkbox"; }
    bool led() const { return css.contains("a-lamp"); }
    bool performanceToggle() const { return name=="performance"; }
    bool twoCoreToggle() const { return name.startsWith("KS: two CPU cores"); }
    bool partToggle() const { return part && name.startsWith("Enable Part "); }
    bool partTab() const { return name.length()==1 && name.containsOnly("1234"); }
    bool sortHeader() const { return name.startsWith("Sort by "); }
    bool gear() const { return visualType=="gear"; }
    bool standardButton() const {
        return (role=="button" || role=="tab" || role=="summary") &&
               !led() && !gear() && !sortHeader() && !css.contains("a-part-heading") &&
               visualType.isNotEmpty() && visualType!="checkbox";
    }
    int standardHeight() const {
        if (visualType=="choice") return soultrain::choiceBoxHeight;
        if (visualType=="small-button" || visualType=="summary" ||
            (visualType=="button" && art.isNotEmpty() && !sortHeader())) return soultrain::regularButtonHeight;
        return 0;
    }

    void applyStandardHeight() {
        const auto target=float(standardHeight());
        if (target<=0.f || bounds.isEmpty()) return;
        artScaleY=target/bounds.getHeight();
        bounds=bounds.withSizeKeepingCentre(bounds.getWidth(),target);
    }

    void moveButton(juce::Point<float> delta) {
        bounds+=delta;
        caption+=delta;
        lampArt+=delta;
        artAnchor+=delta;
    }

    juce::Rectangle<int> buttonBounds() const {
        if (name=="Show init") return bounds.getUnion(caption).getSmallestIntegerContainer();
        return sortHeader() || standardHeight()>0?bounds.toNearestIntEdges():bounds.getSmallestIntegerContainer();
    }

    String role,type,id,name,baseline,value,group,css;
    Rect bounds,caption,thumb,readout,lampArt;
    Rect waveRect,trackRect,thumbRect;
    juce::Rectangle<int> paintBounds;
    String art,visualType,trackArt,thumbArt,placeholder;
    juce::Point<float> artAnchor;
    unsigned part;
    int option;
    int choiceGroup=-1;
    juce::Colour textColour;
    float fontSize;
    int fontWeight;
    bool selected,disabled;
    bool choiceDown=false;
    float artScaleY=1.f,artWidth=0.f;
    HitButton* hitButton=nullptr;
    HitSlider* fader=nullptr;
};

class HitButton final : public juce::Button {
public:
    enum class Rollover { None, Outline, Fill, Choice };

    explicit HitButton(const String& name,Rollover rollover=Rollover::Outline)
        : juce::Button(name),rollover(rollover) {}
    bool menuOpen() const { return bool(getProperties()[soultrain::choiceMenuOpenProperty]); }
    bool showsPress() const { return isDown() && !clickFinished; }
    bool showsRollover() const {
        return isEnabled() && ((isOver() && !clickFinished) || keyboardFocus || menuOpen());
    }
    soultrain::ChoiceState choiceState() const {
        return soultrain::choiceState(isEnabled(),menuOpen() || showsPress(),showsRollover());
    }
    void finishPressPreview() {
        // JUCE can flash a fast or keyboard click for 100 ms after committing it.
        if (!showsPress()) return;
        clickFinished=true;
        if (onStateChange) onStateChange();
    }

    void paintButton(juce::Graphics& g,bool hover,bool down) override {
        if (clickFinished) hover=down=false;
        if (!isEnabled() || (!hover && !down && !keyboardFocus && !menuOpen())) return;
        if (rollover==Rollover::Fill) {
            if (down || hover) g.fillAll(juce::Colour(down?0x33385b7f:0x17385b7f));
            if (keyboardFocus) {
                g.setColour(juce::Colour(0xff58728b));
                g.drawRect(getLocalBounds().reduced(1));
            }
        } else if (rollover==Rollover::Outline) {
            g.setColour(juce::Colour(down?0x40b0ddff:0x2290c8ff));
            g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1),2,1);
        }
    }

    void buttonStateChanged() override {
        if (!isDown()) clickFinished=false;
    }

    void focusGained(FocusChangeType cause) override {
        // Menu selection restores focus directly. Tab navigation still gets a cue.
        setKeyboardFocusHighlight(rollover==Rollover::Choice?cause==focusChangedByTabKey:cause!=focusChangedByMouseClick);
        juce::Button::focusGained(cause);
    }
    void focusLost(FocusChangeType cause) override {
        setKeyboardFocusHighlight(false);
        juce::Button::focusLost(cause);
    }
    void mouseDown(const juce::MouseEvent& event) override {
        const bool previousClick=std::exchange(clickFinished,false);
        setKeyboardFocusHighlight(false);
        juce::Button::mouseDown(event);
        if (previousClick && showsPress() && onStateChange) onStateChange();
    }
private:
    void setKeyboardFocusHighlight(bool focused) {
        if (keyboardFocus==focused) return;
        keyboardFocus=focused;
        if (onStateChange) onStateChange();
    }

    const Rollover rollover;
    bool keyboardFocus=false,clickFinished=false;
};

bool hasAuthoredRollover(const Widget& w) {
    return w.led() || w.role=="button" || w.role=="tab" || w.role=="summary";
}

class HitSlider final : public juce::Slider {
public:
    class Look final : public juce::LookAndFeel_V4 {
        juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override {
            return {static_cast<const HitSlider&>(slider).rail,{}};
        }
    };

    HitSlider(Look& look,bool vertical,juce::Rectangle<int> area,juce::Rectangle<int> hit,
              juce::Rectangle<int> track,Rect startThumbHit)
        : hitBounds(hit-area.getPosition()),rail(track-area.getPosition()),
          thumbHitAtRailStart(startThumbHit-area.getPosition().toFloat()) {
        setWantsKeyboardFocus(true);
        setSliderStyle(vertical?LinearVertical:LinearHorizontal);
        setTextBoxStyle(NoTextBox,false,0,0);
        setLookAndFeel(&look);
        setMouseDragSensitivity(std::max(1,vertical?rail.getHeight():rail.getWidth()));
        setBounds(area);
    }
    ~HitSlider() override { setLookAndFeel(nullptr); }

    bool hitTest(int x,int y) override {
        return (hitBounds.contains(x,y) || thumbHitBounds().contains(juce::Point<float>{float(x),float(y)})) &&
               Slider::hitTest(x,y);
    }

    void mouseDown(const juce::MouseEvent& event) override {
        setSliderSnapsToMousePosition(!thumbHitBounds().contains(event.position));
        Slider::mouseDown(event);
    }

    void mouseUp(const juce::MouseEvent& event) override {
        setSliderSnapsToMousePosition(true);
        Slider::mouseUp(event);
    }

    float thumbPosition() const {
        return getPositionOfValue(getValue())+float(isVertical()?getY():getX());
    }

    bool keyPressed(const juce::KeyPress& key) override {
        const auto code=key.getKeyCode();
        const auto step=key.getModifiers().isShiftDown()?10:1;
        if (code==juce::KeyPress::upKey || code==juce::KeyPress::rightKey)
            setValue(getValue()+step,juce::sendNotificationSync);
        else if (code==juce::KeyPress::downKey || code==juce::KeyPress::leftKey)
            setValue(getValue()-step,juce::sendNotificationSync);
        else if (code==juce::KeyPress::homeKey || code==juce::KeyPress::endKey)
            setValue(code==juce::KeyPress::homeKey?getMinimum():getMaximum(),juce::sendNotificationSync);
        else return false;
        return true;
    }
    void paint(juce::Graphics& g) override {
        if (hasKeyboardFocus(false)) {
            g.setColour(juce::Colour(0x5cb0ddff));
            g.drawRoundedRectangle(hitBounds.toFloat().reduced(1),2,1);
        }
    }

private:
    Rect thumbHitBounds() const {
        const auto position=getPositionOfValue(getValue());
        return isVertical()?thumbHitAtRailStart.translated(0.f,position-float(rail.getY()))
                           :thumbHitAtRailStart.translated(position-float(rail.getX()),0.f);
    }

    const juce::Rectangle<int> hitBounds,rail;
    const Rect thumbHitAtRailStart;
};

// The picker is a child of the editor. AU hosts never have to open a native
// system popup, even for a long Performance library.
class ChoicePicker final : public juce::Component, private juce::ListBoxModel {
public:
    ChoicePicker(Rect anchor, juce::StringArray entries, int current, bool scroll,
                 const juce::Font& face, std::function<void(int)> callback)
        : labels(std::move(entries)), chosen(current), useList(scroll),
          font(face), captions(size_t(labels.size())), finish(std::move(callback)) {
        const auto availableAbove=anchor.getY()-12;
        const auto availableBelow=float(height)-anchor.getBottom()-12;
        const auto fitting=juce::jlimit(1,16,int(std::floor((std::max(availableAbove,availableBelow)-12)/22)));
        const auto anchorWidth=anchor.toNearestIntEdges().getWidth();
        const auto maxContentWidth=width-2*screenInset-2*menuInset;
        float widest=0.f;
        for (const auto& label:labels) widest=std::max(widest,font.getStringWidthFloat(label));
        const auto columnWidth=std::min(maxContentWidth,int(std::ceil(widest))+textLeft+textRight);
        rows=std::min(fitting,labels.size());
        if (!useList) {
            columns=juce::jlimit(1,maxContentWidth/columnWidth,(labels.size()+fitting-1)/fitting);
            rows=std::max(1,(labels.size()+columns-1)/columns);
            rowHeight=juce::jlimit(12,22,(height-28)/rows);
        }
        const int scrollBar=useList && labels.size()>rows?list.getViewport()->getScrollBarThickness():0;
        const auto requiredWidth=columns*columnWidth+scrollBar+2*menuInset;
        const auto w=std::min(width-2*screenInset,std::max(anchorWidth,requiredWidth));
        const auto h=rows*rowHeight+2*menuInset;
        const int x=juce::jlimit(screenInset,width-w-screenInset,juce::roundToInt(anchor.getX()));
        const auto preferredY=availableBelow>=h?anchor.getBottom()+4:anchor.getY()-h-4;
        const int y=juce::jlimit(8,height-h-8,juce::roundToInt(preferredY));
        menu={x,y,w,h};
        setComponentID("soulTrainPicker");
        setWantsKeyboardFocus(true);
        if (useList) {
            list.setComponentID("soulTrainPickerList");
            list.setTitle("Options");
            list.setRowHeight(rowHeight);
            list.setMouseMoveSelectsRows(true);
            list.setColour(juce::ListBox::backgroundColourId,juce::Colours::transparentBlack);
            addAndMakeVisible(list);
            list.setBounds(menu.reduced(menuInset));
            list.selectRow(std::max(0,chosen));
        } else {
            for (int i=0;i<labels.size();++i) {
                auto button=std::make_unique<HitButton>(labels[i],HitButton::Rollover::None);
                const auto column=i/rows;
                const auto left=column*(w-2*menuInset)/columns;
                const auto right=(column+1)*(w-2*menuInset)/columns;
                button->setBounds(x+menuInset+left,y+menuInset+i%rows*rowHeight,right-left,rowHeight);
                button->onClick=[this,i]{choose(i);};
                button->onStateChange=[this,cell=button.get(),i]{
                    if (cell->isOver()) highlightRow(i);
                };
                addAndMakeVisible(*button);
                cells.push_back(std::move(button));
            }
            highlight=std::max(0,chosen);
        }
    }

    ~ChoicePicker() override { list.setModel(nullptr); }

    void allowMultiple(std::vector<bool> initial,std::function<void(int,bool)> callback) {
        checks=std::move(initial);
        change=std::move(callback);
    }

    void fitToMenu() {
        const auto footprint=menu.expanded(13,18).getIntersection({0,0,width,height});
        const auto offset=footprint.getPosition();
        menu-=offset;
        for (auto* child:getChildren()) child->setTopLeftPosition(child->getPosition()-offset);
        setBounds(footprint);
    }

    void dismiss() { finish(-1); }

    void selectIndex(int index) {
        if (juce::isPositiveAndBelow(index,labels.size())) choose(index);
    }

    void setChosen(int index) {
        if (index==chosen) return;
        if (juce::isPositiveAndBelow(chosen,int(cells.size()))) repaint(cells[size_t(chosen)]->getBounds());
        chosen=index;
        if (juce::isPositiveAndBelow(chosen,int(cells.size()))) repaint(cells[size_t(chosen)]->getBounds());
    }

    void focus() {
        if (isShowing() && getPeer()) {
            if (useList) list.grabKeyboardFocus(); else grabKeyboardFocus();
        }
    }

    void paint(juce::Graphics& g) override {
        if (!menu.reduced(2).contains(g.getClipBounds()))
            juce::DropShadow(juce::Colour(0x77000000),12,{0,5}).drawForRectangle(g,menu);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff303e4e),menu.getTopLeft().toFloat(),
                                              juce::Colour(0xff1f2c3a),menu.getBottomRight().toFloat(),false));
        g.fillRect(menu);
        soultrain::paintChoiceFrame(g,menu.toFloat(),soultrain::ChoiceState::open);
        for (int i=0;i<int(cells.size());++i) {
            const auto& cell=*cells[size_t(i)];
            if (!g.clipRegionIntersects(cell.getBounds())) continue;
            juce::Graphics::ScopedSaveState save(g);
            g.setOrigin(cell.getPosition());
            row(g,i,cell.getWidth(),rowHeight,i==highlight);
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        if (!menu.contains(event.getPosition())) finish(-1);
    }

    bool keyPressed(const juce::KeyPress& key) override {
        const auto code=key.getKeyCode();
        if (code==juce::KeyPress::escapeKey || code==juce::KeyPress::tabKey) {
            finish(-1);
            return true;
        }
        if (useList) return false;
        if (code==juce::KeyPress::returnKey || code==juce::KeyPress::spaceKey) {
            choose(highlight);
            return true;
        }
        int candidate=highlight;
        if (code==juce::KeyPress::upKey) --candidate;
        else if (code==juce::KeyPress::downKey) ++candidate;
        else if (code==juce::KeyPress::leftKey) candidate-=rows;
        else if (code==juce::KeyPress::rightKey) candidate+=rows;
        else if (code==juce::KeyPress::homeKey) candidate=0;
        else if (code==juce::KeyPress::endKey) candidate=labels.size()-1;
        else if (const auto character=key.getTextCharacter(); juce::CharacterFunctions::isLetterOrDigit(character)) {
            const auto now=juce::Time::getMillisecondCounter();
            if (now-lastTyped>650) typed.clear();
            lastTyped=now;
            typed+=String::charToString(character);
            for (int i=0;i<labels.size();++i)
                if (labels[i].startsWithIgnoreCase(typed)) {candidate=i;break;}
        } else return false;
        highlightRow(juce::jlimit(0,labels.size()-1,candidate));
        return true;
    }

private:
    static constexpr int screenInset=8,menuInset=6,textLeft=20,textRight=6;
    juce::StringArray labels;
    int chosen;
    bool useList;
    juce::Font font;
    std::vector<soultrain::ChoiceCaption> captions;
    std::function<void(int)> finish;
    std::function<void(int,bool)> change;
    std::vector<bool> checks;
    int rows=1,columns=1,rowHeight=22,highlight=0;
    uint32_t lastTyped=0;
    String typed;
    juce::Rectangle<int> menu;
    juce::ListBox list{"Choices",this};
    std::vector<std::unique_ptr<HitButton>> cells;

    int getNumRows() override { return labels.size(); }
    String getNameForRow(int index) override { return labels[index]; }
    void listBoxItemClicked(int index,const juce::MouseEvent&) override { choose(index); }
    void returnKeyPressed(int index) override { choose(index); }
    void paintListBoxItem(int index,juce::Graphics& g,int w,int h,bool selected) override {
        row(g,index,w,h,selected);
    }

    void highlightRow(int index) {
        if (index==highlight) return;
        repaint(cells[size_t(highlight)]->getBounds());
        highlight=index;
        repaint(cells[size_t(highlight)]->getBounds());
    }

    void choose(int index) {
        if (!change) { finish(index); return; }
        if (index<0 || size_t(index)>=checks.size()) return;
        checks[size_t(index)]=!checks[size_t(index)];
        repaint(cells[size_t(index)]->getBounds());
        change(index,checks[size_t(index)]);
    }

    void row(juce::Graphics& g,int index,int w,int h,bool selected) {
        if (!juce::isPositiveAndBelow(index,labels.size())) return;
        if (selected) {
            g.setColour(juce::Colour(0x25c5dbf6));
            g.fillRect(0,0,w,h);
        }
        const auto ink=index==chosen?soultrain::choiceInk:juce::Colour(selected?0xffeff8ff:0xffd6e6f6);
        if (change?checks[size_t(index)]:index==chosen) {
            const auto mid=h*.5f;
            juce::Path check;
            check.startNewSubPath(5.f,mid);
            check.lineTo(8.f,mid+3.f);
            check.lineTo(13.f,mid-3.f);
            g.setColour(ink);
            g.strokePath(check,juce::PathStrokeType(1.4f));
        }
        const Rect box(float(textLeft),0.f,float(w-textLeft-textRight),float(h));
        auto& caption=captions[size_t(index)];
        caption.set(font,labels[index],box.getWidth());
        caption.draw(g,box,juce::Justification::centredLeft,ink,1.f,
                     index==chosen?soultrain::ChoiceGlow::normal:soultrain::ChoiceGlow::none);
    }
};

const std::array<String,5> pageNames{"Main","Extra","Browser","Perf","Setup"};
const std::array<String,7> effectNames{"Delay","Reverb","Chorus","Dist","EQ","Panning","Vocoder"};
constexpr int chorusEffect=2;
constexpr auto chorusOptionsCaption="Chorus options";

String makeText(const juce::var& row,const String& key) { return row[juce::Identifier(key)].toString(); }

int browserSortColumn(const String& label) {
    static const juce::StringArray columns{"Name","Instrument","Type","Tags","Compatibility","Source","No.","Bank"};
    return columns.indexOf(label)+1;
}

class PickerLayer final : public juce::Component {
public:
    explicit PickerLayer(std::function<void()> close) : dismiss(std::move(close)) {
        setInterceptsMouseClicks(false,true);
    }

    void mouseDown(const juce::MouseEvent&) override { dismiss(); }

private:
    std::function<void()> dismiss;
};

class ColumnGrip final : public juce::Component {
public:
    explicit ColumnGrip(std::function<void(int)> move) : resize(std::move(move)) {
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    }

    void mouseDown(const juce::MouseEvent& event) override {
        last=getParentComponent()->getLocalPoint(nullptr,event.getScreenPosition()).x;
    }

    void mouseDrag(const juce::MouseEvent& event) override {
        const auto next=getParentComponent()->getLocalPoint(nullptr,event.getScreenPosition()).x;
        resize(next-last);
        last=next;
    }

private:
    std::function<void(int)> resize;
    int last=0;
};

class BrowserList final : public juce::ListBox {
public:
    using juce::ListBox::ListBox;

    bool keyPressed(const juce::KeyPress& key) override {
        const int previous=getSelectedRow();
        const auto handled=juce::ListBox::keyPressed(key);
        const int row=getSelectedRow();
        if (handled && row>=0 && row!=previous && getListBoxModel())
            getListBoxModel()->returnKeyPressed(row);
        return handled;
    }
};

}

class SoulTrainJuce::Canvas final : public juce::Component, private juce::MultiTimer, private juce::ListBoxModel {
public:
    Canvas(SoulTrainJuce& owner,Processor& p,Callbacks callbacks,juce::File root)
        : view(owner),processor(p),host(std::move(callbacks)),assets(root==juce::File{}?resources():std::move(root)),skin(assets) {
        catalog=read("catalog.json");
        if (!catalog.isObject() || !assets.getChildFile("KS-main-0.png").existsAsFile())
            throw std::runtime_error("SoulTrain JUCE art resources missing");
        semibold=skin.font(13.f,600).getTypefacePtr();
        list.setComponentID("soulTrainPresetList");
        list.setColour(juce::ListBox::backgroundColourId,juce::Colours::transparentBlack);
        list.setRowHeight(30);
        list.getViewport()->getVerticalScrollBar().setColour(juce::ScrollBar::thumbColourId,juce::Colour(0x6274869b));
        list.getViewport()->getVerticalScrollBar().setColour(juce::ScrollBar::trackColourId,juce::Colours::transparentBlack);
        setComponentID("soulTrainJuce");
        setSize(width,height);
        setOpaque(true);
#if JUCE_WINDOWS
        setCachedComponentImage(new SkinFrame(*this,[this]{return frameState();},[this]{return frameLiveAreas();}));
#endif
        pickerLayer.setBounds(getLocalBounds());
        pickerLayer.setAlwaysOnTop(true);
        addAndMakeVisible(pickerLayer);
        addMouseListener(this,true);
        rebuild();
        startTimer(EditorTimer,63);
    }

    ~Canvas() override {
        stopTimer(EditorTimer);
        stopTimer(DrumEditorTimer);
        closeParameterEntry();
        closePicker();
        removeMouseListener(this);
        list.setModel(nullptr);
        sliders.clear();
        fields.clear();
        buttons.clear();
    }

    bool goTo(String destination) {
        if (destination=="Performance") destination="Perf";
        if (std::find(pageNames.begin(),pageNames.end(),destination)==pageNames.end() ||
            (destination=="Perf" && processor.isKStation())) return false;
        if (destination==selectedPage) return true;
        selectedPage=destination;
        rebuild();
        return true;
    }

    void paint(juce::Graphics& g) override {
        // The opaque base covers partial updates away from the filtered border.
        if (!opaqueBackdrop || !getLocalBounds().reduced(1).contains(g.getClipBounds()))
            g.fillAll(juce::Colour(0xff202630));
        drawImage(g,backdrop,imageOrigin,backdropScale);
        if (page=="Setup" && backdrop.isValid()) paintSetupDividers(g);
        drawImage(g,fxArt,fxOrigin,fxScale);
        for (const auto* panel:panels) drawImage(g,panel->art,panel->area.getPosition(),panel->scale);
        if (!backdrop.isValid()) return;

        // Paint the shadows before any faces so adjacent controls stay above them.
        const auto shadowClip=g.getClipBounds();
        for (const auto& [widget,shadow]:buttonShadows)
            if (shadow.visibleIn(shadowClip) && widget->hitButton && widget->hitButton->isVisible())
                shadow.paint(g,captionOpacity(widget->bounds));

        const bool effectsPage=usesEffects();
        const bool listPage=page=="Browser" || page=="Perf";
        for (const auto& widget:widgets) {
            if (effectsPage && !g.clipRegionIntersects(widget.paintBounds)) continue;
            if (listPage &&
                !g.clipRegionIntersects(widget.bounds.expanded(16.f,28.f).getSmallestIntegerContainer()))
                continue;
            paintWidget(g,widget);
        }
        const auto header=rectangle(regions["tableHeader"]["rect"]);
        const auto frame=rectangle(regions["tableFrame"]["rect"]);
        for (size_t index=0;index<fixedText.size();++index) {
            const auto& ink=fixedText[index];
            auto& caption=buttonCaptions[index];
            if (effectsPage && !ink.clipIntersects(g) &&
                (!caption.widget || !g.clipRegionIntersects(caption.widget->buttonBounds()))) continue;
            if (page=="Perf" && !g.clipRegionIntersects(ink.bounds.expanded(8.f).getSmallestIntegerContainer()))
                continue;
            if (page=="Browser" && header.contains(ink.bounds.getCentre()))
                continue;
            if (page=="Browser" && ink.current=="Tags" && ink.bounds.getY()>frame.getBottom()) {
                juce::Graphics::ScopedSaveState save(g);
                g.addTransform(juce::AffineTransform::translation(tagFieldOffset,0));
                ink.draw(g);
            } else if (caption.widget) paintButtonCaption(g,ink,caption);
            else ink.draw(g,0,captionOpacity(ink.bounds));
        }
        if (g.clipRegionIntersects(lcdGlowBounds)) {
            drawRegion(g,"lcdModel",lcdText[0],true);
            drawRegion(g,"lcdName",lcdText[1],true);
        }
        drawFirmwareName(g);
        if (page=="Perf") paintPerformance(g);
        if (page=="Browser") paintBrowser(g);
        if (page=="Setup")
            paintSetup(g);
    }

    String scene() const { return usesEffects()?effectKey(sections.effect,sections.chorusOptions):key(0); }
    void refreshBrowser() { updateBrowser();refreshLcd(); }
    void mouseMove(const juce::MouseEvent&) override { refreshHoverReadout(); }
    void mouseExit(const juce::MouseEvent&) override { refreshHoverReadout(); }
    void mouseDown(const juce::MouseEvent& event) override {
        if (parameterEntry && event.originalComponent!=parameterEntry.get() &&
            !parameterEntry->isParentOf(event.originalComponent))
            closeParameterEntry();
    }
    void mouseDoubleClick(const juce::MouseEvent& event) override {
        if (event.originalComponent!=this || !event.mods.isLeftButtonDown() ||
            picker || parameterEntry || drumSequencer || draggedParameter || rebuildQueued ||
            processor.isFirmwareLoading()) return;

        const auto position=event.getEventRelativeTo(this).position;
        const auto target=std::find_if(sliders.begin(),sliders.end(),[&](const SliderBinding& slider) {
            return slider.caption.contains(position) && slider.control->isEnabled();
        });
        if (target!=sliders.end()) openParameterEntry(*target);
    }

private:
    enum { EditorTimer=1, DrumEditorTimer };
    struct SliderBinding {
        const Widget* widget=nullptr;
        Rect caption;
        std::unique_ptr<juce::Slider> control;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };
    struct BrowserColumn { String key,label; Rect area; };
    using MenuItems=std::vector<std::pair<juce::var,String>>;
    [[maybe_unused]] SoulTrainJuce& view;
    Processor& processor;
    Callbacks host;
    const juce::File assets;
    soultrain::SkinArt skin;
    std::map<String,juce::var> layouts;
    std::map<String,juce::var> chorusLayouts;
    std::unique_ptr<soultrain::DrumSequencer> drumSequencer;
    String statusMessage;
    struct TextSlot {
        std::unique_ptr<soultrain::Caption> caption;
        double since=0;
        float travel=0.f, pan=0.f;
    };
    mutable std::map<String,TextSlot> textSlots;
    mutable std::map<String,soultrain::ChoiceCaption> choiceCaptions;
    struct ReadoutGlow {
        String value;
        std::optional<soultrain::CaptionGlow> glow;
    };
    mutable std::array<ReadoutGlow,256> readoutGlows;
    std::vector<soultrain::Caption> fixedText;
    std::list<std::pair<String,std::vector<soultrain::Caption>>> captionLayouts;
#if JUCE_WINDOWS
    std::map<String,soultrain::Caption> authoredCaptions;
    struct EffectLayout {
        juce::Image overlay;
        juce::Rectangle<int> repaintArea;
        String outside;
    };
    std::map<String,EffectLayout> effectLayouts;
    String warmedSections;
#endif
    struct ButtonCaption {
        const Widget* widget=nullptr;
        std::array<std::unique_ptr<soultrain::Caption>,3> states;
    };
    std::vector<ButtonCaption> buttonCaptions;
    std::vector<std::pair<const Widget*,soultrain::ControlShadow>> buttonShadows;
    std::vector<std::pair<const Widget*,int>> paintedValues;
    struct ChoiceGroup {
        std::vector<const Widget*> members;
        const Widget* pressed=nullptr;
        const Widget* selected=nullptr;
        const Widget* shown() const { return pressed?pressed:selected; }
    };
    std::vector<ChoiceGroup> choiceGroups;
    std::array<String,2> lcdText;
    std::array<String,4> lastPartNames;
    std::array<Rect,4> performanceAreas{};
    juce::StringArray lastSetupValues;
    int lastSetupZoom=0;
    std::array<bool,7> lastFxLights{};
    float performanceOpacity=.4f;
    juce::var catalog, regions, browser, columnLayout;
    juce::StringArray hiddenColumns;
    std::vector<BrowserColumn> visibleColumns;
    std::map<String,float> columnWidths;
    std::map<String,std::unique_ptr<ColumnGrip>> columnGrips;
    String selectedPage="Main", page, model, selectedPatch, selectedPatchName, tags, savedTags;
    bool selectedPatchIsKSPlus=false;
    bool tagsEdited=false,browserRefreshQueued=false,rebuildQueued=false;
    float tagFieldOffset=0;
    struct Sections {
        int effect=0;
        bool voices=false,arp=false,chorusOptions=false;
    };
    Sections sections;
    std::optional<Sections> requestedSections;
    bool sectionsOnlyQueued=false;
    bool lastReady=false,lastLoading=false,lastPerformance=false;
    unsigned lastPart=0;
    uint8_t lastMask=0;
    int browserTick=0;
    std::array<int,3> pwm{},pitch{};
    int filterMode=0,mixerMode=0;
    std::vector<Widget> widgets;
    const Widget* draggedParameter=nullptr;
    const Widget* adjustedParameter=nullptr;
    const Widget* inspectedParameter=nullptr;
    double adjustmentExpires=0;
    juce::Image backdrop, fxArt;
    bool opaqueBackdrop=false;
    juce::Image lcdGlowMask;
    juce::Rectangle<int> lcdGlowBounds;
    struct Panel {
        juce::Image art;
        Rect area;
        int scale=1;
        juce::var layout;
        std::vector<soultrain::Caption> captions;
    };
    juce::Point<float> imageOrigin,fxOrigin;
    int backdropScale=1,fxScale=1;
    std::map<String,Panel> panelLayouts;
    std::vector<const Panel*> panels;
    juce::Typeface::Ptr semibold;
    struct WaveBinding {
        Widget widget;
        std::unique_ptr<soultrain::WaveIndicator> control;
    };
    std::vector<WaveBinding> waves;
    std::vector<std::unique_ptr<HitButton>> buttons;
    HitButton* sequencerEditButton=nullptr;
    HitButton* drumVoiceEditButton=nullptr;
    Processor::DrumEditInfo drumVoice;
    bool editingDrumVoice=false;
    HitSlider::Look faderLook;
    std::vector<SliderBinding> sliders;
    std::vector<std::unique_ptr<juce::TextEditor>> fields;
    std::unique_ptr<juce::TextEditor> tagEditor;
    std::unique_ptr<juce::TextEditor> parameterEntry;
    std::map<String,juce::RangedAudioParameter*> parameters;
    bool parametersForK=false;
    BrowserList list{"SoulTrain presets",this};
    std::unique_ptr<ChoicePicker> picker;
    unsigned pickerPart=0;
    uint64_t noteLearnToken=0;
    PickerLayer pickerLayer{[this]{if(picker)picker->dismiss();}};
    juce::Component::SafePointer<juce::Component> pickerTrigger;
    juce::Component::SafePointer<soultrain::WaveIndicator> wavePicker;

#if JUCE_WINDOWS
    std::optional<SkinPageState> frameState() const {
        if (processor.isFirmwareLoading() || picker || parameterEntry || drumSequencer) return {};

        SkinPageState state;
        state.layout=scene();
        state.values={int(lastReady),int(lastPart),int(lastPerformance),int(lastMask),
                      int(sections.arp),int(sections.voices),filterMode,mixerMode,int(seqSelected()),int(drumVoice.bank!=0)};
        state.values.insert(state.values.end(),pwm.begin(),pwm.end());
        state.values.insert(state.values.end(),pitch.begin(),pitch.end());
        state.labels.add(processor.osFile.getFullPathName());

        for (const auto& w:widgets) {
            if (w.id.isNotEmpty()) state.values.push_back(raw(w));
            if (w.hitButton) {
                state.values.push_back(int(w.hitButton->isEnabled()));
                state.values.push_back(int(active(w)));
            }
            if (w.role=="tab") state.values.push_back(int(effectLit(w.name)));
            if (w.picker()) state.labels.add(menuValue(w));
        }
        // These controls can be waiting for a message-thread update from the
        // host. Capture the values actually painted, as well as the parameters.
        for (const auto& slider:sliders) state.values.push_back(juce::roundToInt(slider.control->getValue()));
        for (const auto& wave:waves) state.values.push_back(wave.control->value());

        if (page=="Perf")
            for (const auto& name:lastPartNames) state.labels.add(name);
        else if (page=="Setup")
            state.labels.addArray(setupValues());
        else if (page=="Browser") {
            // The browser publishes immutable snapshots. The viewport and text
            // editors can still change independently of a snapshot.
            state.browser=browser;
            state.labels.add(selectedPatch);
            state.labels.add(selectedPatchName);
            state.labels.add(tags);
            state.labels.add(String(tagFieldOffset));
            for (const auto& column:visibleColumns) {
                state.labels.add(column.key);
                state.labels.add(column.area.toString());
            }
        }
        return state;
    }

    juce::RectangleList<int> frameLiveAreas(bool includeLcd=true) const {
        juce::RectangleList<int> areas;
        const auto add=[&](juce::Rectangle<int> area){areas.add(area.getIntersection(getLocalBounds()));};
        const auto addChild=[&](const juce::Component& child){
            add(getLocalArea(&child,child.getLocalBounds()).expanded(2));
        };
        if (includeLcd) add(lcdGlowBounds);
        for (const auto& w:widgets)
            if (w.hitButton && (w.hitButton->showsRollover() || w.hitButton->isDown() ||
                                w.hitButton->hasKeyboardFocus(true)))
                add(mainWidgetPaintBounds(w));
        for (const auto& group:choiceGroups)
            if (group.pressed)
                for (const auto* member:group.members) add(mainWidgetPaintBounds(*member));
        for (const auto& slider:sliders)
            if (slider.control->hasKeyboardFocus(true) || slider.control->isMouseOverOrDragging())
                addChild(*slider.control);
        for (const auto& wave:waves)
            if (wave.control->hasKeyboardFocus(true) || wave.control->isMouseOverOrDragging())
                addChild(*wave.control);
        if (parameterEntry) addChild(*parameterEntry);
        if (page=="Browser") {
            addChild(list);
            for (const auto& field:fields) addChild(*field);
            if (tagEditor) addChild(*tagEditor);
        }
        return areas;
    }
#endif

    bool usesEffects() const { return page=="Main" || page=="Extra"; }
    String key(int tab) const { return model+"-"+page.toLowerCase()+"-"+String(tab); }
    String effectKey(int tab,bool chorusOptions) const {
        return key(tab)+(tab==chorusEffect && chorusOptions?"-options":"");
    }
    juce::var read(const String& filename) {
        const auto cached=layouts.find(filename);
        if (cached!=layouts.end()) return cached->second;
        return layouts.emplace(filename,juce::JSON::parse(assets.getChildFile(filename))).first->second;
    }

    juce::var effectLayout(int tab,bool chorusOptions) {
        const auto authored=read(key(tab)+".json");
        if (tab!=chorusEffect || chorusOptions==(page=="Extra") || !authored.isObject()) return authored;
        const auto name=effectKey(tab,chorusOptions);
        if (const auto cached=chorusLayouts.find(name);cached!=chorusLayouts.end()) return cached->second;

        // Main and Extra use the same Effects area. The original Extra art
        // supplies the two Chorus controls shown when the gear is open.
        const auto source=read(model+(chorusOptions?"-extra-":"-main-")+String(chorusEffect)+".json");
        const auto area=rectangle(authored["regions"]["effectsControls"]["rect"]);
        if (!source.isObject() || area.isEmpty() ||
            area!=rectangle(source["regions"]["effectsControls"]["rect"])) return authored;

        auto layout=authored.getDynamicObject()->clone();
        const auto chorusContent=[&](const juce::var& item) {
            return area.contains(rectangle(item["rect"]).getCentre()) || item["text"]==chorusOptionsCaption;
        };
        for (const auto* field:{"widgets","captionRuns"}) {
            const auto* base=authored[field].getArray();
            const auto* replacement=source[field].getArray();
            if (!base || !replacement) return authored;
            juce::Array<juce::var> content;
            content.ensureStorageAllocated(base->size());
            for (const auto& item:*base) if (!chorusContent(item)) content.add(item);
            for (const auto& item:*replacement) if (chorusContent(item)) content.add(item);
            layout->setProperty(field,juce::var(std::move(content)));
        }
        return chorusLayouts.emplace(name,juce::var(layout.release())).first->second;
    }

    std::optional<Widget> chorusGear(const juce::var& layout) const {
        const auto area=rectangle(layout["regions"]["effectsControls"]["rect"]);
        const auto* nodes=layout["widgets"].getArray();
        if (area.isEmpty() || !nodes) return {};
        for (const auto& node:*nodes) {
            if (node["label"]!="Voices options" || node["visualType"]!="gear") continue;
            Widget gear(node);
            gear.name=chorusOptionsCaption;
            gear.moveButton({area.getRight()-gear.bounds.getRight(),0.f});
            return gear;
        }
        return {};
    }
    juce::Image picture(const String& filename) const {
        return juce::ImageCache::getFromFile(assets.getChildFile(filename));
    }
    static juce::Point<float> origin(const juce::var& v) { return soultrain::point(v); }
    static void drawImage(juce::Graphics& g,const juce::Image& image,juce::Point<float> p,int scale) {
        if (!image.isValid()) return;
        const auto factor=1.f/std::max(1,scale);
        const Rect bounds(p.x,p.y,image.getWidth()*factor,image.getHeight()*factor);
        if (g.clipRegionIntersects(bounds.getSmallestIntegerContainer()))
            g.drawImageTransformed(image,juce::AffineTransform::scale(factor).translated(p));
    }
    void erase(juce::Graphics& g,Rect rect) const {
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(rect.getSmallestIntegerContainer());
        g.setOpacity(1.f);
        drawImage(g,backdrop,imageOrigin,backdropScale);
        drawImage(g,fxArt,fxOrigin,fxScale);
        for (const auto* panel:panels) drawImage(g,panel->art,panel->area.getPosition(),panel->scale);
    }
    juce::Font font(float size,bool lcd=false) const { return skin.font(size,400,lcd); }

    void text(juce::Graphics& g,const String& value,Rect area,juce::Colour colour,float size,
              juce::Justification align=juce::Justification::centred,bool lcd=false) const {
        g.setColour(colour);
        g.setFont(font(size,lcd));
        g.drawFittedText(value,area.getSmallestIntegerContainer(),align,1,.85f);
    }

    unsigned partFor(const Widget& widget) const { return widget.part?widget.part:processor.currentPart()+1; }

    int oscillatorFor(const Widget& w) const {
        if (w.id.startsWith("osc") && juce::CharacterFunctions::isDigit(w.id[3]))
            return juce::jlimit(1,3,int(w.id[3]-'0'))-1;
        return juce::jlimit(1,3,w.group.fromFirstOccurrenceOf("Oscillator ",false,false).getIntValue())-1;
    }

    String binding(const Widget& w) const {
        if (!w.dial() && !w.textInput()) return w.id;
        if (usesEffects() && page=="Main" && w.id.startsWith("osc")) {
            const auto osc=oscillatorFor(w);
            const auto prefix="osc"+String(osc+1)+"_";
            if (w.id.endsWith("_pulse_width")) {
                const std::array<String,3> map{"pulse_width","lfo2_pwm","env_pwm"};
                return prefix+map[size_t(pwm[size_t(osc)])];
            }
            if (w.id.endsWith("_env_pitch")) {
                const auto selected=pitch[size_t(osc)];
                if (processor.isKStation() && selected==2) return prefix+"bend_depth";
                const std::array<String,3> map{"env_pitch","lfo1_pitch","level_mod"};
                return prefix+map[size_t(selected)];
            }
        }
        if (page=="Main" && w.id=="filter_lfo2")
            return std::array<String,3>{"filter_lfo2","filter_velocity","filter_keytrack"}[size_t(filterMode)];
        if (page=="Extra" && w.id=="ring_level_mod" && mixerMode) return "external_level";
        return w.id;
    }

    String assignedKnobCaption(const Widget& w) const {
        String group;
        if (w.id.endsWith("_pulse_width")) group="Oscillator "+String(oscillatorFor(w)+1)+" PWM source";
        else if (w.id.endsWith("_env_pitch")) group="Oscillator "+String(oscillatorFor(w)+1)+" knob mode";
        else if (w.id=="filter_lfo2") group="Filter knob mode";
        else if (w.id=="ring_level_mod") group="Mixer knob mode";
        const auto choice=std::find_if(widgets.begin(),widgets.end(),[&](const Widget& option) {
            return group.isNotEmpty() && option.group==group && active(option);
        });
        return choice!=widgets.end()?choice->name:spec(w)["label"].toString().toLowerCase();
    }

    const juce::var& parameterSpec(const String& id) const {
        return catalog[juce::Identifier(model)][juce::Identifier(id)];
    }
    const juce::var& spec(const Widget& w) const { return parameterSpec(binding(w)); }
    juce::RangedAudioParameter* parameter(const Widget& w) const {
        const auto i=parameters.find(String(partFor(w))+":"+binding(w));
        return i==parameters.end()?nullptr:i->second;
    }
    bool drumRhythm() const {
        if (auto* mode=processor.uiParameter("arp_pattern"))
            return juce::roundToInt(mode->convertFrom0to1(mode->getValue()))==7;
        return false;
    }
    bool seqSelected() const { return processor.userDrumRhythmSelected() && drumRhythm(); }
    int raw(const Widget& w) const {
        if (w.id=="arp_rhythm" && seqSelected())
            return Processor::UserDrumRhythm;
        if (auto* p=parameter(w))
            return juce::roundToInt(p->convertFrom0to1(p->getValue()));
        return int(spec(w)["default"]);
    }
    const Widget* parameterReadout() {
        if (picker) return nullptr;
        if (draggedParameter && available(*draggedParameter)) return draggedParameter;
        if (isMouseOver(true)) {
            const auto position=getMouseXYRelative().toFloat();
            const auto hovered=std::find_if(widgets.begin(),widgets.end(),[&](const Widget& w){
                return w.id.isNotEmpty() && w.bounds.contains(position) && available(w);
            });
            if (hovered!=widgets.end()) return &*hovered;
        }
        return adjustedParameter && juce::Time::getMillisecondCounterHiRes()<adjustmentExpires &&
               available(*adjustedParameter)?adjustedParameter:nullptr;
    }
    String readoutLabel(const Widget& w) const {
        if (w.id=="arp_rhythm") return "Rhythm";
        const auto label=spec(w)["label"].toString();
        const auto id=binding(w);
        if (id.startsWith("osc") && (id.endsWith("_bend_depth") || id.endsWith("_lfo2_level")))
            return "OSC "+String(oscillatorFor(w)+1)+" "+label;
        return label;
    }
    static String midiNoteLabel(int note) {
        return juce::MidiMessage::getMidiNoteName(note,true,true,4)+String::fromUTF8(" · ")+String(note);
    }

    String valueLabel(const Widget& w,int value) const {
        if (w.id=="arp_rhythm") {
            if (value==Processor::UserDrumRhythm) return "SEQ";
            if (drumRhythm()) return String(value+1);
            return value?String(value):String("Off");
        }
        if (w.id=="vocoder_sibilance_type" && value==0) return "High pass";
        if (w.id=="part_low" || w.id=="part_high") return midiNoteLabel(juce::jlimit(0,127,value));
        const auto s=spec(w);
        const auto offset=int(s["offset"]);
        const auto index=value-int(s["min"]);
        const auto options=s["choices"];
        if (index>=0 && index<options.size()) {
            const auto label=options[index].toString();
            if (w.id=="lfo1_wave" || w.id=="lfo2_wave")
                return label.substring(0,1).toUpperCase()+label.substring(1).toLowerCase().replace("s/h","S/H");
            return label;
        }
        return String(value+offset);
    }

    void refreshLcd() {
        const auto drumLoad=processor.drumMapLoadStatus();
        if (drumLoad==Processor::DrumLoadStatus::Loading || drumLoad==Processor::DrumLoadStatus::Rejected) {
            const std::array<String,2> next{"KS",drumLoad==Processor::DrumLoadStatus::Loading?
                "Loading Drum Map...":"Drum Map unavailable"};
            if(lcdText!=next){lcdText=next;repaint(lcdGlowBounds);}
            return;
        }
        const bool showDrum=editingDrumVoice && drumVoice.note>=0 && !draggedParameter &&
            (!adjustedParameter || juce::Time::getMillisecondCounterHiRes()>=adjustmentExpires);
        const auto* inspected=showDrum?nullptr:parameterReadout();
        inspectedParameter=inspected;
        std::array<String,2> next{
            inspected?readoutLabel(*inspected):processor.isKStation()?String("K-Station"):String("KS"),
            inspected?valueLabel(*inspected,raw(*inspected)):processor.isFirmwareLoading()?String("Loading firmware..."):
                processor.firmwareReady()?processor.currentName():String("Load firmware")
        };
        if (showDrum)
            next={"Drum "+juce::MidiMessage::getMidiNoteName(drumVoice.note,true,true,4)+" / MIDI "+String(drumVoice.note),
                  drumVoice.name};
        if (lcdText==next) return;
        lcdText=std::move(next);
        repaint(lcdGlowBounds);
    }

    void refreshHoverReadout() {
        if (!drumSequencer && !picker && !draggedParameter && parameterReadout()!=inspectedParameter)
            refreshLcd();
    }

    static float scrollOffset(const TextSlot& slot,double now) {
        if (slot.travel<=0.f) return 0.f;
        const auto duration=std::max(5000.f,4000.f+50.f*slot.travel);
        const auto phase=std::fmod(std::max(0.0,now-slot.since-1200.0),2.0*duration)/duration;
        return slot.travel*float(juce::jlimit(0.,1.,(std::min(phase,2.-phase)-.2)/.6));
    }

    bool lcdScrollChanged() const {
        const auto now=juce::Time::getMillisecondCounterHiRes();
        for (const auto* key:{"lcdModel","lcdName"}) {
            const auto slot=textSlots.find(key);
            if (slot!=textSlots.end() && slot->second.pan!=scrollOffset(slot->second,now)) return true;
        }
        return false;
    }

    void setParameter(const Widget& w,int value) {
        if (auto* p=parameter(w)) {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(float(value)));
            p->endChangeGesture();
        }
    }

    void updateLcdGlowMask() {
        const auto top=rectangle(regions["lcdModel"]["rect"]);
        const auto bottom=rectangle(regions["lcdName"]["rect"]);
        const auto bounds=top.isEmpty() || bottom.isEmpty()?juce::Rectangle<int>{}:
            top.getUnion(bottom).expanded(15.f,7.f).getSmallestIntegerContainer();
        if (bounds==lcdGlowBounds) return;
        lcdGlowBounds=bounds;
        lcdGlowMask={};
        if (bounds.isEmpty()) return;

        // Both rows share one feathered light mask. The glyphs keep their
        // individual line clips so long names can still scroll on the LCD.
        lcdGlowMask=juce::Image(juce::Image::ARGB,bounds.getWidth(),bounds.getHeight(),true);
        juce::Image::BitmapData pixels(lcdGlowMask,juce::Image::BitmapData::writeOnly);
        for (int y=0;y<pixels.height;++y)
            for (int x=0;x<pixels.width;++x) {
                const auto edge=std::min({x,y,pixels.width-1-x,pixels.height-1-y});
                const auto fade=juce::jlimit(0.f,1.f,float(edge)/9.f);
                pixels.setPixelColour(x,y,juce::Colours::white.withAlpha(fade*fade*(3.f-2.f*fade)));
            }
    }

    void cachedText(juce::Graphics& g,const String& key,const String& value,Rect rect,Rect clip,
                    juce::Colour colour,float size,int weight,juce::Justification align,
                    const String& shadow={},bool lcd=false,float opacity=1.f) const {
        auto& slot=textSlots[key];
        if (!slot.caption)
            slot.caption=std::make_unique<soultrain::Caption>(skin.font(size,weight,lcd),rect,clip,colour,shadow,1.f,align);
        const auto now=juce::Time::getMillisecondCounterHiRes();
        if (slot.caption->current!=value) {
            std::optional<soultrain::CaptionGlow>* cachedGlow=nullptr;
            if (lcd && key=="lcdName" && value.isNotEmpty() && value.containsOnly("0123456789-")) {
                auto& entry=readoutGlows[static_cast<unsigned>(value.getIntValue())%readoutGlows.size()];
                if (entry.value!=value) {
                    entry.value=value;
                    entry.glow.reset();
                }
                cachedGlow=&entry.glow;
            }
            slot.caption->set(value,cachedGlow);
            slot.since=now;
            slot.travel=lcd?std::max(0.f,std::ceil(skin.font(size,weight,true).getStringWidthFloat(value)-rect.getWidth())):0.f;
        }
        slot.pan=scrollOffset(slot,now);
        const bool feathered=lcd && lcdGlowMask.isValid();
        if (feathered) {
            juce::Graphics::ScopedSaveState save(g);
            if (g.reduceClipRegion(lcdGlowMask,juce::AffineTransform::translation(lcdGlowBounds.getPosition().toFloat())))
                slot.caption->drawGlow(g,slot.pan,opacity);
        }
        slot.caption->draw(g,slot.pan,opacity,!feathered);
    }

    void drawRegion(juce::Graphics& g,const String& key,const String& value,bool lcd=false) const {
        const auto info=regions[juce::Identifier(key)];
        if (!info.isObject()) return;
        const auto rect=rectangle(info["rect"]);
        const auto style=info["style"];
        const auto colour=cssColour(style["colour"].toString(),juce::Colour(0xff84ddf8));
        const auto size=std::max(9.f,float(style["fontSize"]));
        const auto align=style["align"].toString()=="right"?juce::Justification::centredRight:juce::Justification::centredLeft;
        const String glow=lcd?"rgba(132, 221, 248, 0.76) 0px 0px 6px, rgba(132, 221, 248, 0.40) 0px 0px 14px":"";
        cachedText(g,key,value,rect,rect,colour,size,int(style["weight"]),align,glow,lcd,captionOpacity(rect));
    }

    void paintSetupDividers(juce::Graphics& g) const {
        // Cover the two baked dividers with the nearby plate texture.
        const auto cover=[&](int x,int h){
            g.drawImage(backdrop,x,179,7,h,(x+8)*backdropScale,179*backdropScale,7*backdropScale,h*backdropScale);
        };
        cover(512,405);
        cover(987,602);
        g.setColour(juce::Colour(0x397a8194));
        g.fillRect(515.5f,180.f,1.f,590.f);

        g.setColour(juce::Colour(0xf0171d26));
        g.fillRect(0,910,width,1);
        g.setColour(juce::Colour(0x666b7784));
        g.fillRect(0,911,width,1);
    }

    void paintSetup(juce::Graphics& g) const {
        constexpr float cardWidth=414.f;
        const auto title=juce::Colour(0xff253c57),body=juce::Colour(0xff4a5667);
#if JUCE_WINDOWS
        const auto graphicsStatus=view.rendererStatus();
        if (graphicsStatus.isNotEmpty())
            text(g,graphicsStatus,{setupSynthX+215.f,740.f,199.f,24.f},body,12.f,juce::Justification::centredLeft);
#endif
        const auto line=[&](const String& content,float y,juce::Colour colour,float size=13.f){
            text(g,content,{setupDataX,y,cardWidth,20.f},colour,size,juce::Justification::centredLeft);
        };
        line("Silver-K folder",293.f,title);
        line(processor.dataFolder.getFullPathName(),316.f,body);
        line("ROMs",392.f,title);
        line("K-Station 2.1.01 and KS 2.1.02 firmware (BIN, SYX or ZIP)",413.f,body);
        line("KS factory Data MIDI with programs and performances.",434.f,body);
        line("KS Wave ROM. A zero ROM is created if needed.",455.f,body);
        line("Patches   Presets and banks, including ZIP archives",486.f,title);
        line("Skins        Custom skins",512.f,title);
        line("Click Rescan after adding or replacing files.",540.f,body);
        if(processor.isFirmwareLoading())
            line("Loading firmware...",618.f,body);
        else {
            const auto lines=juce::StringArray::fromLines(processor.dataFolderStatus());
            for(int i=0;i<lines.size();++i)line(lines[i],618.f+20.f*float(i),body);
        }
        const auto status=setupStatus();
        if (status.isEmpty()) return;
        g.setColour(body);
        g.setFont(font(13));
        g.drawFittedText(status,Rect(setupDataX,687.f,cardWidth,72.f).getSmallestIntegerContainer(),juce::Justification::topLeft,3);
    }

    String setupStatus() const {
        return statusMessage.isNotEmpty()?statusMessage:host.nativeStatus?host.nativeStatus():processor.status();
    }

    int interfaceZoomPercent() const { return juce::roundToInt(100*processor.editorScale.load()); }

    juce::StringArray setupValues() const {
        if (processor.isFirmwareLoading()) return {"Loading firmware..."};
        juce::StringArray values{processor.dataFolder.getFullPathName(),processor.dataFolderStatus(),setupStatus(),
                                 String(processor.ksTwoCoreProcessing()?1:0)};
        for (const auto& w:widgets)
            if (w.picker() && w.name!="Interface size") values.add(menuValue(w));
        return values;
    }

    void drawFirmwareName(juce::Graphics& g) const {
        const auto region=regions["firmwareName"];
        if (!region.isObject()) return;
        const auto rect=rectangle(region["rect"]);
        const auto textBounds=rect.withWidth(650);
        if (!g.clipRegionIntersects(rect.getUnion(textBounds).getSmallestIntegerContainer())) return;
        String value=processor.firmwareReady()?"Loaded":"Not loaded";
        if (processor.firmwareReady())
            for (const auto& firmware:processor.firmwares)
                if (firmware.file==processor.osFile) {value=firmware.file.getFileName();break;}
        const auto style=region["style"];
        const auto ink=cssColour(style["colour"].toString(),juce::Colour(0xffcfd3e0));
        cachedText(g,"firmwareName",value,textBounds,textBounds,ink,float(style["fontSize"]),
                   int(style["weight"]),juce::Justification::centredLeft);
    }

    static juce::Rectangle<int> faderRail(const Widget& w) {
        auto track=w.bounds.getSmallestIntegerContainer();
        if (!w.thumb.isEmpty()) {
            // The authored vertical track places the thumb centres just inside the ends.
            track.setVerticalRange({juce::roundToInt(w.trackRect.getY()+.5f),
                                    juce::roundToInt(w.trackRect.getBottom()-3.5f)});
        } else {
            track.setHorizontalRange({juce::roundToInt(w.bounds.getX()+8.f),
                                      juce::roundToInt(w.bounds.getRight()-8.f)});
        }
        return track;
    }

    static Rect faderReadout(const Widget& w) {
        return w.bounds.withY(w.bounds.getY()-26.75f).withHeight(14.5f);
    }

    static Rect faderThumbBounds(const Widget& w,float centre) {
        if (!w.trackArt.isEmpty() && !w.thumbArt.isEmpty())
            return w.thumbRect.withY(centre-w.thumbRect.getHeight()*.5f);
        return {centre-8.f,w.bounds.getCentreY()-5.5f,16.f,11.f};
    }

    static Rect faderThumbHitBounds(const Widget& w,float centre) {
        return faderThumbBounds(w,centre).expanded(3.f);
    }

    juce::Rectangle<int> faderBounds(const Widget& w) const {
        const auto rail=faderRail(w);
        const auto vertical=!w.thumb.isEmpty();
        const auto first=float(vertical?rail.getY():rail.getX());
        const auto last=float(vertical?rail.getBottom():rail.getRight());
        auto area=w.bounds.getUnion(faderThumbHitBounds(w,first)).getUnion(faderThumbHitBounds(w,last));
        if (!w.trackArt.isEmpty() && !w.thumbArt.isEmpty()) {
            area=area.getUnion(skin.spriteBounds(w.trackArt,w.trackRect.getPosition()))
                     .getUnion(skin.spriteBounds(w.thumbArt,faderThumbBounds(w,first).getPosition()))
                     .getUnion(skin.spriteBounds(w.thumbArt,faderThumbBounds(w,last).getPosition()));
        }
        return area.expanded(1.f).getSmallestIntegerContainer();
    }

    void drawRange(juce::Graphics& g,const Widget& w,int value,float opacity) const {
        float thumb;
        if (w.fader) thumb=w.fader->thumbPosition();
        else {
            const auto& s=spec(w);
            const auto min=int(s["min"]),max=int(s["max"]);
            if (max<=min) return;
            const auto rail=faderRail(w).toFloat();
            const auto fraction=float(juce::jlimit(min,max,value)-min)/float(max-min);
            if (w.thumb.isEmpty()) thumb=juce::jmap(fraction,rail.getX(),rail.getRight());
            else thumb=juce::jmap(fraction,rail.getBottom(),rail.getY());
        }
        if (!w.trackArt.isEmpty() && !w.thumbArt.isEmpty()) {
            skin.sprite(g,w.trackArt,w.trackRect.getPosition(),opacity);
            skin.sprite(g,w.thumbArt,faderThumbBounds(w,thumb).getPosition(),opacity);
        } else {
            const auto track=w.bounds.withSizeKeepingCentre(w.bounds.getWidth(),9.f);
            g.setColour(juce::Colour(0xffe0e0e2).withMultipliedAlpha(opacity));
            g.fillRoundedRectangle(track,4.5f);
            const auto filled=track.withRight(thumb);
            g.setColour(juce::Colour(0xffb6b6b6).withMultipliedAlpha(opacity));
            g.fillRoundedRectangle(filled,4.5f);
            g.setColour(juce::Colours::white.withMultipliedAlpha(opacity));
            g.drawRoundedRectangle(track.reduced(.5f),4.f,1.f);
            g.fillRoundedRectangle(faderThumbBounds(w,thumb),5.5f);
        }
    }

    static bool pageChoice(const Widget& w) {
        return std::find(pageNames.begin(),pageNames.end(),w.name)!=pageNames.end();
    }

    bool active(const Widget& w) const {
        if (w.name==drumVoiceEditCaption) return editingDrumVoice;
        if (w.name=="Arpeggiator options") return sections.arp;
        if (w.name=="Voices options") return sections.voices;
        if (w.name==chorusOptionsCaption) return sections.chorusOptions;
        if (w.id.isNotEmpty()) return w.option>=0?raw(w)==w.option:raw(w)!=0;
        if (w.role=="tab") return w.name==effectNames[size_t(sections.effect)];
        if (pageChoice(w)) return w.name==(model=="K" && selectedPage=="Perf"?String("Main"):selectedPage);
        if (w.group.contains("PWM source")) return pwm[size_t(oscillatorFor(w))]==w.option;
        if (w.group.startsWith("Oscillator") && w.group.contains("knob mode")) return pitch[size_t(oscillatorFor(w))]==w.option;
        if (w.group=="Filter knob mode") return filterMode==(w.name=="keytrack"?2:w.option);
        if (w.group=="Mixer knob mode") return mixerMode==w.option;
        if (w.performanceToggle()) return processor.performanceMode();
        if (w.name=="Show all bank slots") return bool(browser["allSlots"]);
        if (w.name=="Show init") return bool(browser["showInit"]);
        if (w.name=="Show only favourites") return bool(browser["favouritesOnly"]);
        if (w.partToggle()) return (lastMask&(1u<<(w.part-1)))!=0;
        if (w.twoCoreToggle()) return processor.ksTwoCoreProcessing();
        if (w.partTab()) return unsigned(w.name.getIntValue())==processor.currentPart()+1;
        return w.selected;
    }

    String choiceGroupKey(const Widget& w) const {
        if (w.id.isNotEmpty()) {
            if (w.option>=0 && (w.led() || w.visualType=="small-button"))
                return "parameter:"+String(partFor(w))+":"+w.id;
            return {};
        }
        if (w.role=="tab") return "effects";
        if (pageChoice(w)) return "pages";
        if (w.partTab()) return "parts";
        if (w.group.contains("PWM source") || (w.group.startsWith("Oscillator") && w.group.contains("knob mode")) ||
            w.group=="Filter knob mode" || w.group=="Mixer knob mode")
            return w.group;
        return {};
    }

    bool choiceIsShown(const Widget& w) const {
        return choiceGroups[size_t(w.choiceGroup)].shown()==&w;
    }

    void repaintChoiceChange(const Widget* oldChoice,const Widget* newChoice,juce::Rectangle<int> dirty={}) {
        if (oldChoice!=newChoice) {
            if (oldChoice) dirty=dirty.getUnion(oldChoice->paintBounds);
            if (newChoice) dirty=dirty.getUnion(newChoice->paintBounds);
        }
        if (!dirty.isEmpty()) repaint(dirty);
    }

    void refreshChoiceGroup(ChoiceGroup& group,bool redraw=true) {
        const auto* oldChoice=group.shown();
        const auto& first=*group.members.front();
        const auto value=first.id.isNotEmpty()?std::optional<int>(raw(first)):std::nullopt;
        const auto selected=std::find_if(group.members.begin(),group.members.end(),[&](const Widget* w) {
            return value?w->option==*value:active(*w);
        });
        group.selected=selected==group.members.end()?nullptr:*selected;
        if (redraw) repaintChoiceChange(oldChoice,group.shown());
    }

    void buttonVisualStateChanged(Widget& w,juce::Rectangle<int> area) {
        if (w.choiceGroup<0) {
            repaint(area);
            return;
        }
        auto& group=choiceGroups[size_t(w.choiceGroup)];
        const auto* previous=group.shown();
        const bool down=w.hitButton->isEnabled() && w.hitButton->showsPress();
        if (down!=w.choiceDown) {
            w.choiceDown=down;
            if (down) group.pressed=&w;
            else if (group.pressed==&w) group.pressed=nullptr;
        }
        repaintChoiceChange(previous,group.shown(),area);
    }

    bool partDimmed(unsigned part) const {
        return part && (!lastPerformance || !(lastMask&(1u<<(part-1))));
    }

    float captionOpacity(Rect bounds) const {
        if (page=="Perf")
            for (unsigned part=1;part<=4;++part)
                if (partDimmed(part) && bounds.getY()>=214.f &&
                    performanceAreas[part-1].contains(bounds.getCentre()))
                    return performanceOpacity;
        return 1.f;
    }

    bool effectLit(const String& tab) const {
        const auto isLit=[this](const String& id,bool bipolar) {
            Widget target(juce::var{});
            target.id=id;
            const auto& range=spec(target);
            return range.isObject() && raw(target)+(bipolar?int(range["offset"]):0)!=0;
        };
        if(tab=="Delay") return isLit("delay",false);
        if(tab=="Reverb") return isLit("reverb",false);
        if(tab=="Chorus") return isLit("chorus",false);
        if(tab=="Dist") return isLit("distortion_level",true);
        if(tab=="EQ") return isLit("eq_level",true)||isLit("eq_depth",true);
        if(tab=="Panning") return isLit("pan_position",true)||isLit("pan_depth",true);
        return isLit("vocoder_balance",true);
    }

    static juce::AffineTransform widgetArtTransform(const Widget& w) {
        return juce::AffineTransform::scale(1.f,w.artScaleY,w.artAnchor.x,w.bounds.getCentreY());
    }

    juce::Rectangle<int> mainWidgetPaintBounds(const Widget& w) const {
        if (w.standardButton()) return w.buttonBounds();
        auto bounds=w.bounds.getUnion(w.caption);
        const auto transform=widgetArtTransform(w);
        const auto includeArt=[&](const String& state){
            bounds=bounds.getUnion(skin.spriteBounds(state,w.artAnchor).transformedBy(transform));
        };
        includeArt(w.art);
        if (w.gear()) {
            includeArt(skin.style("gear","off"));
            includeArt(skin.style("gear","on"));
        }
        // Include the artwork for each state, including any glow outside the hit area.
        if (!w.lampArt.isEmpty()) bounds=bounds.getUnion(soultrain::SkinArt::indicatorBounds(w.lampArt));
        if (w.led() && !w.lampArt.isEmpty()) {
            const auto height=juce::roundToInt(w.bounds.getHeight());
            includeArt(skin.lamp(height,false));
            includeArt(skin.lamp(height,true));
        }
        return bounds.expanded(2.f).getSmallestIntegerContainer();
    }

    void paintWidgetArt(juce::Graphics& g,const Widget& w,const String& art,float opacity,float widthChange=0.f) const {
        if (w.artScaleY==1.f) {
            skin.sprite(g,art,w.artAnchor,opacity,widthChange);
            return;
        }
        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(widgetArtTransform(w));
        skin.sprite(g,art,w.artAnchor,opacity,widthChange);
    }

    soultrain::ButtonHighlight buttonHighlight(const Widget& w) const {
        const auto* button=w.hitButton;
        if (!button || !button->isEnabled()) return soultrain::ButtonHighlight::none;
        if (w.choiceGroup>=0)
            return soultrain::buttonHighlightFor(choiceIsShown(w),button->showsRollover(),false);
        const bool toggle=w.visualType=="header-button" || w.visualType=="small-button" ||
                          w.gear() || w.name=="Show only favourites";
        return soultrain::buttonHighlightFor(toggle && active(w),button->showsRollover(),
                                            button->showsPress() || (button->menuOpen() && !w.picker()));
    }

    void paintButtonCaption(juce::Graphics& g,const soultrain::Caption& authored,ButtonCaption& caption) const {
        const auto& w=*caption.widget;
        const auto highlight=buttonHighlight(w);
        auto& styled=caption.states[size_t(highlight)];
        if (!styled) {
            const String shadow=highlight==soultrain::ButtonHighlight::pressed?soultrain::pressedButtonTextShadow:"";
            styled=std::make_unique<soultrain::Caption>(authored.restyled(soultrain::buttonTextColour(highlight),shadow));
        }
        const auto opacity=captionOpacity(w.bounds)*(w.hitButton && !w.hitButton->isEnabled()?.45f:1.f);
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(w.buttonBounds().reduced(3));
        styled->draw(g,0,opacity);
    }

    void paintStandardWidget(juce::Graphics& g,const Widget& w,float opacity) {
        const auto bounds=w.buttonBounds();
        if (!g.clipRegionIntersects(bounds)) return;
        const auto highlight=buttonHighlight(w);
        soultrain::paintStandardButton(g,bounds,highlight,opacity);
        juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(bounds.reduced(3));
        if (w.role=="tab") {
            const auto lit=effectLit(w.name);
            if (!skin.paintIndicator(g,w.lampArt,lit,false,opacity))
                skin.sprite(g,skin.lamp(15,lit),w.lampArt.getPosition().translated(0.f,-1.5f),opacity);
        } else if (w.visualType=="preset") {
            const auto c=bounds.toFloat().getCentre();
            const auto dy=w.name=="Previous preset"?-3.f:3.f;
            juce::Path arrow;
            arrow.addTriangle(c.x,c.y+dy,c.x-5.f,c.y-dy,c.x+5.f,c.y-dy);
            g.setColour(soultrain::buttonTextColour(highlight).withMultipliedAlpha(opacity));
            g.fillPath(arrow);
        }
        const auto alpha=opacity*(w.hitButton && !w.hitButton->isEnabled()?.45f:1.f);
        if (w.name==soultrain::DrumSequencer::EditName || w.name==drumVoiceEditCaption) {
            paintActionCaption(g,w,w.baseline,w.fontSize,w.fontWeight,highlight,alpha);
        } else if (page=="Setup" && (w.name==String::fromUTF8("Choose Silver-K folder…") || w.name=="Rescan folders" ||
                                    w.name=="Panic" || w.name=="Skins folder" || w.name=="Export template")) {
            paintActionCaption(g,w,w.name,12.f,400,highlight,alpha);
        } else if (page=="Browser" && browserActionCaption(w.name)) {
            const auto& label=w.name==exportSysExCaption && selectedPatchIsKSPlus?exportSilverKCaption:w.name;
            paintActionCaption(g,w,label,w.fontSize,400,highlight,alpha);
        } else if (w.name=="Show only favourites") {
            const auto label=active(w)?String::fromUTF8("★  Favourites"):String::fromUTF8("☆  Favourites");
            paintActionCaption(g,w,label,13.f,400,highlight,alpha,juce::Justification::centredLeft);
        }
    }

    void paintWidget(juce::Graphics& g,const Widget& w) {
        if (w.dial() || !w.waveRect.isEmpty() || w.sortHeader()) return;
        if (w.range() && !g.clipRegionIntersects(w.paintBounds)) return;
        const bool drumAction=w.name==soultrain::DrumSequencer::EditName || w.name==drumVoiceEditCaption;
        if (drumAction && (!w.hitButton || !w.hitButton->isVisible())) return;
        const auto opacity=captionOpacity(w.bounds);
        if (w.range()) {
            const auto value=w.fader?juce::roundToInt(w.fader->getValue()):raw(w);
            drawRange(g,w,value,opacity);
            if(page=="Perf" && w.part) {
                const auto box=faderReadout(w);
                if (g.clipRegionIntersects(box.getSmallestIntegerContainer()))
                    cachedText(g,"volume:"+String(w.part),String(value),box,box,juce::Colour(0xff101011),
                               12.f,400,juce::Justification::centredRight,{},false,opacity);
            }
            return;
        }

        if (w.gear()) {
            const auto highlight=buttonHighlight(w);
            const auto art=skin.style("gear",highlight==soultrain::ButtonHighlight::pressed?"on":"off");
            paintWidgetArt(g,w,art.isNotEmpty()?art:w.art,opacity);
            if (highlight==soultrain::ButtonHighlight::hover) {
                juce::Graphics::ScopedSaveState save(g);
                g.reduceClipRegion(w.buttonBounds());
                soultrain::paintButtonHighlight(g,w.bounds,highlight,opacity);
            }
            return;
        }

        if (w.standardButton()) {
            paintStandardWidget(g,w,opacity);
            return;
        }

        auto artwork=w.art;
        if (w.name=="Selected patch tags" && tagFieldOffset!=0) {
            juce::Graphics::ScopedSaveState save(g);
            const auto x=w.bounds.getX();
            const auto scale=1.f-tagFieldOffset/w.bounds.getWidth();
            g.addTransform(juce::AffineTransform::translation(-x,0).scaled(scale,1.f)
                               .translated(x+tagFieldOffset,0));
            skin.sprite(g,artwork,w.artAnchor,opacity);
            return;
        }
        const auto* button=w.hitButton;
        if (w.led() && !w.lampArt.isEmpty()) {
            // The header follows the applied mode. A part shows the accepted click
            // until the engine publishes the changed mask.
            const auto* preview=w.performanceToggle()?nullptr:button;
            const auto lit=w.choiceGroup>=0?choiceIsShown(w):
                active(w)!=(!w.partToggle() && !w.twoCoreToggle() && preview && preview->isEnabled() && preview->showsPress());
            if (skin.paintIndicator(g,w.lampArt,lit,preview && preview->showsRollover(),opacity)) artwork.clear();
            else if (const auto replacement=skin.lamp(juce::roundToInt(w.bounds.getHeight()),lit);replacement.isNotEmpty())
                artwork=replacement;
        }
        if (w.choiceControl()) {
            const auto state=button?button->choiceState():soultrain::ChoiceState::normal;
            if (page=="Setup" && !w.caption.isEmpty() && w.name!="Firmware")
                text(g,w.name,w.caption,juce::Colour(0xff253c57),12.f,juce::Justification::centredLeft);
            paintWidgetArt(g,w,artwork,opacity,w.bounds.getWidth()-w.artWidth);
            const Rect box=w.bounds.reduced(5.f,0.f);
            auto& caption=choiceCaptions[instance(w)];
            caption.set(skin.choiceFont,menuValue(w),box.getWidth());
            caption.draw(g,box,juce::Justification::centred,soultrain::choiceInk,opacity,
                         soultrain::choiceGlow(state));
            soultrain::paintChoiceFrame(g,w.buttonBounds().toFloat(),state,opacity);
        } else {
            paintWidgetArt(g,w,artwork,opacity);
            if (w.name=="Show init")
                text(g,w.name,w.caption,w.textColour,w.fontSize,juce::Justification::centredLeft);
            if ((w.name=="Show all bank slots" || w.name=="Show init") && active(w)) {
                const auto box=w.bounds.reduced(2.f);
                juce::Path tick;
                tick.startNewSubPath(box.getX(),box.getCentreY());
                tick.lineTo(box.getX()+3.f,box.getBottom());
                tick.lineTo(box.getRight(),box.getY());
                g.setColour(juce::Colour(0xff253c57));
                g.strokePath(tick,juce::PathStrokeType(1.35f));
            }
        }
    }

    void paintActionCaption(juce::Graphics& g,const Widget& w,const String& label,float size,int weight,
                            soultrain::ButtonHighlight state,float opacity,
                            juce::Justification align=juce::Justification::centred) const {
        const auto bounds=w.buttonBounds().reduced(3).toFloat();
        const String shadow=state==soultrain::ButtonHighlight::pressed?soultrain::pressedButtonTextShadow:"";
        const auto textBounds=align==juce::Justification::centredLeft?bounds.reduced(5.f,0.f):bounds;
        cachedText(g,instance(w)+":"+String(int(state)),label,textBounds,bounds,soultrain::buttonTextColour(state),
                   size,weight,align,shadow,false,opacity);
    }

    String instance(const Widget& w) const {
        return w.name+":"+String(w.part)+":"+String(w.bounds.getX(),2)+":"+String(w.bounds.getY(),2);
    }

    static String patchLabel(const juce::var& row) {
        const auto bank=makeText(row,"bank");
        return (bank.isEmpty()?String{}:bank+" / ")+
               (makeText(row,"number")+" "+makeText(row,"name")).trim();
    }

    MenuItems options(const Widget& w) const {
        MenuItems result;
        const auto addStrings=[&](const auto& values,int first){
            int index=first;
            for (const String& label:values) result.emplace_back(index++,label);
        };
        if (w.id.isNotEmpty()) {
            const auto s=spec(w);
            auto choices=s["choices"];
            const auto min=int(s["min"]);
            const auto max=w.id=="arp_rhythm" && drumRhythm()?Processor::UserDrumRhythm:int(s["max"]);
            for (int n=min;n<=max && n-min<256;++n) result.emplace_back(n,valueLabel(w,n));
        } else if (w.name.containsIgnoreCase(" preset") && w.part) {
            if (const auto* rows=browser["rows"].getArray())
                for (const auto& row:*rows) if (bool(row["program"]))
                    result.emplace_back(row["id"],patchLabel(row));
        } else if (w.name=="Firmware" || w.name=="Select firmware") {
            int index=0;
            for (const auto& item:processor.firmwares)
                result.emplace_back(index++,String(item.kStation?String::fromUTF8("K · "):String::fromUTF8("KS · "))+item.file.getFileName());
        } else if (w.name=="Setup model") {
            result={{0,"KS"},{1,"K-Station"}};
        } else if (w.name=="Clock source") {
            addStrings(juce::StringArray{"Host","Internal","MIDI"},0);
        } else if (w.name==firmwareSwitchCaption) {
            addStrings(Processor::firmwareSwitchLabels,0);
        } else if (w.name=="Audio buffer profile") {
            addStrings(juce::StringArray{"Low latency","Balanced","Safe","Maximum reserve"},0);
#if JUCE_WINDOWS
        } else if (w.name==rendererCaption) {
            result={{0,"Software"},{1,"OpenGL"}};
#endif
        } else if (w.name=="Skin") {
            const auto menu=host.skinMenu?host.skinMenu():juce::var{};
            if(const auto* items=menu["choices"].getArray())
                for(const auto& item:*items)result.emplace_back(item["value"],item["label"].toString());
        } else if (w.name=="Interface size") {
            for (int i:{50,67,75,85,100,125,150}) result.emplace_back(i,String(i)+"%");
        } else {
            const auto key=w.name=="Instrument"?"models":w.name=="Source"?"sources":w.name=="KS bank"?"banks":"";
            if (const auto* items=browser[key].getArray())
                for (const auto& item:*items) result.emplace_back(item["value"],item["label"].toString());
        }
        return result;
    }

    String menuValue(const Widget& w) const {
        if (w.id.isNotEmpty()) {
            const auto current=raw(w);
            if(w.id=="arp_rhythm")return valueLabel(w,current);
            if(w.id=="vocoder_sibilance_type" && current==0)return "HP";
            if (current==w.value.getIntValue() && binding(w)==w.id) return w.baseline;
            auto value=valueLabel(w,current);
            if(w.id=="osc_key_phase")
                value=value=="Free running"?"Free run":value.replace(" degrees",String::fromUTF8("°"));
            else if(w.id.endsWith("_sync"))
                value=value.replace(" dotted","d").replace(" triplet","t");
            return value;
        }
        if (w.part && w.name.containsIgnoreCase(" preset")) {
            const auto* programs=browser["partPrograms"].getArray();
            if (!programs || !juce::isPositiveAndBelow(int(w.part-1),programs->size())) return w.baseline;
            const auto id=(*programs)[int(w.part-1)].toString();
            if (const auto* rows=browser["rows"].getArray())
                for (const auto& item:*rows)
                    if (item["id"].toString()==id && item["name"].toString().substring(0,16)==lastPartNames[w.part-1])
                        return patchLabel(item);
            return w.baseline;
        }
        if (w.name=="Firmware" || w.name=="Select firmware") {
            if (processor.isFirmwareLoading()) return w.baseline;
            for (const auto& item:processor.firmwares)
                if (item.file==processor.osFile)
                    return String(item.kStation?String::fromUTF8("K · "):String::fromUTF8("KS · "))+item.file.getFileName();
            return w.baseline;
        }
        if (w.name=="Setup model") return processor.isKStation()?"K-Station":"KS";
        if (w.name=="Clock source") return juce::StringArray{"Host","Internal","MIDI"}[int(processor.clockSource())];
#if JUCE_WINDOWS
        if (w.name==rendererCaption) return view.rendererLabel();
#endif
        if (w.name==firmwareSwitchCaption) return Processor::firmwareSwitchLabels[size_t(processor.firmwareSwitch())];
        if (w.name=="Audio buffer profile")
            return juce::StringArray{"Low latency","Balanced","Safe","Maximum reserve"}[processor.bufferProfile()];
        if (w.name=="Skin") return host.skinMenu?host.skinMenu()["label"].toString():w.baseline;
        if (w.name=="Interface size") return String(interfaceZoomPercent())+"%";
        const auto key=w.name=="Instrument"?"model":w.name=="Source"?"source":"bank";
        const auto items=w.name=="Instrument"?"models":w.name=="Source"?"sources":"banks";
        if (const auto* entries=browser[items].getArray())
            for (const auto& item:*entries)
                if (item["value"]==browser[key]) return item["label"].toString();
        return w.baseline;
    }

    void native(const String& action) { if (host.nativeAction) host.nativeAction(action); }
    void send(juce::var data) { if (host.browserAction) host.browserAction(data); }
    void setBrowser(const String& action,const juce::var& value) {
        send(message({{"action",action},{"value",value}}));
        refreshBrowserSoon();
    }

    void refreshBrowserSoon() {
        if (page!="Browser" || browserRefreshQueued) return;
        browserRefreshQueued=true;
        const juce::Component::SafePointer<Canvas> self(this);
        juce::MessageManager::callAsync([self]{
            if (!self) return;
            self->browserRefreshQueued=false;
            self->updateBrowser();
        });
    }

    void menuSelected(const Widget& w,const juce::var& value) {
        if (!available(w)) return;
        if (w.id=="arp_rhythm") {
            String error;
            const auto rhythm=uint8_t(int(value));
            if(!processor.selectDrumRhythm(processor.drumEditorSlot(),rhythm,error)) {
                statusMessage=std::move(error);
                return;
            }
            refreshDrumControls();
            repaint(w.bounds.getSmallestIntegerContainer());
            refreshLcd();
            return;
        }
        if (w.id.isNotEmpty()) {
            setParameter(w,int(value));
            if (w.id=="part_low" || w.id=="part_high") {
                auto other=w;
                other.id=w.id=="part_low"?"part_high":"part_low";
                if ((w.id=="part_low" && int(value)>raw(other)) || (w.id=="part_high" && int(value)<raw(other)))
                    setParameter(other,int(value));
            }
        }
        else if (w.part && w.name.containsIgnoreCase(" preset"))
            send(message({{"action","loadPart"},{"part",int(w.part)},{"id",value}}));
        else if (w.name=="Firmware" || w.name=="Select firmware") native("firmware:"+value.toString());
        else if (w.name=="Setup model") native(int(value)?"model:K":"model:KS");
        else if (w.name=="Clock source") processor.setClockSource(ClockSource(int(value)));
#if JUCE_WINDOWS
        else if (w.name==rendererCaption) {
            const bool enabled=int(value)!=0;
            String error;
            if (processor.setOpenGLRendererPreference(enabled,error)) {
                statusMessage.clear();
                view.useOpenGL(enabled);
            } else {
                statusMessage=std::move(error);
                refreshLcd();
            }
        }
#endif
        else if (w.name==firmwareSwitchCaption) processor.setFirmwareSwitch(Processor::FirmwareSwitch(int(value)));
        else if (w.name=="Audio buffer profile") processor.setBufferProfile(int(value));
        else if (w.name=="Skin") native("skin:"+value.toString());
        else if (w.name=="Interface size") native("scale:"+value.toString());
        else if (w.name=="Instrument") setBrowser("model",value);
        else if (w.name=="Source") setBrowser("source",value);
        else if (w.name=="KS bank") setBrowser("bank",value);
        repaint(w.buttonBounds().expanded(4));
    }

    void openMenu(const Widget& w,juce::Component& target) {
        if (w.id=="part_low" || w.id=="part_high") {
            presentNotePicker(target,raw(w),false,[this,w](int note){
                if (available(w)) menuSelected(w,note);
            });
            pickerPart=w.part;
            return;
        }
        auto values=options(w);
        if (values.empty()) return;
        juce::StringArray labels;
        for (const auto& item:values) labels.add(item.second);
        int current=-1;
        if (w.id.isNotEmpty()) {
            const auto value=raw(w);
            for(size_t i=0;i<values.size();++i)
                if(int(values[i].first)==value) {current=int(i);break;}
        } else current=labels.indexOf(menuValue(w));
        const bool scroll=w.name.containsIgnoreCase(" preset") || w.name=="Firmware" || w.name=="Select firmware"
            || (page=="Browser" && values.size()>32)
            || (w.name=="Skin" && values.size()>10);
        presentPicker(target,std::move(labels),current,scroll,
            [this,w,values=std::move(values)](int result) {
                if (size_t(result)<values.size() && available(w))
                    menuSelected(w,values[size_t(result)].first);
            });
        pickerPart=w.part;
    }

    void presentNotePicker(juce::Component& target,int current,bool allowClear,std::function<void(int)> apply) {
        juce::StringArray labels;
        for (int note=0;note<128;++note) labels.add(midiNoteLabel(note));
        // Keep the row index equal to the MIDI note in both menus.
        const auto clearRow=labels.size();
        if (allowClear) labels.add("Clear range");
        presentPicker(target,std::move(labels),current<0 && allowClear?clearRow:current,false,
            [clearRow,apply=std::move(apply)](int row){
                if (row>=0) apply(row==clearRow?-1:row);
            });
        picker->setDescription("Play a MIDI note or choose one from the menu");
        noteLearnToken=processor.beginNoteLearn();
    }

    void presentPicker(juce::Component& target,juce::StringArray labels,int current,bool scroll,
                       std::function<void(int)> apply,Rect anchor={}) {
        if (anchor.isEmpty()) anchor=getLocalArea(&target,target.getLocalBounds()).toFloat();
        const juce::Component::SafePointer<Canvas> self(this);
        const juce::Component::SafePointer<juce::Component> trigger(&target);
        closeParameterEntry();
        closePicker();
        picker=std::make_unique<ChoicePicker>(anchor,std::move(labels),current,scroll,skin.choiceFont,
            [self,trigger,apply=std::move(apply)](int result) {
                if (!self) return;
                self->cancelNoteLearn();
                const juce::Component::SafePointer<ChoicePicker> selected(self->picker.get());
                juce::MessageManager::callAsync([self,trigger,selected,apply,result] {
                    if (!self || !selected || self->picker.get()!=selected.getComponent()) return;
                    self->closePicker();
                    if (trigger && trigger->isShowing() && trigger->getPeer()) trigger->grabKeyboardFocus();
                    apply(result);
                });
            });
        showPicker(target);
    }

    void showPicker(juce::Component& trigger) {
        pickerTrigger=&trigger;
        trigger.getProperties().set(soultrain::choiceMenuOpenProperty,true);
        trigger.repaint();
        picker->fitToMenu();
        pickerLayer.setInterceptsMouseClicks(true,true);
        pickerLayer.addAndMakeVisible(*picker);
        picker->focus();
    }

    void cancelNoteLearn() {
        if (noteLearnToken) processor.cancelNoteLearn(noteLearnToken);
        noteLearnToken=0;
    }

    void closePicker() {
        cancelNoteLearn();
        if (pickerTrigger) {
            pickerTrigger->getProperties().remove(soultrain::choiceMenuOpenProperty);
            pickerTrigger->repaint();
        }
        pickerTrigger=nullptr;
        if (wavePicker) wavePicker->setMenuOpen(false);
        wavePicker=nullptr;
        pickerLayer.setInterceptsMouseClicks(false,true);
        if (picker) {
            const auto area=getLocalArea(picker.get(),picker->getLocalBounds());
            picker.reset();
            repaint(area);
        }
        pickerPart=0;
    }

    void toggleDrumSequencer() {
        closeParameterEntry();
        if (drumSequencer) {
            closePicker();
            drumSequencer.reset();
            refreshPaintedValues();
            refreshLcd();
            repaint();
            return;
        }
        processor.setDrumEdit(false);
        refreshDrumControls();
        const juce::Component::SafePointer<Canvas> self(this);
        drumSequencer=std::make_unique<soultrain::DrumSequencer>(processor,skin,
            [this](juce::Component& trigger,juce::StringArray contents,int current,bool scroll,std::function<void(int)> finish) {
                presentPicker(trigger,std::move(contents),current,scroll,std::move(finish));
            },
            [this](juce::Component& trigger,int current,std::function<void(int)> finish) {
                presentNotePicker(trigger,current,true,std::move(finish));
            },
            [self]{
                juce::MessageManager::callAsync([self]{
                    if(self && self->drumSequencer) self->toggleDrumSequencer();
                });
            });
        drumSequencer->setBounds(getLocalBounds());
        addAndMakeVisible(*drumSequencer);
        drumSequencer->focusClose();
        repaint();
    }

    void refreshDrumControls() {
        const auto showButton=[this](HitButton* button,bool visible,bool selected) {
            if (!button) return;
            if (button->isVisible()!=visible || button->getToggleState()!=selected) {
                button->setVisible(visible);
                button->setToggleState(selected,juce::dontSendNotification);
                repaint(button->getBounds().expanded(8));
            }
        };
        showButton(sequencerEditButton,seqSelected(),false);
        bool editing=processor.drumEditEnabled();
        auto voice=drumVoiceEditButton || editing ? processor.drumEditInfo() : Processor::DrumEditInfo{};
        if (editing && !voice.bank) {
            processor.setDrumEdit(false);
            editing=false;
        }
        const bool refresh=(editing!=editingDrumVoice) || (editing && !(voice==drumVoice));
        editingDrumVoice=editing;
        drumVoice=std::move(voice);
        showButton(drumVoiceEditButton,drumVoice.bank!=0,editing);
        if (editing && !isTimerRunning(DrumEditorTimer)) startTimer(DrumEditorTimer,16);
        if (!editing) stopTimer(DrumEditorTimer);
        if (refresh) {
            closeParameterEntry();
            adjustedParameter=nullptr;
            processor.refreshControls();
            refreshPaintedValues();
            refreshLcd();
        }
    }

    void refreshPerformanceSwitches() {
        const bool performance=processor.performanceMode();
        const auto mask=performance?processor.displayedActiveParts():uint8_t{0};
        const bool modeChanged=performance!=lastPerformance;
        const auto changed=modeChanged?15:lastMask^mask;
        if (changed==0) return;
        lastPerformance=performance;
        lastMask=mask;

        if (pickerPart && partDimmed(pickerPart)) closePicker();
        const auto affected=[changed](unsigned part){return part && (changed&(1u<<(part-1)));};
        for (const auto& w:widgets) {
            if (w.hitButton && (affected(w.part) || (modeChanged && w.partTab())))
                w.hitButton->setEnabled(available(w));
            if (modeChanged && (w.performanceToggle() || w.partTab()))
                repaint(w.bounds.expanded(16.f).getSmallestIntegerContainer());
        }
        for (const auto& slider:sliders) {
            const auto& widget=*slider.widget;
            if (!affected(widget.part)) continue;
            slider.control->setEnabled(available(widget));
            if (widget.dial()) slider.control->setAlpha(captionOpacity(widget.bounds));
        }
        for (size_t part=0;part<performanceAreas.size();++part)
            if (affected(unsigned(part+1))) repaint(performanceAreas[part].expanded(8.f,16.f).getSmallestIntegerContainer());
        refreshLcd();
    }

    void refreshPerformanceSelection() {
        const auto part=processor.currentPart();
        if (lastPart==part) return;
        lastPart=part;
        for (const auto& w:widgets)
            if (w.partTab()) repaint(w.buttonBounds());
        refreshLcd();
    }

    bool available(const Widget& w) const {
        const auto& name=w.name;
        if (name=="Undo" || name=="Redo") return false;
        if (w.twoCoreToggle()) return !processor.isKStation() && !processor.isFirmwareLoading();
        if (w.part) {
            if (!processor.firmwareReady() || processor.isFirmwareLoading() || !lastPerformance) return false;
            if (name=="Part "+String(w.part)) return true;
            if (!w.partToggle() && partDimmed(w.part)) return false;
        }
        if (w.partTab() && name!="1") return processor.performanceMode();
        if (name=="Select firmware") return !processor.firmwares.empty() && !processor.isFirmwareLoading();
        if (name=="Skin") return !processor.isFirmwareLoading();
        if (name=="Remove source") return browser["canRemoveSource"].isVoid()?int(browser["source"])>1:bool(browser["canRemoveSource"]);
        if (name=="Save tags" || name==exportSysExCaption || name=="Selected patch tags") return selectedPatch.isNotEmpty();
        if (w.performanceToggle()) return processor.firmwareReady() && !processor.isFirmwareLoading();
        if (usesEffects() && w.bounds.getY()>125) return processor.firmwareReady() && !processor.isFirmwareLoading();
        if (w.id.isNotEmpty()) return processor.firmwareReady() && !processor.isFirmwareLoading();
        if (page=="Setup" && w.disabled) return false;
        return true;
    }

    void activate(const Widget& w) {
        const auto& name=w.name;
        if (!available(w)) return;
        if (name==soultrain::DrumSequencer::EditName) { toggleDrumSequencer(); return; }
        if (name==drumVoiceEditCaption) { processor.setDrumEdit(!processor.drumEditEnabled());refreshDrumControls();return; }
        if (w.id.isNotEmpty()) {
            if (name.endsWithIgnoreCase(" waveform")) {
                const auto s=spec(w);
                const auto min=int(s["min"]), span=1+int(s["max"])-min;
                if (span>0) setParameter(w,min+(raw(w)-min+span+(name.contains("Previous")?-1:1))%span);
            } else setParameter(w,w.option>=0?w.option:!raw(w));
            return;
        }
        if (name=="Voices options" || name=="Arpeggiator options" || name==chorusOptionsCaption) {
            auto next=requestedSections.value_or(sections);
            if (name==chorusOptionsCaption) next.chorusOptions=!next.chorusOptions;
            else if (name=="Voices options") next.voices=!next.voices;
            else next.arp=!next.arp;
            requestedSections=next;
            rebuildSoon(true);
            return;
        }
        if (name=="Columns") { openColumns(w); return; }
        if (name==saveCurrentCaption && w.hitButton && processor.performanceMode()) {
            presentPicker(*w.hitButton,{String::fromUTF8("Save Performance…"),String::fromUTF8("Save selected part…")},
                          -1,false,[this](int option){
                if(option<0)return;
                send(message({{"action","saveCurrent"},{"value",option==0?"performance":"part"}}));
                refreshBrowserSoon();
            });
            return;
        }
        if (w.role=="tab") {
            const auto it=std::find(effectNames.begin(),effectNames.end(),name);
            if (it!=effectNames.end()) {
                auto next=requestedSections.value_or(sections);
                const auto effect=int(it-effectNames.begin());
                if (effect!=next.effect) {
                    next.effect=effect;
                    next.chorusOptions=false;
                    requestedSections=next;
                    rebuildSoon(true);
                }
            }
            return;
        }
        if (pageChoice(w)) {
            if (selectedPage!=name) {
                selectedPage=name;
                rebuildSoon();
            }
            return;
        }
        if (w.group.contains("PWM source")) { pwm[size_t(oscillatorFor(w))]=juce::jlimit(0,2,w.option);rebuildSoon();return; }
        if (w.group.startsWith("Oscillator") && w.group.contains("knob mode")) { pitch[size_t(oscillatorFor(w))]=juce::jlimit(0,2,w.option);rebuildSoon();return; }
        if (w.group=="Filter knob mode") { filterMode=name=="keytrack"?2:juce::jlimit(0,2,w.option);rebuildSoon();return; }
        if (w.group=="Mixer knob mode") { mixerMode=juce::jlimit(0,1,w.option);rebuildSoon();return; }
        juce::String error;
        if (w.partToggle()) {
            if (processor.setPartActive(w.part-1,!active(w),error)) refreshPerformanceSwitches();
            else statusMessage=std::move(error);
            return;
        }
        if (w.performanceToggle()) {
            if (!processor.setPerformanceMode(!processor.performanceMode(),error)) statusMessage=std::move(error);
            return;
        }
        if (w.twoCoreToggle()) {
            if (processor.setKSTwoCoreProcessing(!processor.ksTwoCoreProcessing(),error)) statusMessage.clear();
        } else if (name.startsWith("Edit sound") || name.startsWith("Part ") || w.partTab()) {
            processor.selectPart(w.part?w.part-1:unsigned(name.getIntValue()-1));
            if (name.startsWith("Edit sound")) {
                selectedPage="Main";
                auto next=requestedSections.value_or(sections);
                next.arp=next.voices=false;
                requestedSections=next;
            }
            if (page=="Perf" && selectedPage==page) refreshPerformanceSelection();
            else rebuildSoon();
            return;
        } else if (name=="Prev" || name.contains("Previous") || name.startsWith("Next")) {
            adjustedParameter=nullptr;
            const bool previous=name=="Prev" || name.contains("Previous");
            if (w.part) send(message({{"action","stepPart"},{"part",int(w.part)},{"value",previous?-1:1}}));
            else native(previous?"previousPreset":"nextPreset");
        } else if (name=="Show only favourites") setBrowser("favouritesOnly",!bool(browser["favouritesOnly"]));
        else if (name=="Show all bank slots") setBrowser("allSlots",!bool(browser["allSlots"]));
        else if (name=="Show init") setBrowser("showInit",!bool(browser["showInit"]));
        else if (name.startsWith("Sort by ")) {
            const auto column=browserSortColumn(name.substring(8));
            if (column>0) send(message({{"action","sort"},{"column",column}}));
        } else if (name=="Save tags") {
            saveBrowserTags();
        } else if (name==exportSysExCaption) {
            send(message({{"action","export"},{"id",selectedPatch}}));
        } else {
            static const std::map<String,String> fileActions{
                {String::fromUTF8("Choose Silver-K folder…"),"dataFolder"},{"Rescan folders","rescanData"},
                {"Skins folder","openSkins"},{"Export template","exportSkin"},
                {"Performance diagnostics","diagnostics"},{"Panic","panic"}};
            static const std::map<String,String> browserActions{
                {String::fromUTF8("Add file…"),"addFile"},{String::fromUTF8("Add folder…"),"addFolder"},{"Rescan","rescan"},
                {"Remove source","removeSource"},{saveCurrentCaption,"saveCurrent"}};
            if (const auto i=fileActions.find(name);i!=fileActions.end()) native(i->second);
            else if (const auto j=browserActions.find(name);j!=browserActions.end())
                send(message({{"action",j->second}}));
        }
        if (error.isNotEmpty()) statusMessage=std::move(error);
        if (page=="Browser") refreshBrowserSoon();
        else repaint();
    }

    void rebuildSoon(bool sectionsOnly=false) {
        if (rebuildQueued) {
            sectionsOnlyQueued&=sectionsOnly;
            return;
        }
        sectionsOnlyQueued=sectionsOnly;
        rebuildQueued=true;
        const juce::Component::SafePointer<Canvas> self(this);
        juce::MessageManager::callAsync([self]{
            if (self && self->rebuildQueued) self->rebuild(self->sectionsOnlyQueued);
        });
    }

    Rect parameterCaption(const Widget& w) const {
        if (!w.caption.isEmpty() || !w.range()) return w.caption;

        // The envelope faders have captions below the track. Performance
        // volume captions sit on the left, immediately above their tracks.
        const auto name=w.name.fromLastOccurrenceOf(" ",false,false);
        const bool vertical=!w.thumb.isEmpty();
        const auto captionBand=w.bounds.withY(vertical?w.bounds.getBottom():w.bounds.getY()-30.f)
            .withHeight(vertical?34.f:30.f);
        for (const auto& caption:fixedText)
            if (captionBand.contains(caption.bounds.getCentre()) && caption.current.equalsIgnoreCase(name))
                return caption.bounds.expanded(3.f,2.f);
        return {};
    }

    void closeParameterEntry() {
        if (!parameterEntry) return;
        const auto area=parameterEntry->getBounds();
        parameterEntry.reset();
        repaint(area);
    }

    void openParameterEntry(const SliderBinding& entry) {
        auto& slider=*entry.control;
        const auto& widget=*entry.widget;
        const auto offset=int(spec(widget)["offset"]);
        const auto minimum=juce::roundToInt(slider.getMinimum())+offset;
        const auto maximum=juce::roundToInt(slider.getMaximum())+offset;
        const auto label=readoutLabel(widget);
        parameterEntry=std::make_unique<juce::TextEditor>(label);
        auto& editor=*parameterEntry;
        editor.setComponentID("soultrain-parameter-entry");
        editor.setBounds(juce::Rectangle<int>(58,24).withCentre(entry.caption.getCentre().roundToInt())
            .constrainedWithin(getLocalBounds()));
        editor.setFont(skin.font(14.f,600));
        editor.setJustification(juce::Justification::centred);
        editor.setIndents(2,0);
        editor.setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xfff2f6fa));
        editor.setColour(juce::TextEditor::textColourId,juce::Colour(0xff203349));
        editor.setColour(juce::TextEditor::outlineColourId,juce::Colour(0xff587d9e));
        editor.setColour(juce::TextEditor::focusedOutlineColourId,juce::Colour(0xff587d9e));
        editor.setTooltip(label+" ("+String(minimum)+" to "+String(maximum)+")");
        editor.setText(String(juce::roundToInt(slider.getValue())+offset),false);

        const juce::Component::SafePointer<Canvas> self(this);
        const juce::Component::SafePointer<juce::TextEditor> field(&editor);
        const juce::Component::SafePointer<juce::Slider> control(&slider);
        const auto cancel=[self,field] {
            juce::MessageManager::callAsync([self,field] {
                if (self && field && self->parameterEntry.get()==field.getComponent())
                    self->closeParameterEntry();
            });
        };
        editor.onEscapeKey=cancel;
        editor.onFocusLost=cancel;
        editor.onReturnKey=[self,field,control,offset,minimum,maximum,firmware=processor.osFile,
                            part=processor.currentPart(),performance=processor.performanceMode()] {
            if (!field) return;
            const auto text=field->getText().trim();
            juce::MessageManager::callAsync([self,field,control,offset,minimum,maximum,firmware,part,performance,text] {
                if (!self || !field || self->parameterEntry.get()!=field.getComponent()) return;
                if (!control || !control->isEnabled() || self->processor.isFirmwareLoading() ||
                    self->processor.osFile!=firmware || self->processor.performanceMode()!=performance ||
                    (self->page!="Perf" && self->processor.currentPart()!=part)) {
                    self->closeParameterEntry();
                    return;
                }
                const bool negative=text.startsWithChar('-');
                const auto digits=negative || text.startsWithChar('+')?text.substring(1):text;
                const auto validNumber=digits.isNotEmpty() && digits.length()<=9 && digits.containsOnly("0123456789");
                const auto value=validNumber?digits.getIntValue()*(negative?-1:1):0;
                if (!validNumber || value<minimum || value>maximum) {
                    const juce::Colour invalid(0xffac4039);
                    field->setColour(juce::TextEditor::outlineColourId,invalid);
                    field->setColour(juce::TextEditor::focusedOutlineColourId,invalid);
                    field->selectAll();
                    return;
                }
                self->closeParameterEntry();
                const auto rawValue=value-offset;
                if (control && control->getValue()!=rawValue) {
                    const juce::Slider::ScopedDragNotification gesture(*control);
                    control->setValue(rawValue,juce::sendNotificationSync);
                }
            });
        };
        pickerLayer.addAndMakeVisible(editor);
        if (editor.isShowing() && editor.getPeer()) editor.grabKeyboardFocus();
        editor.selectAll();
        repaint(editor.getBounds());
    }

    void addRange(Widget& w) {
        auto* parameterToEdit=parameter(w);
        if (!parameterToEdit) return;
        SliderBinding entry;
        entry.widget=&w;
        entry.caption=parameterCaption(w);
        if (w.dial()) {
            const auto& specValue=spec(w);
            const soultrain::SoulTrainKnob::Range range{int(specValue["min"]),int(specValue["max"]),
                int(specValue["offset"]),int(specValue["default"])};
            const auto& dial=skin.knob;
            auto knob=std::make_unique<soultrain::SoulTrainKnob>(dial,range,w.name);
            knob->setSize(int(dial.width),int(dial.height));
            knob->setTransform(juce::AffineTransform::translation(w.bounds.getPosition()-dial.dial.getPosition()));
            knob->setAlpha(captionOpacity(w.bounds));
            entry.control=std::move(knob);
        } else {
            const auto vertical=!w.thumb.isEmpty();
            const auto rail=faderRail(w);
            const auto first=float(vertical?rail.getY():rail.getX());
            auto fader=std::make_unique<HitSlider>(faderLook,vertical,faderBounds(w),
                w.bounds.getSmallestIntegerContainer(),rail,faderThumbHitBounds(w,first));
            w.fader=fader.get();
            entry.control=std::move(fader);
        }
        auto& control=*entry.control;
        control.setName(w.name);
        control.setComponentID(String(partFor(w))+":"+binding(w));
        control.setEnabled(available(w));
        entry.attachment=std::make_unique<juce::SliderParameterAttachment>(*parameterToEdit,control,nullptr);
        control.onDragStart=[this,widget=&w]{
            adjustedParameter=nullptr;
            draggedParameter=widget;
            refreshLcd();
        };
        control.onDragEnd=[this]{
            if (adjustedParameter) adjustmentExpires=juce::Time::getMillisecondCounterHiRes()+1800.;
            draggedParameter=nullptr;
            refreshLcd();
        };
        control.onValueChange=[this,widget=&w]{
            if (draggedParameter==widget) {
                adjustedParameter=widget;
                refreshLcd();
            }
            if (widget->range() && page=="Perf" && widget->part)
                repaint(faderReadout(*widget).getSmallestIntegerContainer());
        };
        addAndMakeVisible(control);
        sliders.push_back(std::move(entry));
    }

    void addWave(const Widget& w) {
        auto native=std::make_unique<soultrain::WaveIndicator>(skin.wave,skin.wave.choicesFor(model),
            unsigned(oscillatorFor(w)+1),raw(w));
        native->setSize(int(skin.wave.width),int(skin.wave.height));
        native->setTransform(juce::AffineTransform::translation(w.waveRect.getPosition()-skin.wave.face.getPosition()));
        native->setEnabled(available(w));
        native->onWaveformChange=[this,w](int value){setParameter(w,value);refreshLcd();};
        const juce::Component::SafePointer<soultrain::WaveIndicator> wave(native.get());
        native->onMenuRequested=[this,wave](juce::Component& trigger,const auto& choices,int selected) {
            if (!wave || !wave->isEnabled()) return;
            juce::StringArray labels;
            for (const auto& choice:choices) labels.add(choice.title);
            presentPicker(trigger,std::move(labels),selected,false,[wave](int index) {
                if (wave && wave->isEnabled() && index>=0) wave->selectWaveform(size_t(index));
            },getLocalArea(wave.getComponent(),wave->faceBounds()));
            picker->setComponentID("soultrain-wave-menu");
            wavePicker=wave;
            wave->setMenuOpen(true);
        };
        addAndMakeVisible(*native);
        waves.push_back({w,std::move(native)});
    }

    std::vector<soultrain::Caption> makeFixedText(const juce::var& layout) {
        std::vector<soultrain::Caption> result;
        Rect removedSwitch;
        if (const auto* widgets=layout["widgets"].getArray())
            for (const auto& widget:*widgets)
                if (widget["label"]==effectsMasterCaption) {
                    removedSwitch=rectangle(widget["rect"]);
                    break;
                }
        if (const auto* captions=layout["captionRuns"].getArray()) {
            result.reserve(size_t(captions->size()));
            for (const auto& ink:*captions) {
                if (ink["text"]=="on/off" && removedSwitch.contains(rectangle(ink["rect"]).getCentre())) continue;
                const auto box=rectangle(ink["rect"]);
                if (ink["text"]=="SoulTrain" && box.getY()>915.f && box.getX()<160.f) continue;
#if JUCE_WINDOWS
                // Effect variants share nearly all of their glyphs and glows.
                const auto authored=juce::JSON::toString(ink,true);
                result.push_back(authoredCaptions.try_emplace(authored,skin,ink).first->second);
#else
                result.emplace_back(skin,ink);
#endif
                auto& caption=result.back();
                if (caption.bounds.getY()<=915.f) continue;
                if (caption.current=="0.5.0")
                    caption.set(String(JucePlugin_VersionString)+String(SILVER_RELEASE_VERSION).trimCharactersAtStart("0123456789."));
            }
        }
        return result;
    }

    const std::vector<soultrain::Caption>& prepareFixedText(const String& name,const juce::var& layout) {
        const auto cached=std::find_if(captionLayouts.begin(),captionLayouts.end(),
            [&](const auto& entry){return entry.first==name;});
        if (cached!=captionLayouts.end()) {
            captionLayouts.splice(captionLayouts.end(),captionLayouts,cached);
        } else {
            captionLayouts.emplace_back(name,makeFixedText(layout));
            if (captionLayouts.size()>4) captionLayouts.pop_front();
        }
        return captionLayouts.back().second;
    }

    void addFixedText(const String& name,const juce::var& layout) {
        const auto& captions=prepareFixedText(name,layout);
        fixedText.insert(fixedText.end(),captions.begin(),captions.end());
    }

#if JUCE_WINDOWS
    static String layoutOutsideEffects(const juce::var& layout,Rect section) {
        juce::Array<juce::var> outside{layout["width"],layout["height"]};
        if (const auto* regionsObject=layout["regions"].getDynamicObject())
            for (const auto& region:regionsObject->getProperties())
                if (region.name!=juce::Identifier("effectsControls")) {
                    outside.add(region.name.toString());
                    outside.add(region.value);
                }
        for (const auto* field:{"widgets","captionRuns"}) {
            juce::Array<juce::var> nodes;
            if (const auto* source=layout[field].getArray())
                for (const auto& node:*source) {
                    const auto bounds=rectangle(node["rect"]);
                    if (bounds.isEmpty() || !section.contains(bounds)) nodes.add(node);
                }
            outside.add(juce::var(std::move(nodes)));
        }
        return juce::JSON::toString(juce::var(std::move(outside)),true);
    }

    const EffectLayout& prepareEffect(int index,bool chorusOptions=false) {
        const auto name=effectKey(index,chorusOptions);
        if (const auto cached=effectLayouts.find(name);cached!=effectLayouts.end())
            return cached->second;

        const auto base=read(key(0)+".json");
        const auto layout=index==0?base:effectLayout(index,chorusOptions);
        EffectLayout result;
        if (index!=0) result.overlay=picture(key(index)+".png");
        auto section=rectangle(base["regions"]["effectsControls"]["rect"]);
        const auto controlsArea=section;
        if (const auto* nodes=base["widgets"].getArray())
            for (const auto& node:*nodes)
                if (node["role"]=="tab")
                    section=section.getUnion(rectangle(node["rect"]));
        if (const auto gear=chorusGear(base)) section=section.getUnion(gear->bounds);
        section=section.expanded(10.f);

        const auto outside=layoutOutsideEffects(layout,section);
        const bool local=index==0 || prepareEffect(0).outside==outside;
        const auto& captions=prepareFixedText(name,layout);
        // Full layouts from custom artwork can change areas outside the FX
        // section. Only reuse the rest of the frame when those entries agree.
        if (layout.isObject() && !controlsArea.isEmpty() &&
            rectangle(layout["regions"]["effectsControls"]["rect"])==controlsArea &&
            local) {
            auto area=section.getSmallestIntegerContainer();
            if (index==chorusEffect)
                if (const auto gear=chorusGear(layout)) area=area.getUnion(mainWidgetPaintBounds(*gear));
            if (const auto* nodes=layout["widgets"].getArray())
                for (const auto& node:*nodes) {
                    Widget widget(node);
                    if (widget.bounds.isEmpty() || !section.contains(widget.bounds)) continue;
                    widget.applyStandardHeight();
                    area=area.getUnion(mainWidgetPaintBounds(widget));
                    if (widget.dial()) {
                        const auto& knob=skin.knob;
                        const auto position=widget.bounds.getPosition()-knob.dial.getPosition();
                        area=area.getUnion(Rect(position.x,position.y,knob.width,knob.height).getSmallestIntegerContainer());
                    }
                }
            for (const auto& caption:captions)
                if (section.contains(caption.bounds)) area=area.getUnion(caption.paintBounds());
            if (result.overlay.isValid()) {
                const auto point=origin(layout["imageOrigin"]);
                const auto scale=float(std::max(1,int(layout["imageScale"])));
                area=area.getUnion(Rect(point.x,point.y,result.overlay.getWidth()/scale,
                                        result.overlay.getHeight()/scale).getSmallestIntegerContainer());
            }
            result.repaintArea=area.expanded(2).getIntersection(getLocalBounds());
        }
        if (index==0) result.outside=outside;
        return effectLayouts.emplace(name,std::move(result)).first->second;
    }

    void warmNextSection() {
        if (!usesEffects() || lastLoading || rebuildQueued || picker || parameterEntry ||
            draggedParameter || drumSequencer) return;
        const auto* frame=dynamic_cast<const SkinFrame*>(getCachedComponentImage());
        if (!frame || !frame->hasRendered()) return;

        const auto base=key(0);
        if (warmedSections==base) return;
        // Prepare one hidden section per idle timer tick after the first frame.
        const auto suffixes=model=="KS"?juce::StringArray{"-voices","-arp"}:juce::StringArray{"-voices"};
        for (const auto& suffix:suffixes) {
            const auto name=base+suffix;
            if (panelLayouts.find(name)==panelLayouts.end()) {
                preparePanel(name);
                return;
            }
        }
        for (size_t index=0;index<effectNames.size();++index)
            if (effectLayouts.find(key(int(index)))==effectLayouts.end()) {
                prepareEffect(int(index));
                return;
            }
        prepareEffect(chorusEffect,true);
        warmedSections=base;
    }

    std::vector<float> parameterValues() const {
        std::vector<float> values;
        values.reserve(parameters.size());
        for (const auto& entry:parameters)
            values.push_back(entry.second?entry.second->getValue():0.f);
        return values;
    }
#endif

    void addInput(const Widget& w) {
        if (w.type!="search" && w.type!="text") return;
        auto input=std::make_unique<juce::TextEditor>(w.name);
        const bool editsTags=w.name=="Selected patch tags";
        input->setEnabled(available(w));
        input->setBounds(w.bounds.getSmallestIntegerContainer());
        input->setColour(juce::TextEditor::backgroundColourId,juce::Colours::transparentBlack);
        input->setColour(juce::TextEditor::outlineColourId,juce::Colours::transparentBlack);
        if(editsTags)styleTagInput(*input);
        else input->setColour(juce::TextEditor::textColourId,w.textColour);
        input->setFont(font(std::max(11.f,w.fontSize)));
        input->setJustification(juce::Justification::centredLeft);
        input->setBorder(juce::BorderSize<int>{0});
        input->setIndents(7,0);
        if (editsTags) {
            input->setText(tags,false);
            input->onTextChange=[this,input=input.get()]{tags=input->getText();tagsEdited=tags!=savedTags;};
            input->onReturnKey=[this]{saveBrowserTags();};
        } else {
            input->setTextToShowWhenEmpty(w.placeholder,w.textColour.withMultipliedAlpha(.65f));
            const auto search=w.name.containsIgnoreCase("search");
            input->setText(browser[search?"query":"tagFilter"].toString(),false);
            input->onTextChange=[this,input=input.get(),search]{setBrowser(search?"query":"tagFilter",input->getText());};
        }
        addAndMakeVisible(*input);
        fields.push_back(std::move(input));
    }

    void addButton(Widget& w) {
        const auto sortHeader=w.sortHeader();
        if (w.name.isEmpty() || (w.bounds.isEmpty() && !sortHeader)) return;
        const auto authored=hasAuthoredRollover(w);
        using Rollover=HitButton::Rollover;
        auto button=std::make_unique<HitButton>(w.name,sortHeader?Rollover::Fill:
            w.choiceControl()?Rollover::Choice:authored?Rollover::None:Rollover::Outline);
        button->setTitle(w.name);
        if (w.name==soultrain::DrumSequencer::EditName) {
            button->setComponentID("drumSequencerEdit");
            button->setDescription("Open the Drum Sequencer");
            sequencerEditButton=button.get();
        } else if (w.name==drumVoiceEditCaption) {
            button->setComponentID("drumVoiceEdit");
            drumVoiceEditButton=button.get();
        }
        if (w.id=="arp_rhythm")button->setComponentID("arpRhythmPicker");
        button->setBounds(w.buttonBounds());
        if (sortHeader) button->setMouseCursor(juce::MouseCursor::PointingHandCursor);
        button->setEnabled(available(w));
        auto area=w.bounds;
        if (authored && !w.standardButton() && !w.lampArt.isEmpty())
            area=area.getUnion(soultrain::SkinArt::indicatorBounds(w.lampArt).expanded(2.f));
        if (w.name=="Select firmware") button->setAlpha(.01f);
        auto* rawButton=button.get();
        w.hitButton=rawButton;
        const auto dirty=(w.standardButton()?w.buttonBounds():area.getSmallestIntegerContainer()).getUnion(w.paintBounds);
        if (authored || w.choiceControl() || w.choiceGroup>=0)
            button->onStateChange=[this,widget=&w,dirty]{buttonVisualStateChanged(*widget,dirty);};
        button->onClick=[this,w,rawButton,dirty] {
            const juce::Component::SafePointer<HitButton> live(rawButton);
            rawButton->finishPressPreview();
            if (w.picker()) openMenu(w,*rawButton);
            else activate(w);
            if (!live) return;
            if (w.choiceGroup>=0) refreshChoiceGroup(choiceGroups[size_t(w.choiceGroup)]);
            else if (!w.id.isEmpty()) repaint(dirty);
        };
        addAndMakeVisible(*button);
        if (w.bounds.isEmpty()) button->setVisible(false);
        buttons.push_back(std::move(button));
    }

    void compactSetup() {
        const auto modelChoice=std::find_if(widgets.begin(),widgets.end(),[](const Widget& w){return w.name=="Setup model";});
        if (modelChoice!=widgets.end()) {
            const auto row=modelChoice->bounds;
            const auto cell=std::floor((row.getWidth()-12.f)/2.f);
            const auto gap=row.getWidth()-2.f*cell;
            const std::array<String,2> controls{"Setup model","Clock source"};
            for (size_t i=0;i<controls.size();++i) {
                const auto control=std::find_if(widgets.begin(),widgets.end(),[&](const Widget& w){return w.name==controls[i];});
                if (control==widgets.end()) continue;
                const auto position=row.getPosition().translated(float(i)*(cell+gap),0.f);
                const auto delta=position-control->bounds.getPosition();
                control->moveButton(delta);
                control->bounds=control->bounds.withWidth(cell);
                for (auto& caption:fixedText)
                    if (caption.current==(i==0?String("Model"):controls[i])) caption.translate(delta);
            }
        }

        const auto sectionOffset=[](Rect area) -> juce::Point<float> {
            if (area.getY()<175.f || area.getY()>=780.f) return {};
            if (area.getX()>=1000.f) return {setupDataX-originalSetupDataX,0.f};
            if (area.getX()>=520.f) return {setupSynthX-setupDataX,260.f};
            if (area.getY()>=530.f && area.getY()<580.f) return {0.f,-160.f};
            return {};
        };
        for (auto& widget:widgets) {
            if (const auto offset=sectionOffset(widget.bounds);offset!=juce::Point<float>{}) widget.moveButton(offset);
        }
        for (auto& caption:fixedText) {
            if (const auto offset=sectionOffset(caption.bounds);offset!=juce::Point<float>{}) caption.translate(offset);
        }
    }

    void layoutPerformanceNavigation() {
        for (auto& previous:widgets) {
            if (!previous.part || !previous.name.contains("Previous")) continue;
            const auto next=std::find_if(widgets.begin(),widgets.end(),[&](const Widget& w){
                return w.part==previous.part && w.name.startsWith("Next");
            });
            if (next==widgets.end()) continue;
            const auto gap=next->bounds.getX()-previous.bounds.getRight();
            const auto buttonWidth=std::round((previous.bounds.getWidth()+next->bounds.getWidth())*.5f);
            const auto position=previous.bounds.getX()+buttonWidth+gap;
            const auto replace=[&](Widget& widget,const String& label,Rect oldBounds) {
                const auto caption=std::find_if(fixedText.begin(),fixedText.end(),[&](const auto& ink){
                    return ink.current==widget.name && oldBounds.contains(ink.bounds.getCentre());
                });
                widget.name=widget.baseline=label;
                widget.bounds=widget.bounds.withWidth(buttonWidth);
                if (caption!=fixedText.end()) {
                    *caption=soultrain::Caption(skin.font(widget.fontSize,widget.fontWeight),widget.bounds,
                        widget.bounds,widget.textColour,{},1.f,juce::Justification::centred);
                    caption->set(label);
                }
            };
            replace(previous,"Prev",previous.bounds);
            const auto oldNext=next->bounds;
            next->moveButton({position-oldNext.getX(),0.f});
            replace(*next,"Next",oldNext);
        }
    }

    void layoutButtonRows() {
        std::array<std::vector<Widget*>,3> rows;
        for (auto& w:widgets) {
            if (!w.standardButton() || w.bounds.isEmpty()) continue;
            if (w.visualType=="header-button" && std::find(pageNames.begin(),pageNames.end(),w.name)!=pageNames.end())
                rows[0].push_back(&w);
            else if (w.role=="tab") rows[1].push_back(&w);
            else if (w.id=="voice_mode") rows[2].push_back(&w);
        }
        for (auto& row:rows) {
            if (row.size()<2) continue;
            std::sort(row.begin(),row.end(),[](const Widget* a,const Widget* b){return a->bounds.getX()<b->bounds.getX();});
            auto left=float(juce::roundToInt(row.front()->bounds.getX()));
            for (size_t i=0;i<row.size();++i) {
                auto& w=*row[i];
                const auto original=w.bounds;
                const auto seam=i+1<row.size()?(original.getRight()+row[i+1]->bounds.getX())*.5f:original.getRight();
                const auto right=float(juce::roundToInt(seam));
                // Each face is inset by one pixel. A shared outer edge gives
                // two pixels of visible space while keeping the row's width.
                w.moveButton({left-original.getX(),0.f});
                w.bounds=w.bounds.withWidth(right-left);
                const auto centreShift=w.bounds.getCentreX()-original.getCentreX();
                if (centreShift!=0.f)
                    for (auto& caption:fixedText)
                        if ((caption.current==w.name || caption.current==w.baseline) &&
                            original.contains(caption.bounds.getCentre()))
                            caption.translate({centreShift,0.f});
                left=right;
            }
        }
    }

    void rebuild(bool sectionsOnly=false) {
        rebuildQueued=false;
        const auto requested=std::exchange(requestedSections,std::nullopt);
        if (sectionsOnly && requested) {
            if (requested->effect==sections.effect && requested->voices==sections.voices &&
                requested->arp==sections.arp && requested->chorusOptions==sections.chorusOptions) return;
#if JUCE_WINDOWS
            if (replaceSections(*requested)) return;
#endif
        }
        if (requested) sections=*requested;
        rebuildLayout();
#if JUCE_WINDOWS
        if (auto* frame=dynamic_cast<SkinFrame*>(getCachedComponentImage())) frame->pageChanged();
#endif
        repaint();
    }

#if JUCE_WINDOWS
    void addPanelPaintAreas(juce::RectangleList<int>& areas,const Panel& panel,bool arp) const {
        const auto& area=panel.area;
        auto covered=area.getSmallestIntegerContainer();
        for (const auto& w:widgets)
            if (area.contains(w.bounds.getCentre()) || (arp && (w.name==soultrain::DrumSequencer::EditName ||
                                                               w.name==drumVoiceEditCaption)))
                covered=covered.getUnion(w.paintBounds);
        for (const auto& slider:sliders)
            if (area.contains(slider.widget->bounds.getCentre()))
                covered=covered.getUnion(getLocalArea(slider.control.get(),slider.control->getLocalBounds()));
        for (const auto& wave:waves)
            if (area.contains(wave.widget.bounds.getCentre()))
                covered=covered.getUnion(getLocalArea(wave.control.get(),wave.control->getLocalBounds()));
        for (const auto& caption:fixedText)
            if (area.contains(caption.bounds.getCentre()))
                covered=covered.getUnion(caption.paintBounds());
        for (const auto& caption:panel.captions)
            covered=covered.getUnion(caption.paintBounds());
        // A replaced button can leave a shadow beyond the panel rectangle.
        areas.add(covered.expanded(6).getIntersection(getLocalBounds()));
    }

    bool replaceSections(Sections next) {
        auto* frame=dynamic_cast<SkinFrame*>(getCachedComponentImage());
        if (!frame || !usesEffects() || page!=selectedPage || lastLoading ||
            lastReady!=processor.firmwareReady() || lastPart!=processor.currentPart() ||
            lastPerformance!=processor.performanceMode() ||
            model!=(processor.isKStation()?"K":"KS") || !frame->canRetainFrame(frameState()))
            return false;

        auto areas=frameLiveAreas(false);
        const auto base=key(0);
        if (next.effect!=sections.effect || next.chorusOptions!=sections.chorusOptions) {
            const auto previous=prepareEffect(sections.effect,sections.chorusOptions).repaintArea;
            const auto upcoming=prepareEffect(next.effect,next.chorusOptions).repaintArea;
            if (previous.isEmpty() || upcoming.isEmpty()) return false;
            areas.add(previous.getUnion(upcoming));
        }
        const auto* voices=next.voices!=sections.voices?&preparePanel(base+"-voices"):nullptr;
        const auto* arp=next.arp!=sections.arp && model=="KS"?&preparePanel(base+"-arp"):nullptr;
        if ((voices && voices->area.isEmpty()) || (arp && arp->area.isEmpty())) return false;
        if (voices) addPanelPaintAreas(areas,*voices,false);
        if (arp) addPanelPaintAreas(areas,*arp,true);

        const auto part=lastPart;
        const auto values=parameterValues();
        const auto readout=lcdText;
        frame->replaceRegion(std::move(areas),[&](juce::RectangleList<int>& dirty)->std::optional<SkinPageState> {
            sections=next;
            rebuildLayout(true);
            if (base!=key(0) || part!=lastPart || values!=parameterValues()) return {};
            if (voices) addPanelPaintAreas(dirty,*voices,false);
            if (arp) addPanelPaintAreas(dirty,*arp,true);
            if (readout!=lcdText) dirty.add(lcdGlowBounds);
            for (const auto& area:frameLiveAreas(false)) dirty.add(area);
            return frameState();
        });
        return true;
    }
#endif

    void rebuildLayout(bool keepLcd=false) {
        const auto oldModel=model;
        const auto oldPage=page;
        lastLoading=processor.isFirmwareLoading();
        if (model.isEmpty() || !lastLoading) model=processor.isKStation()?"K":"KS";
        page=model=="K" && selectedPage=="Perf"?"Main":selectedPage;
        if (page!=oldPage || model!=oldModel) sections.chorusOptions=false;
        if (oldModel.isNotEmpty() && oldModel!=model) {
            pwm.fill(0);pitch.fill(0);filterMode=0;mixerMode=0;sections.arp=sections.voices=false;
        }
        lastReady=processor.firmwareReady();
        lastPerformance=processor.performanceMode();
        lastMask=page=="Perf" && lastPerformance?processor.displayedActiveParts():0;
        lastPart=processor.currentPart();
        closeParameterEntry();
        closePicker();
        drumSequencer.reset();
        updateBrowser();
        const auto isK=processor.isKStation();
        if (parameters.empty() || parametersForK!=isK) {
            parametersForK=isK;
            parameters.clear();
            for (const auto& c:processor.visibleControls()) {
                const auto id=semanticId(c.id);
                for (unsigned part=1;part<=(isK?1u:4u);++part)
                    parameters.emplace(String(part)+":"+id,processor.parameters.getParameter(
                        isK?"k_"+String(c.id):Processor::parameterId(c,part-1)));
            }
        }
        const auto baseKey=key(0);
        const auto layout=sections.effect==0 || !usesEffects()?baseKey:scene();
        const auto baseFile=read(baseKey+".json");
        const auto file=layout==baseKey?baseFile:effectLayout(sections.effect,sections.chorusOptions);
        regions=file["regions"];
        updateLcdGlowMask();
        columnLayout=file["columns"];
        backdrop=picture(baseKey+".png");
        backdropScale=std::max(1,int(baseFile["imageScale"]));
        imageOrigin={};
        opaqueBackdrop=backdrop.isValid() && !backdrop.hasAlphaChannel() &&
            backdrop.getWidth()/backdropScale>=width && backdrop.getHeight()/backdropScale>=height;
        fxArt={};
#if JUCE_WINDOWS
        if (usesEffects()) fxArt=prepareEffect(sections.effect,sections.chorusOptions).overlay;
#endif
        if (usesEffects() && sections.effect!=0) {
#if !JUCE_WINDOWS
            fxArt=picture(key(sections.effect)+".png");
#endif
            fxOrigin=origin(file["imageOrigin"]);
            fxScale=std::max(1,int(file["imageScale"]));
        }
        draggedParameter=adjustedParameter=inspectedParameter=nullptr;
        sequencerEditButton=drumVoiceEditButton=nullptr;
        buttonShadows.clear();
        sliders.clear();buttons.clear();fields.clear();panels.clear();waves.clear();widgets.clear();
        tagEditor.reset();
        columnGrips.clear();
        fixedText.clear();
        choiceCaptions.clear();
        if (keepLcd) {
            for (auto slot=textSlots.begin();slot!=textSlots.end();) {
                if (slot->first=="lcdModel" || slot->first=="lcdName") ++slot;
                else slot=textSlots.erase(slot);
            }
        } else {
            textSlots.clear();
            readoutGlows={};
        }
        addFixedText(layout,file);
        if (page=="Browser")
            fixedText.erase(std::remove_if(fixedText.begin(),fixedText.end(),[](const soultrain::Caption& caption){
                return browserActionCaption(caption.current);
            }),fixedText.end());
        if (page=="Perf") {
            performanceOpacity=float(file["performanceOpacity"]);
            for (size_t part=0;part<performanceAreas.size();++part) {
                const juce::Identifier key("part"+String(int(part+1)));
                performanceAreas[part]=rectangle(regions[key]["rect"]);
            }
            fixedText.erase(std::remove_if(fixedText.begin(),fixedText.end(),[](const auto& ink){
                return ink.current=="Enable Performance";
            }),fixedText.end());
        }
        if (const auto* nodes=file["widgets"].getArray())
            for (const auto& node:*nodes)
                if (node["label"]!=effectsMasterCaption && (page!="Perf" || node["label"]!="Enable Performance"))
                    widgets.emplace_back(node);
        if (usesEffects()) {
            if (sections.effect==chorusEffect)
                if (auto gear=chorusGear(file)) widgets.push_back(std::move(*gear));
            if (sections.voices) addPanel(baseKey+"-voices");
            if (sections.arp && model=="KS") addPanel(baseKey+"-arp");
        }
        if (page=="Setup") {
            // The existing layout provides the original button backgrounds.
            // The file actions and the folder guide are supplied by Setup.
            fixedText.erase(std::remove_if(fixedText.begin(),fixedText.end(),[](const soultrain::Caption& run){
                return (run.bounds.getX()>=1000.f && run.bounds.getY()>=290.f && run.bounds.getY()<760.f)
                    || run.current.equalsIgnoreCase("Demo song") || run.current=="Panic" || run.current=="Interface size";
            }),fixedText.end());
            widgets.erase(std::remove_if(widgets.begin(),widgets.end(),[](const Widget& w){
                return w.name==String::fromUTF8("Data folder…") || w.name==String::fromUTF8("Wave ROM…")
                    || w.name==String::fromUTF8("KS Data…") || w.name==String::fromUTF8("Load skin…")
                    || w.name.equalsIgnoreCase("Demo song") || w.name=="Original interface";
            }),widgets.end());
            for(auto& w:widgets){
                if(w.name==String::fromUTF8("Load firmware…")){
                    w.name=String::fromUTF8("Choose Silver-K folder…");
                    w.bounds=w.bounds.withPosition(1012.f,348.f).withWidth(166.f);
                }else if(w.name=="Rescan ROMs"){
                    w.name="Rescan folders";
                    w.bounds=w.bounds.withPosition(1186.f,348.f);
                }else if(w.name=="Skins folder")w.bounds=w.bounds.withPosition(1012.f,571.f);
                else if(w.name=="Export template")w.bounds=w.bounds.withPosition(1116.f,571.f);
                else if(w.name=="Panic")w.bounds=w.bounds.withX(716.f);
                else continue;
                w.artAnchor=w.bounds.getPosition();
            }
            const auto firmware=std::find_if(widgets.begin(),widgets.end(),[](const Widget& w){return w.name=="Firmware";});
            if (firmware!=widgets.end()) {
                constexpr float policyWidth=165.f, gap=14.f;
                auto selection=*firmware;
                selection.name=firmwareSwitchCaption;
                selection.bounds=firmware->bounds.withLeft(firmware->bounds.getRight()-policyWidth);
                selection.caption=selection.bounds.withY(selection.bounds.getY()-25.f).withHeight(19.f);
                selection.artAnchor=selection.bounds.getPosition();
                firmware->bounds=firmware->bounds.withTrimmedRight(policyWidth+gap);
                widgets.push_back(std::move(selection));
            }
            const auto size=std::find_if(widgets.begin(),widgets.end(),[](const Widget& w){return w.name=="Interface size";});
            if(size!=widgets.end()){
                constexpr float sizeWidth=144.f, gap=14.f;
                const auto row=size->bounds;
                auto selection=*size;
                selection.name="Skin";
                selection.baseline="SoulTrain (Default)";
                selection.bounds=row.withTrimmedRight(sizeWidth+gap);
                size->bounds=row.withLeft(row.getRight()-sizeWidth);
                for(auto* choice:{&selection,&*size}){
                    choice->caption=choice->bounds.withY(row.getY()-25.f).withHeight(19.f);
                    choice->artAnchor=choice->bounds.getPosition();
                }
                widgets.push_back(std::move(selection));
            }
            compactSetup();
#if JUCE_WINDOWS
            const auto buffer=std::find_if(widgets.begin(),widgets.end(),[](const Widget& w){return w.name=="Audio buffer profile";});
            if (buffer!=widgets.end()) {
                auto renderer=*buffer;
                renderer.name=rendererCaption;
                renderer.bounds=renderer.bounds.withPosition(setupSynthX,735.f).withWidth(200.f);
                renderer.caption=renderer.bounds.withY(712.f).withHeight(19.f);
                renderer.artAnchor=renderer.bounds.getPosition();
                widgets.push_back(std::move(renderer));
            }
#endif
        }
        if (page=="Perf") layoutPerformanceNavigation();
        const auto trigger=rectangle(regions["firmwareTrigger"]["rect"]);
        const auto hasFooter=std::any_of(widgets.begin(),widgets.end(),[](const Widget& w){return w.name=="Select firmware";});
        if (page=="Browser") {
            if (const auto toggle=std::find_if(widgets.begin(),widgets.end(),[](const Widget& w){
                return w.name=="Show all bank slots";
            });toggle!=widgets.end()) {
                auto showInit=*toggle;
                showInit.name="Show init";
                showInit.moveButton({140.f,0.f});
                showInit.caption=Rect(showInit.bounds.getRight()+6.f,showInit.bounds.getY()-3.f,60.f,18.f);
                widgets.push_back(std::move(showInit));
            }
            const auto saved=browser["columnLayout"];
            if (saved.isObject()) {
                hiddenColumns.clear();
                if (const auto* keys=saved["hidden"].getArray())
                    for (const auto& key:*keys) if (key.toString()!="favourite") hiddenColumns.addIfNotAlreadyThere(key.toString());
                columnWidths.clear();
                if (const auto* widths=saved["widths"].getDynamicObject()) {
                    const auto& properties=widths->getProperties();
                    for (int i=0;i<properties.size();++i)
                        columnWidths[properties.getName(i).toString()]=juce::jlimit(56.f,float(width),float(properties.getValueAt(i)));
                }
            }
            layoutSortControls();
        }
        if (!hasFooter && !trigger.isEmpty()) {
            Widget firmware(juce::var{});
            firmware.name="Select firmware";
            firmware.role="combobox";
            firmware.bounds=trigger;
            widgets.push_back(firmware);
        }
        layoutButtonRows();
        if (model=="KS" && sections.arp && usesEffects()) {
            const auto rhythm=std::find_if(widgets.begin(),widgets.end(),[](const Widget& w){return w.id=="arp_rhythm";});
            const auto* authored=baseFile["widgets"].getArray();
            if (rhythm!=widgets.end() && authored) for (const auto& node:*authored) {
                if (node["param"]!="voice_mode" || node["label"]!="Mono") continue;
                Widget drum(node);
                const auto next=std::find_if(authored->begin(),authored->end(),[](const auto& button) {
                    return button["param"]=="voice_mode" && button["label"]=="Mono AG";
                });
                const auto gap=next!=authored->end()?
                    rectangle((*next)["rect"]).getX()-drum.bounds.getRight():1.f;
                if (const auto mono=std::find_if(widgets.begin(),widgets.end(),[](const Widget& w) {
                    return w.id=="voice_mode" && w.name=="Mono";
                });mono!=widgets.end()) drum.bounds=drum.bounds.withWidth(mono->bounds.getWidth());
                drum.name=soultrain::DrumSequencer::EditName;
                drum.id.clear();
                drum.baseline=drum.name;
                drum.moveButton(juce::Point<float>{rhythm->bounds.getRight()+gap,
                    rhythm->bounds.getCentreY()-drum.bounds.getHeight()*.5f}-drum.bounds.getPosition());
                auto voice=drum;
                voice.name=voice.baseline=drumVoiceEditCaption;
                voice.moveButton({drum.bounds.getWidth(),0.f});
                // The action buttons keep their positions from the original Rhythm layout.
                if (const auto unison=std::find_if(widgets.begin(),widgets.end(),[](const Widget& w) {
                    return w.id=="unison" && w.picker();
                });unison!=widgets.end()) {
                    const auto width=unison->bounds.getWidth();
                    const auto captionShift=(width-rhythm->bounds.getWidth())*.5f;
                    rhythm->bounds=rhythm->bounds.withWidth(width);
                    const auto label=std::find_if(fixedText.begin(),fixedText.end(),[&](const auto& caption) {
                        return caption.current==rhythm->name;
                    });
                    if (label!=fixedText.end()) label->translate({captionShift,0.f});
                }
                widgets.push_back(std::move(drum));
                widgets.push_back(std::move(voice));
                break;
            }
        }
        for (auto& widget:widgets) widget.applyStandardHeight();
        initPaintState();
        for (auto& widget:widgets) {
            if (widget.dial() || widget.range()) addRange(widget);
            else if (!widget.waveRect.isEmpty()) addWave(widget);
            else if (widget.name.endsWithIgnoreCase(" waveform") && !widget.readout.isEmpty()) continue;
            else if (widget.textInput()) addInput(widget);
            else addButton(widget);
        }
        refreshDrumControls();
        if (page=="Browser") {
            auto view=rectangle(regions["tableFrame"]["rect"]);
            if (view.isEmpty()) view={40,361,1389,400};
            const auto header=rectangle(regions["tableHeader"]["rect"]);
            const auto top=header.isEmpty()?view.getY()+36:header.getBottom();
            auto rows=juce::Rectangle<float>(view.getX()+1,top,view.getWidth()-2,
                                             std::max(0.f,view.getBottom()-top-1)).getSmallestIntegerContainer();
            list.setBounds(rows);
            addAndMakeVisible(list);
            positionColumnGrips();
        } else {
            list.setVisible(false);
        }
        updateBrowser(true);
        refreshLcd();
    }

    void initPaintState() {
        paintedValues.clear();
        choiceGroups.clear();
        std::map<String,int> groupIndices;
        for (auto& w:widgets) {
            if (w.standardButton() || w.gear()) {
                auto shadow=skin.buttonShadow(w.buttonBounds());
                if (shadow.image.isValid()) buttonShadows.emplace_back(&w,std::move(shadow));
            }
            if (w.range()) {
                w.paintBounds=faderBounds(w);
                if (page=="Perf" && w.part)
                    w.paintBounds=w.paintBounds.getUnion(faderReadout(w).getSmallestIntegerContainer());
            } else if (usesEffects()) w.paintBounds=mainWidgetPaintBounds(w);
            if (w.waveRect.isEmpty() && !w.picker() && !w.dial() && !w.range() && !w.bounds.isEmpty()) {
                const auto key=choiceGroupKey(w);
                if (key.isNotEmpty()) {
                    const auto [group,inserted]=groupIndices.try_emplace(key,int(choiceGroups.size()));
                    if (inserted) choiceGroups.emplace_back();
                    w.choiceGroup=group->second;
                    choiceGroups[size_t(w.choiceGroup)].members.push_back(&w);
                    if (w.paintBounds.isEmpty()) w.paintBounds=mainWidgetPaintBounds(w);
                }
            }
            if (!w.caption.isEmpty() && w.dial() && binding(w)!=w.id) {
                const auto ink=std::find_if(fixedText.begin(),fixedText.end(),[&](const auto& caption) {
                    return w.caption.contains(caption.bounds.getCentre());
                });
                if (ink!=fixedText.end()) {
                    const auto title=assignedKnobCaption(w);
                    ink->setCentredText(title,w.caption,title=="a/d env level"?String("a/d env lev"):String{});
                }
            }
            // Slider attachments invalidate the control on local and host edits.
            if (w.choiceGroup<0 && w.id.isNotEmpty() && !w.dial() && !w.range() && w.waveRect.isEmpty())
                paintedValues.emplace_back(&w,raw(w));
        }
        for (auto& group:choiceGroups) refreshChoiceGroup(group,false);
        buttonCaptions.clear();
        buttonCaptions.resize(fixedText.size());
        for (const auto& w:widgets) {
            if (!w.standardButton()) continue;
            const auto splitPartCaption=w.part && w.name.startsWith("Edit sound");
            for (size_t i=0;i<fixedText.size();++i) {
                const auto& ink=fixedText[i];
                if ((splitPartCaption || ink.current==w.name || ink.current==w.baseline) &&
                    w.bounds.contains(ink.bounds.getCentre()))
                    buttonCaptions[i].widget=&w;
            }
        }
        if (usesEffects()) lastFxLights=fxLights();
        if (page=="Setup") {
            lastSetupValues=setupValues();
            lastSetupZoom=interfaceZoomPercent();
        }
        if (page=="Perf") lastPartNames=processor.partNames();
    }

    std::array<bool,7> fxLights() const {
        std::array<bool,7> lights{};
        std::transform(effectNames.begin(),effectNames.end(),lights.begin(),[this](const auto& effectName){
            return effectLit(effectName);
        });
        return lights;
    }

    void refreshPaintedValues() {
        for (auto& group:choiceGroups) refreshChoiceGroup(group);
        bool changed=false;
        for (auto& [widget,previous]:paintedValues) {
            const auto next=raw(*widget);
            if (previous!=next) {
                previous=next;
                if (usesEffects()) repaint(widget->paintBounds);
                else if (page=="Perf") repaint(widget->buttonBounds().expanded(4));
                else changed=true;
            }
        }
        if (usesEffects()) {
            const auto next=fxLights();
            if (next!=lastFxLights) {
                for (const auto& widget:widgets) if (widget.role=="tab") {
                    const auto option=std::find(effectNames.begin(),effectNames.end(),widget.name);
                    if (option==effectNames.end()) continue;
                    const auto index=size_t(option-effectNames.begin());
                    if (next[index]!=lastFxLights[index]) repaint(widget.paintBounds);
                }
                lastFxLights=next;
            }
        } else if (page=="Setup") {
            auto next=setupValues();
            changed|=next!=lastSetupValues;
            lastSetupValues=std::move(next);
            const int zoom=interfaceZoomPercent();
            if (lastSetupZoom!=zoom) {
                lastSetupZoom=zoom;
                for (const auto& w:widgets)
                    if (w.name=="Interface size") repaint(mainWidgetPaintBounds(w));
            }
        } else if (page=="Perf") {
            auto next=processor.partNames();
            for (size_t part=0;part<next.size();++part)
                if (next[part]!=lastPartNames[part]) repaint(performanceAreas[part].getSmallestIntegerContainer());
            lastPartNames=std::move(next);
        }
        if (changed) repaint();
    }

    const Panel& preparePanel(const String& name) {
        const auto cached=panelLayouts.find(name);
        if (cached!=panelLayouts.end()) return cached->second;

        Panel panel;
        panel.layout=read(name+".json");
        panel.art=picture(name+".png");
        if (panel.layout.isObject() && panel.art.isValid()) {
            const auto position=origin(panel.layout["imageOrigin"]);
            panel.scale=std::max(1,int(panel.layout["imageScale"]));
            panel.area={position.x,position.y,float(panel.art.getWidth())/panel.scale,
                        float(panel.art.getHeight())/panel.scale};
            panel.captions=makeFixedText(panel.layout);
        }
        return panelLayouts.emplace(name,std::move(panel)).first->second;
    }

    void addPanel(const String& name) {
        const auto& panel=preparePanel(name);
        const auto& area=panel.area;
        if (area.isEmpty()) return;
        widgets.erase(std::remove_if(widgets.begin(),widgets.end(),
            [&](const Widget& w){return area.contains(w.bounds.getCentre());}),widgets.end());
        if (const auto* content=panel.layout["widgets"].getArray())
            for (const auto& node:*content) {
                Widget widget(node);
                if (area.contains(widget.bounds.getCentre())) widgets.push_back(std::move(widget));
            }
        fixedText.erase(std::remove_if(fixedText.begin(),fixedText.end(),
            [&](const soultrain::Caption& run){return area.contains(run.bounds.getCentre());}),fixedText.end());
        fixedText.insert(fixedText.end(),panel.captions.begin(),panel.captions.end());
        panels.push_back(&panel);
    }

    void paintPerformance(juce::Graphics& g) const {
        for (unsigned i=0;i<4;++i) {
            if (!g.clipRegionIntersects(performanceAreas[i].getSmallestIntegerContainer())) continue;
            const auto& name=lastPartNames[i];
            const auto title=String::fromUTF8("Preset · ")+(name.isNotEmpty()?name:"Init");
            drawRegion(g,"preset"+String(i+1),title);
        }
    }

    std::vector<BrowserColumn> columns() const {
        std::vector<BrowserColumn> result;
        const auto* input=columnLayout.getArray();
        if (!input || input->isEmpty()) return result;
        const auto start=rectangle((*input)[0]["rect"]).getX();
        const auto end=rectangle((*input)[input->size()-1]["rect"]).getRight();
        float occupied=0.f;
        for (const auto& node:*input) {
            const auto key=node["key"].toString();
            if (hiddenColumns.contains(key)) continue;
            auto area=rectangle(node["rect"]);
            if (const auto custom=columnWidths.find(key);custom!=columnWidths.end()) area.setWidth(custom->second);
            occupied+=area.getWidth();
            result.push_back({key,node["text"].toString(),area});
        }
        if (result.empty()) return result;
        const auto fixed=result.front().key=="favourite"?result.front().area.getWidth():0.f;
        const auto scale=(end-start-fixed)/std::max(1.f,occupied-fixed);
        auto x=start;
        for (auto& column:result) {
            column.area.setX(x);
            if (column.key!="favourite") column.area.setWidth(column.area.getWidth()*scale);
            x=column.area.getRight();
        }
        return result;
    }

    void layoutSortControls() {
        visibleColumns=columns();
        for (auto& widget:widgets) if (widget.name.startsWith("Sort by ")) {
            const auto label=widget.name.substring(8);
            const auto found=std::find_if(visibleColumns.begin(),visibleColumns.end(),
                [&](const BrowserColumn& col){return col.label==label;});
            if (found==visibleColumns.end()) widget.bounds={};
            else widget.bounds=found->area;
        }
    }

    void saveColumns() {
        auto* state=new juce::DynamicObject;
        auto* widths=new juce::DynamicObject;
        for (const auto& [key,value]:columnWidths) widths->setProperty(juce::Identifier(key),value);
        juce::Array<juce::var> hidden;
        for (const auto& key:hiddenColumns) hidden.add(key);
        state->setProperty("widths",juce::var(widths));
        state->setProperty("hidden",hidden);
        setBrowser("columnLayout",juce::var(state));
    }

    void resizeColumn(const String& key,int delta) {
        const auto left=std::find_if(visibleColumns.begin(),visibleColumns.end(),[&](const auto& column){return column.key==key;});
        if (left==visibleColumns.end() || std::next(left)==visibleColumns.end() || delta==0) return;
        const auto right=std::next(left);
        constexpr float minimum=56.f;
        const auto shift=juce::jlimit(std::min(0.f,minimum-left->area.getWidth()),
            std::max(0.f,right->area.getWidth()-minimum),float(delta));
        if (shift==0.f) return;
        for (const auto& column:visibleColumns) columnWidths[column.key]=column.area.getWidth();
        columnWidths[left->key]+=shift;
        columnWidths[right->key]-=shift;
        refreshColumnLayout();
        saveColumns();
    }

    void positionColumnGrips() {
        for (auto& [key,grip]:columnGrips) {
            const auto end=visibleColumns.empty()?visibleColumns.end():std::prev(visibleColumns.end());
            if (std::none_of(visibleColumns.begin(),end,[&](const auto& column){return column.key==key;}))
                grip->setVisible(false);
        }
        for (size_t i=0;i+1<visibleColumns.size();++i) {
            const auto& column=visibleColumns[i];
            if (column.key=="favourite") continue;
            auto& grip=columnGrips[column.key];
            if (!grip) {
                grip=std::make_unique<ColumnGrip>([this,key=column.key](int delta){resizeColumn(key,delta);});
                grip->setComponentID("presetColumnGrip:"+column.key);
                addChildComponent(*grip,getIndexOfChildComponent(&pickerLayer));
            }
            const auto header=column.area.toNearestIntEdges();
            grip->setBounds(header.getRight()-4,header.getY(),8,header.getHeight());
            grip->setVisible(true);
        }
    }

    void refreshColumnLayout() {
        layoutSortControls();
        for (const auto& widget:widgets) if (widget.name.startsWith("Sort by "))
            for (auto& button:buttons) if (button->getName()==widget.name) {
                button->setBounds(widget.bounds.toNearestIntEdges());
                button->setVisible(!widget.bounds.isEmpty());
            }
        positionColumnGrips();
        repaint(rectangle(regions["tableHeader"]["rect"]).getSmallestIntegerContainer());
        list.repaint();
    }

    void openColumns(const Widget& widget) {
        auto* trigger=widget.hitButton;
        juce::StringArray labels,keys;
        std::vector<bool> checks;
        const auto* input=columnLayout.getArray();
        if (!trigger || !input) return;
        for (const auto& col:*input) {
            const auto key=col["key"].toString();
            if (key=="favourite" || key=="name") continue;
            keys.add(key);
            labels.add(col["text"].toString());
            checks.push_back(!hiddenColumns.contains(key));
        }
        if (labels.isEmpty()) return;
        closePicker();
        const juce::Component::SafePointer<Canvas> self(this);
        const auto anchor=getLocalArea(trigger,trigger->getLocalBounds()).toFloat();
        picker=std::make_unique<ChoicePicker>(anchor,std::move(labels),-1,false,skin.choiceFont,[self](int){
            juce::MessageManager::callAsync([self]{if(self)self->closePicker();});
        });
        picker->allowMultiple(std::move(checks),[this,keys](int index,bool visible){
            if (visible) hiddenColumns.removeString(keys[index]);
            else hiddenColumns.addIfNotAlreadyThere(keys[index]);
            refreshColumnLayout();
            saveColumns();
        });
        showPicker(*trigger);
    }

    static void drawSortArrow(juce::Graphics& g,float x,float y,bool up) {
        const float direction=up?1.f:-1.f;
        juce::Path arrow;
        arrow.startNewSubPath(x,y-2.f*direction);
        arrow.lineTo(x+3.f,y+2.f*direction);
        arrow.lineTo(x-3.f,y+2.f*direction);
        arrow.closeSubPath();
        g.fillPath(arrow);
    }

    void paintBrowser(juce::Graphics& g) const {
        if (browser["rows"].size()>0)
            erase(g,rectangle(regions["emptyMessage"]["rect"]));
        else if (bool(browser["loading"])) drawRegion(g,"emptyMessage",String::fromUTF8("Loading library…"));

        const auto count=bool(browser["loading"])?String(String::fromUTF8("Updating library…")):
            String(browser["rows"].size())+(browser["rows"].size()==1?String::fromUTF8(" patch · "):String::fromUTF8(" patches · "))+
            (processor.isKStation()?"K-Station":"KS");
        drawRegion(g,"browserCount",count);
        drawRegion(g,"status",browser["status"].toString().isNotEmpty()?browser["status"].toString():
            "Choose a patch to load it into the selected part.");
        const auto selected=rectangle(regions["browserSelected"]["rect"]);
        g.setColour(juce::Colour(0xff101011));
        g.setFont(skin.font(12.f,700));
        g.drawFittedText(selectedPatchName,selected.withWidth(selected.getWidth()+tagFieldOffset).getSmallestIntegerContainer(),
                         juce::Justification::centredLeft,1,1.f);
        auto strip=rectangle(regions["tableHeader"]["rect"]);
        strip.setRight(rectangle(regions["tableFrame"]["rect"]).getRight()-1.f);
        if (!g.clipRegionIntersects(strip.getSmallestIntegerContainer())) return;
        g.setColour(juce::Colour(0xffbfccdc));
        g.fillRect(strip);
        g.setColour(juce::Colour(0xff91a3b6));
        g.fillRect(strip.removeFromBottom(1.f));
        const int sortColumn=int(browser["sortColumn"]);
        const auto headerFont=skin.font(13.f,600);
        g.setFont(headerFont);
        for (const auto& header:visibleColumns) {
            const auto caption=header.area.reduced(8.f,0.f);
            const auto column=browserSortColumn(header.label);
            const bool sortable=column>0;
            const bool sorted=sortable && column==sortColumn;
            g.setColour(juce::Colour(0xff18232d));
            g.drawFittedText(header.label,caption.withTrimmedRight(sortable?12.f:0.f).getSmallestIntegerContainer(),
                header.key=="favourite"?juce::Justification::centred:juce::Justification::centredLeft,1,1.f);
            if (sortable) {
                const auto arrowX=std::min(caption.getX()+headerFont.getStringWidthFloat(header.label)+7.f,
                                           caption.getRight()-3.f);
                if (sorted) drawSortArrow(g,arrowX,caption.getCentreY(),bool(browser["sortAscending"]));
                else {
                    g.setColour(juce::Colour(0xff62748a));
                    drawSortArrow(g,arrowX,caption.getCentreY()-3.f,true);
                    drawSortArrow(g,arrowX,caption.getCentreY()+3.f,false);
                }
            }
        }
    }

    void saveBrowserTags() {
        if (selectedPatch.isEmpty()) return;
        for (const auto& field:fields)
            if (field->getName()=="Selected patch tags") tags=field->getText();
        tagsEdited=false;
        send(message({{"action","tags"},{"id",selectedPatch},{"value",tags}}));
    }

    void updateBrowser(bool force=false) {
        if ((page!="Perf" && page!="Browser") || !host.browserSnapshot) return;
        auto next=host.browserSnapshot();
        if (!force && browser.getDynamicObject()==next.getDynamicObject()) return;
        const auto previous=browser;
        browser=std::move(next);
        if (page=="Perf") {
            for (const auto& widget:widgets)
                if (widget.part && widget.picker() && widget.id.isEmpty()) repaint(widget.buttonBounds());
            return;
        }

        const auto invalidate=[this](const char* name){
            repaint(rectangle(regions[name]["rect"]).getSmallestIntegerContainer());
        };
        if (force || previous["sortColumn"]!=browser["sortColumn"] || previous["sortAscending"]!=browser["sortAscending"])
            invalidate("tableHeader");
        if (force || previous["loading"]!=browser["loading"] || previous["rows"].size()!=browser["rows"].size()) {
            invalidate("browserCount");
            invalidate("emptyMessage");
        }
        if (force || previous["status"]!=browser["status"]) invalidate("status");
        for (const auto& w:widgets) {
            if (w.picker() || w.name=="Show only favourites" || w.name=="Show all bank slots" ||
                w.name=="Show init" || browserActionCaption(w.name))
                repaint(w.bounds.expanded(5.f).getSmallestIntegerContainer());
        }

        if (!force && previous["rows"].getArray()==browser["rows"].getArray()) {
            for (auto& button:buttons) if (button->getName()=="Remove source") {
                Widget widget(juce::var{});
                widget.name=button->getName();
                button->setEnabled(available(widget));
            }
            return;
        }

        int selectedRow=-1;
        juce::var selected;
        if (const auto* rows=browser["rows"].getArray())
            for (int index=0;index<rows->size();++index)
                if (bool((*rows)[index]["selected"])) {
                    selectedRow=index;
                    selected=(*rows)[index];
                    break;
                }
        const auto id=selected["id"].toString();
        const auto currentTags=selected["tags"].toString();
        const bool selectionChanged=id!=selectedPatch;
        if (selectionChanged) tagsEdited=false;
        const bool refreshTags=selectionChanged || (!tagsEdited && currentTags!=savedTags);
        if (refreshTags) tags=currentTags;
        selectedPatch=id;
        selectedPatchIsKSPlus=bool(selected["ksPlus"]);
        savedTags=currentTags;
        selectedPatchName=id.isEmpty()?"Select a patch":selected["name"].toString();

        list.updateContent();
        list.repaint();
        if (selectedRow<0) list.deselectAllRows();
        else if (list.getSelectedRow()!=selectedRow) list.selectRow(selectedRow,!selectionChanged);

        const auto nameWidth=rectangle(regions["browserSelected"]["rect"]).getWidth();
        repaint(rectangle(regions["browserSelected"]["rect"]).withWidth(250.f).getSmallestIntegerContainer());
        tagFieldOffset=juce::jlimit(nameWidth,250.f,
            std::ceil(skin.font(12.f,700).getStringWidthFloat(selectedPatchName))+2.f)-nameWidth;
        for (auto& field:fields) if (field->getName()=="Selected patch tags") {
            field->setEnabled(id.isNotEmpty());
            if (refreshTags) field->setText(tags,false);
            const auto widget=std::find_if(widgets.begin(),widgets.end(),[](const Widget& w){
                return w.name=="Selected patch tags";
            });
            if (widget!=widgets.end()) {
                field->setBounds(widget->bounds.withTrimmedLeft(tagFieldOffset).getSmallestIntegerContainer());
                repaint(widget->bounds.expanded(6.f,22.f).getSmallestIntegerContainer());
            }
        }
        for (auto& button:buttons) {
            Widget widget(juce::var{});
            widget.name=button->getName();
            if (browserActionCaption(widget.name))
                button->setEnabled(available(widget));
            if (widget.name==exportSysExCaption)
                button->setTitle(selectedPatchIsKSPlus?exportSilverKCaption:exportSysExCaption);
        }
    }

    int getNumRows() override { return browser["rows"].size(); }
    juce::var browserRow(int row) const {
        if (const auto* rows=browser["rows"].getArray()) return (*rows)[row];
        return {};
    }

    void paintListBoxItem(int row,juce::Graphics& g,int w,int h,bool) override {
        const auto item=browserRow(row);
        if (!item.isObject()) return;
        const bool selected=bool(item["selected"]);
        if (selected || row%2)
            g.fillAll(juce::Colour(selected?0x779ebbd7:0x70e2e7ee));
        g.setColour(juce::Colour(0xffaeb8c8));
        g.fillRect(0,h-1,w,1);
        for (const auto& col:visibleColumns) {
            auto cell=col.area.translated(-float(list.getX()),-col.area.getY()).withHeight(float(h));
            if (cell.getRight()<w) {
                g.setColour(juce::Colour(0x5574869b));
                g.fillRect(cell.removeFromRight(1));
            }
            if (col.key=="favourite") {
                const auto star=bool(item["favourite"]);
                juce::Path icon;
                icon.addStar(cell.getCentre().translated(4.5f,-.4f),5,4.45f,9.95f,0);
                g.setColour(juce::Colour(star?0xff225982:0xff101011));
                if (star) g.fillPath(icon);
                else g.strokePath(icon,juce::PathStrokeType(1.8f));
            } else {
                g.setFont(col.key=="name" && semibold?juce::Font(semibold).withPointHeight(13):font(13));
                g.setColour(juce::Colour(browserTextColour));
                g.drawText(makeText(item,col.key),cell.reduced(8,0).getSmallestIntegerContainer(),
                           juce::Justification::centredLeft,true);
            }
        }
    }

    void returnKeyPressed(int row) override { selectBrowserRow(row); }
    void selectBrowserRow(int row) {
        const auto item=browserRow(row);
        if (!item.isObject()) return;
        send(message({{"action","load"},{"id",item["id"]}}));
        updateBrowser();
    }

    const BrowserColumn* columnAt(const juce::MouseEvent& event) const {
        const auto x=getLocalPoint(event.eventComponent,event.getPosition()).x;
        const auto found=std::find_if(visibleColumns.begin(),visibleColumns.end(),[&](const auto& column){
            return float(x)>=column.area.getX() && float(x)<column.area.getRight();
        });
        return found==visibleColumns.end()?nullptr:&*found;
    }

    void listBoxItemClicked(int row,const juce::MouseEvent& event) override {
        const auto* column=columnAt(event);
        if (column && (column->key=="tags" || column->key=="source")) return;
        if (column && column->key=="favourite") {
            const auto item=browserRow(row);
            if (!item.isObject()) return;
            send(message({{"action","favourite"},{"id",item["id"]},{"value",!bool(item["favourite"])}}));
            updateBrowser();
        } else selectBrowserRow(row);
    }

    void listBoxItemDoubleClicked(int row,const juce::MouseEvent& event) override {
        const auto* column=columnAt(event);
        const auto item=browserRow(row);
        if (!column || !item.isObject()) return;
        if (column->key=="source") {
            if (!event.mods.isPopupMenu()) send(message({{"action","openSourceFolder"},{"id",item["id"]}}));
            return;
        }
        if (column->key!="tags") return;
        const auto rowBounds=list.getRowPosition(row,true).translated(list.getX(),list.getY());
        const auto cell=column->area.withY(float(rowBounds.getY())).withHeight(float(rowBounds.getHeight()))
            .getSmallestIntegerContainer().getIntersection(list.getBounds());
        if (cell.getHeight()<22) return;
        tagEditor=std::make_unique<juce::TextEditor>("Edit preset tags");
        auto* editor=tagEditor.get();
        editor->setComponentID("soulTrainTagCellEditor");
        editor->setBounds(cell.reduced(1));
        editor->setIndents(7,0);
        editor->setJustification(juce::Justification::centredLeft);
        editor->setFont(skin.font(12.f,500));
        editor->setColour(juce::TextEditor::backgroundColourId,juce::Colour(0xfff2f6fa));
        styleTagInput(*editor);
        editor->setColour(juce::TextEditor::focusedOutlineColourId,juce::Colour(0xff587d9e));
        editor->setText(item["tags"].toString(),false);
        const juce::Component::SafePointer<Canvas> self(this);
        const juce::Component::SafePointer<juce::TextEditor> field(editor);
        const auto finish=[self,field,id=item["id"]](bool save) {
            if (!field) return;
            const auto value=field->getText();
            juce::MessageManager::callAsync([self,field,id,value,save]{
                if (!self || !field || self->tagEditor.get()!=field.getComponent()) return;
                const auto area=field->getBounds();
                self->tagEditor.reset();
                self->repaint(area);
                if (save) {
                    self->send(message({{"action","tags"},{"id",id},{"value",value}}));
                    self->refreshBrowserSoon();
                }
            });
        };
        editor->onReturnKey=[finish]{finish(true);};
        editor->onFocusLost=[finish]{finish(true);};
        editor->onEscapeKey=[finish]{finish(false);};
        editor->onTextChange=[this,editor,prior=editor->getText().length()]()mutable{
            const auto entered=editor->getText();
            const bool forward=entered.length()>prior;
            prior=entered.length();
            if (!forward || editor->getCaretPosition()!=entered.length()) return;
            const auto prefix=entered.fromLastOccurrenceOf(",",false,false).trim();
            if (prefix.length()<2) return;
            if (const auto* available=browser["facets"].getArray()) for (const auto& entry:*available) {
                const auto option=entry.toString();
                if (!option.startsWith("Tag / ")) continue;
                const auto suggestion=option.substring(6);
                if (suggestion.length()>prefix.length() && suggestion.startsWithIgnoreCase(prefix)) {
                    const auto completion=entered+suggestion.substring(prefix.length());
                    editor->setText(completion,false);
                    editor->setHighlightedRegion({entered.length(),completion.length()});
                    break;
                }
            }
        };
        addAndMakeVisible(*editor);
        editor->grabKeyboardFocus();
        editor->moveCaretToEnd();
    }

    void timerCallback(int timer) override {
        if (!isShowing()) {
            if (noteLearnToken) closePicker();
            stopTimer(DrumEditorTimer);
            return;
        }
        if (timer==DrumEditorTimer) {refreshDrumControls();return;}
#if JUCE_WINDOWS
        view.restoreRendererIfShowing();
#endif
        processor.refreshControls();
        const bool loading=processor.isFirmwareLoading();
        if (lastLoading!=loading ||
            (!loading && (lastReady!=processor.firmwareReady() || model!=(processor.isKStation()?"K":"KS") ||
                          (page!="Perf" && (lastPart!=processor.currentPart() ||
                                             lastPerformance!=processor.performanceMode()))))) {
            rebuild();
        } else {
            if (page=="Perf") {
                refreshPerformanceSwitches();
                refreshPerformanceSelection();
            }
            if (noteLearnToken && picker) {
                if (const int note=processor.takeLearnedNote(noteLearnToken);note>=0) picker->selectIndex(note);
            }
            if (drumSequencer) { drumSequencer->refreshPlayback();return; }
            refreshDrumControls();
            // Hosts and MIDI can also change the visible controls.
            if (++browserTick%4==0 || (page=="Browser" && bool(browser["loading"]))) updateBrowser();
            for (auto& w:waves) {
                const auto waveform=raw(w.widget);
                if (w.control->value()!=waveform && w.control->setWaveform(waveform) && picker &&
                    wavePicker.getComponent()==w.control.get())
                    picker->setChosen(w.control->selectedChoice());
            }
            refreshPaintedValues();
            refreshLcd();
            if (lcdScrollChanged()) repaint(lcdGlowBounds);
#if JUCE_WINDOWS
            warmNextSection();
#endif
        }
    }
};

#if JUCE_WINDOWS
class SoulTrainJuce::OpenGLMode final : private juce::OpenGLRenderer, private juce::Timer {
public:
    explicit OpenGLMode(SoulTrainJuce& owner)
        : view(owner),safeView(&owner) {
        context.setRenderer(this);
        context.setContinuousRepainting(false);
        context.setImageCacheSize(64u*1024u*1024u);
    }

    ~OpenGLMode() override {
        stopTimer();
        context.detach();
    }

    void select(bool enabled) {
        if (enabled==selected) return;
        ++request;
        selected=enabled;
        status.clear();
        stopTimer();
        if (selected && view.isShowing() && view.getPeer()) {
            status="Starting OpenGL...";
            startTimer(2000);
            // Render the retained SoulTrain raster on the GPU without putting
            // the Canvas itself under a different component cache.
            context.attachTo(view);
        } else {
            context.detach();
            if (selected) {
                selected=false;
                status="OpenGL unavailable";
            }
        }
        view.canvas->repaint();
    }

    String label() const { return selected?"OpenGL":"Software"; }
    String description() const { return status; }

private:
    void newOpenGLContextCreated() override {
        const auto attempt=request.load();
        const auto* name=juce::gl::glGetString(juce::gl::GL_RENDERER);
        const auto adapter=name?String::fromUTF8(reinterpret_cast<const char*>(name)):String{};
        const bool hardware=adapter.isNotEmpty() && context.areShadersAvailable() &&
            !adapter.containsIgnoreCase("GDI Generic") && !adapter.containsIgnoreCase("llvmpipe") &&
            !adapter.containsIgnoreCase("softpipe") && !adapter.containsIgnoreCase("SwiftShader") &&
            !adapter.containsIgnoreCase("Software Rasterizer") && !adapter.containsIgnoreCase("Basic Render");
        juce::MessageManager::callAsync([owner=safeView,attempt,adapter,hardware]{
            if (owner && owner->openGLMode) owner->openGLMode->ready(attempt,hardware,adapter);
        });
    }

    void renderOpenGL() override {}
    void openGLContextClosing() override {}

    void ready(unsigned attempt,bool hardware,const String& adapter) {
        if (!selected || attempt!=request.load()) return;
        if (!hardware) {
            timerCallback();
            return;
        }
        stopTimer();
        status=adapter;
        view.canvas->repaint();
    }

    void timerCallback() override {
        select(false);
        status="OpenGL unavailable";
        view.canvas->repaint();
    }

    SoulTrainJuce& view;
    const juce::Component::SafePointer<SoulTrainJuce> safeView;
    juce::OpenGLContext context;
    std::atomic<unsigned> request{0};
    bool selected=false;
    String status;
};

void SoulTrainJuce::restoreRendererIfShowing() {
    // The host supplies the native peer after constructing the editor.
    if (restoreOpenGL && isShowing() && getPeer()) useOpenGL(true);
}

void SoulTrainJuce::useOpenGL(bool enabled) {
    restoreOpenGL=false;
    if (enabled && !openGLMode) openGLMode=std::make_unique<OpenGLMode>(*this);
    if (openGLMode) openGLMode->select(enabled);
}

String SoulTrainJuce::rendererLabel() const { return openGLMode?openGLMode->label():"Software"; }
String SoulTrainJuce::rendererStatus() const { return openGLMode?openGLMode->description():String{}; }
#endif

SoulTrainJuce::SoulTrainJuce(Processor& p,Callbacks callbacks,juce::File assets)
{
    setComponentID("soulTrainNative");
    canvas=std::make_unique<Canvas>(*this,p,std::move(callbacks),std::move(assets));
    scaledSurface.setSize(width,height);
    scaledSurface.addAndMakeVisible(*canvas);
    addAndMakeVisible(scaledSurface);
#if JUCE_WINDOWS
    restoreOpenGL=p.prefersOpenGLRenderer();
#endif
}

SoulTrainJuce::~SoulTrainJuce()=default;
void SoulTrainJuce::resized() {
    if (getWidth()>0 && getHeight()>0)
        scaledSurface.setTransform(juce::AffineTransform::scale(float(getWidth())/width,float(getHeight())/height));
}
bool SoulTrainJuce::showPage(const String& page) { return canvas->goTo(page); }
void SoulTrainJuce::refreshBrowser() { canvas->refreshBrowser(); }
String SoulTrainJuce::currentScene() const { return canvas->scene(); }

}
