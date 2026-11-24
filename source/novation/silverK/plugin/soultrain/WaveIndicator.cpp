#include "WaveIndicator.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace soultrain
{
namespace
{
using juce::String;
using Rect = juce::Rectangle<float>;

Rect rectangle(const juce::var& value)
{
    return value.size()==4?Rect{float(value[0]),float(value[1]),float(value[2]),float(value[3])}:Rect{};
}

WaveIndicator::Layer readLayer(const juce::File& directory, const juce::var& value)
{
    const auto image = juce::ImageFileFormat::loadFrom(directory.getChildFile(value["file"].toString()));
    if (!image.isValid() || !image.hasAlphaChannel())
        throw std::runtime_error("Invalid SoulTrain oscillator artwork");
    return {image,{float(value["x"]),float(value["y"])}};
}

Rect captionBounds(const WaveIndicator::Artwork& art,const WaveIndicator::Glyph* glyph)
{
    const auto x=art.label.x+(glyph?glyph->advance:0.f);
    return {x,art.label.y,art.arrows[0].getX()-x,art.lineHeight};
}

int prefixMatch(const WaveIndicator::Choices& choices, const String& text)
{
    const auto option = std::find_if(choices.begin(),choices.end(),[&](const auto& choice){
        return choice.title.startsWithIgnoreCase(text);
    });
    return option == choices.end() ? -1 : int(option-choices.begin());
}
}

WaveIndicator::Artwork WaveIndicator::Artwork::load(const juce::File& directory,const juce::Font& font)
{
    const auto json = juce::JSON::parse(directory.getChildFile("wave-artwork.json"));
    const auto arrows = json["arrows"];
    const auto label = json["label"];
    if (arrows.size()!=2 || label.size()!=3)
        throw std::runtime_error("Missing or incomplete SoulTrain oscillator artwork");
    Artwork art;
    art.width = float(json["width"]);
    art.height = float(json["height"]);
    art.pixelScale = float(json["artScale"]);
    art.face = rectangle(json["control"]);
    art.trigger = rectangle(json["trigger"]);
    art.arrows = {rectangle(arrows[0]),rectangle(arrows[1])};
    art.label = {float(label[0]),float(label[1])};
    art.lineHeight = float(label[2]);
    const auto* entries = json["glyphs"].getArray();
    if (art.width<=0 || art.height<=0 || art.pixelScale<=0 || art.face.isEmpty()
        || !entries)
        throw std::runtime_error("Incomplete SoulTrain oscillator artwork");
    art.frame=readLayer(directory,json["frame"]);
    for (const auto& item:*entries)
        art.glyphs.push_back({int(item["raw"]),readLayer(directory,item),float(item["advance"])});
    for (const auto* model:{"K","KS"})
    {
        const auto* list=json["models"][model].getArray();
        if (!list || list->isEmpty()) throw std::runtime_error("Missing SoulTrain oscillator choices");
        auto& choices=art.models[model];
        choices.reserve(size_t(list->size()));
        for (const auto& item:*list)
        {
            const auto value=int(item["raw"]);
            const auto glyph=std::find_if(art.glyphs.begin(),art.glyphs.end(),
                [value](const auto& candidate){return candidate.value==value;});
            const auto name=item["name"].toString();
            const auto box=captionBounds(art,glyph==art.glyphs.end()?nullptr:&*glyph);
            auto& choice=choices.emplace_back(Choice{value,name,item["title"].toString(),{}});
            choice.caption.set(font,name,box.getWidth());
        }
    }
    return art;
}

const WaveIndicator::Choices& WaveIndicator::Artwork::choicesFor(const String& model) const
{
    return models.at(model);
}

class WaveIndicator::Input final : public juce::Button
{
public:
    explicit Input(const String& name) : Button(name) { setWantsKeyboardFocus(true); }
    void paintButton(juce::Graphics&,bool,bool) override {}
    bool keyPressed(const juce::KeyPress& key) override
    {
        return (onKey && onKey(key)) || Button::keyPressed(key);
    }
    std::function<bool(const juce::KeyPress&)> onKey;
};

WaveIndicator::WaveIndicator(const Artwork& artwork, const Choices& available, unsigned oscillator, int raw)
    : art(artwork), choices(available)
{
    if (choices.empty()) throw std::invalid_argument("A waveform selector needs options");
    const auto stem="OSC "+String(oscillator);
    setName(stem+" Waveform");
    trigger=std::make_unique<Input>(getName());
    previous=std::make_unique<Input>(stem+" Previous waveform");
    next=std::make_unique<Input>(stem+" Next waveform");
    trigger->setComponentID("soultrain-wave-trigger");
    previous->setComponentID("soultrain-wave-previous");
    next->setComponentID("soultrain-wave-next");
    previous->onClick=[this]{step(-1,true);};
    next->onClick=[this]{step(1,true);};
    trigger->onClick=[this]{requestMenu();};
    trigger->onStateChange=previous->onStateChange=next->onStateChange=[this]{repaint();};
    previous->onKey=next->onKey=[this](const auto& key){return handleKey(key,true);};
    trigger->onKey=[this](const auto& key){return handleKey(key,false);};
    addAndMakeVisible(*trigger);
    addAndMakeVisible(*previous);
    addAndMakeVisible(*next);
    if (!setWaveform(raw)) throw std::invalid_argument("Unsupported initial waveform");
}

WaveIndicator::~WaveIndicator()=default;

int WaveIndicator::value() const noexcept {return choices[selected].value;}
String WaveIndicator::readout() const {return choices[selected].name;}

bool WaveIndicator::setWaveform(int raw)
{
    const auto position=std::find_if(choices.begin(),choices.end(),[raw](const auto& item){return item.value==raw;});
    if (position==choices.end()) return false;
    selected=size_t(position-choices.begin());
    refreshCaption();
    return true;
}

void WaveIndicator::selectWaveform(size_t index)
{
    if (index>=choices.size() || index==selected) return;
    selected=index;
    refreshCaption();
    if (auto callback=onWaveformChange) callback(value());
}

void WaveIndicator::step(int increment,bool wrap)
{
    const auto count=int(choices.size());
    const auto candidate=int(selected)+increment;
    selectWaveform(size_t(wrap?(candidate+count)%count:juce::jlimit(0,count-1,candidate)));
}

void WaveIndicator::refreshCaption()
{
    const auto selectedGlyph=std::find_if(art.glyphs.begin(),art.glyphs.end(),
        [this](const auto& item){return item.value==value();});
    glyph=selectedGlyph==art.glyphs.end()?nullptr:&*selectedGlyph;
    trigger->setDescription(choices[selected].title);
    trigger->setTooltip(choices[selected].title);
    repaint();
}

bool WaveIndicator::handleKey(const juce::KeyPress& key,bool arrow)
{
    const auto code=key.getKeyCode();
    if (code==juce::KeyPress::upKey || code==juce::KeyPress::leftKey) step(-1,arrow);
    else if (code==juce::KeyPress::downKey || code==juce::KeyPress::rightKey) step(1,arrow);
    else if (!arrow && code==juce::KeyPress::homeKey) selectWaveform(0);
    else if (!arrow && code==juce::KeyPress::endKey) selectWaveform(choices.size()-1);
    else if (!arrow && (code==juce::KeyPress::returnKey || code==juce::KeyPress::spaceKey)) requestMenu();
    else if (!arrow && key.getTextCharacter()>=32 && !key.getModifiers().isCommandDown()
             && !key.getModifiers().isAltDown())
    {
        const auto now=juce::Time::getMillisecondCounter();
        if (now-lastTyped>700) typeahead.clear();
        lastTyped=now;
        typeahead+=String::charToString(key.getTextCharacter());
        if (const auto match=prefixMatch(choices,typeahead);match>=0) selectWaveform(size_t(match));
    }
    else return false;
    return true;
}

void WaveIndicator::requestMenu()
{
    if (onMenuRequested) onMenuRequested(*trigger,choices,int(selected));
}

void WaveIndicator::setMenuOpen(bool open)
{
    if (menuOpen==open) return;
    menuOpen=open;
    repaint();
}

juce::AffineTransform WaveIndicator::transform() const noexcept
{
    const auto scale=std::min(float(getWidth())/art.width,float(getHeight())/art.height);
    return juce::AffineTransform::scale(scale).translated((float(getWidth())-art.width*scale)*.5f,
                                                         (float(getHeight())-art.height*scale)*.5f);
}

void WaveIndicator::drawLayer(juce::Graphics& g,const Layer& layer) const
{
    g.setOpacity(1);
    g.drawImageTransformed(layer.image,juce::AffineTransform::scale(1/art.pixelScale).translated(layer.origin));
}

void WaveIndicator::paint(juce::Graphics& g)
{
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(transform());
    drawLayer(g,art.frame);
    const auto state=choiceState(isEnabled(),menuOpen || trigger->isDown() || previous->isDown() || next->isDown(),
                                 isMouseOver(true) || keyboardFocus);
    if (glyph) drawLayer(g,glyph->art);
    choices[selected].caption.draw(g,captionBounds(art,glyph),juce::Justification::centredLeft,
                                   choiceInk,1.f,choiceGlow(state));
    paintChoiceFrame(g,art.face,state);
}

void WaveIndicator::resized()
{
    const auto layout=transform();
    trigger->setBounds(art.trigger.transformedBy(layout).getSmallestIntegerContainer());
    previous->setBounds(art.arrows[0].transformedBy(layout).getSmallestIntegerContainer());
    next->setBounds(art.arrows[1].transformedBy(layout).getSmallestIntegerContainer());
}

bool WaveIndicator::hitTest(int x,int y)
{
    return art.face.contains(juce::Point<float>(float(x),float(y)).transformedBy(transform().inverted()));
}

void WaveIndicator::focusOfChildComponentChanged(FocusChangeType cause)
{
    keyboardFocus=cause==focusChangedByTabKey && hasKeyboardFocus(true);
    repaint();
}

void WaveIndicator::mouseEnter(const juce::MouseEvent&) {repaint();}
void WaveIndicator::mouseExit(const juce::MouseEvent&) {repaint();}
}
