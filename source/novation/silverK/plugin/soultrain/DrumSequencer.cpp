#include "DrumSequencer.h"

#include "SkinArt.h"

namespace soultrain {
using silverK::Processor;
namespace {

using Rect=juce::Rectangle<int>;
using Colour=juce::Colour;
using String=juce::String;
const Rect frame{48,142,1390,693};
const Rect header{49,143,1388,71};
const Rect footer{49,712,1388,122};
const Rect stepValueReadout{1351,779,68,30};
constexpr int gridX=318, gridY=262, rowHeight=44, padHeight=38;
constexpr int padWidth=63, stepGap=5, beatGap=6;
constexpr int defaultVelocity=100;
const Colour ink(0xffd9e6f3), secondary(0xffb6c7d8), displayInk(0xffa3e9ff);

using Edit = Processor::DrumGridEdit;
struct StepParameter {
    Edit::Kind kind;
    const char* label;
    int minimum, maximum;
};
constexpr std::array<StepParameter,3> stepParameters{{
    {Edit::Kind::Velocity,"Velocity",1,127},
    {Edit::Kind::Probability,"Probability",0,Edit::MaxProbability},
    {Edit::Kind::Ratchet,"Ratchet",1,Edit::MaxRatchet}
}};

String keyName(uint8_t note) {
    return juce::MidiMessage::getMidiNoteName(note,true,true,4);
}

String switchLabel(uint8_t step) {
    if (step==Edit::SwitchImmediately) return "Immediate";
    if (step==Edit::SwitchAtEnd) return "End of sequence";
    return "After step "+String(step);
}

Rect padBounds(size_t row,size_t step) {
    return {gridX+int(step)*(padWidth+stepGap)+int(step/4)*beatGap,
            gridY+int(row)*rowHeight,padWidth,padHeight};
}

void fill(juce::Graphics& g,juce::Rectangle<float> bounds,Colour top,Colour bottom) {
    g.setGradientFill({top,bounds.getTopLeft(),bottom,bounds.getBottomLeft(),false});
    g.fillRect(bounds);
}

void buttonFace(juce::Graphics& g,juce::Rectangle<float> bounds,bool display,bool selected,bool hover=false) {
    auto top=Colour(display?0xff183248:0xff445265);
    auto bottom=Colour(display?0xff223b50:0xff26303c);
    if(hover) { top=top.brighter(.12f);bottom=bottom.brighter(.12f); }
    fill(g,bounds,top,bottom);
    g.setColour(Colour(selected?0xffbfd8e9:0xff738aa0));
    g.drawRect(bounds,1.5f);
    g.setColour(Colour(display?0xff15283b:0xff242f3c));
    g.drawRect(bounds.reduced(1.5f),1.0f);
}

void closeGlyph(juce::Graphics& g,juce::Rectangle<float> bounds,Colour colour) {
    const auto c=bounds.getCentre();
    const auto d=std::min(bounds.getWidth(),bounds.getHeight())*.22f;
    g.setColour(colour);
    g.drawLine(c.x-d,c.y-d,c.x+d,c.y+d,2.4f);
    g.drawLine(c.x-d,c.y+d,c.x+d,c.y-d,2.4f);
}

void buttonLabel(juce::Graphics& g,std::unique_ptr<Caption>& glow,const String& label,
                 const juce::Font& font,juce::Rectangle<float> bounds,juce::Justification alignment,
                 ButtonHighlight highlight,float opacity) {
    const auto colour=buttonTextColour(highlight);
    if (highlight==ButtonHighlight::pressed) {
        if (!glow || glow->bounds!=bounds)
            glow=std::make_unique<Caption>(font,bounds,bounds,colour,pressedButtonTextShadow,1.f,alignment);
        glow->set(label);
        glow->draw(g,0,opacity);
    } else {
        g.setColour(colour.withMultipliedAlpha(opacity));
        g.setFont(font);
        g.drawText(label,bounds,alignment);
    }
}
}

class DrumSequencer::Choice final : public juce::Button {
public:
    enum class Style { Menu, Sequence, Close };

    Choice(const SkinArt& skin,Style style=Style::Menu)
        : juce::Button(style==Style::Close?"Close Drum Sequencer":"Choose"),skin_(skin),style_(style) {}

    void show(String label,int note=-1) {
        if(label_==label && note_==note)return;
        label_=std::move(label);note_=note;
        setTitle(note<0?label_:label_+", MIDI "+String(note_));
        repaint();
    }

    void setPlaying(bool playing) {
        if(playing_==playing)return;
        playing_=playing;
        repaint();
    }

    void paintButton(juce::Graphics& g,bool hover,bool down) override {
        const auto bounds=getLocalBounds().toFloat();
        juce::Graphics::ScopedSaveState restore(g);
        const auto opacity=isEnabled()?1.f:.45f;
        const bool selected=getToggleState();
        const bool focused=hasKeyboardFocus(false);
        const auto highlight=isEnabled()?buttonHighlightFor(selected,hover || focused,down):ButtonHighlight::none;
        const auto state=choiceState(isEnabled(),down || bool(getProperties()[choiceMenuOpenProperty]),hover || keyboardFocus_);
        if(style_==Style::Menu) {
            fill(g,bounds,Colour(0xff183248),Colour(0xff223b50));
            paintChoiceFrame(g,bounds,state,opacity);
        } else {
            paintStandardButton(g,getLocalBounds(),highlight,opacity);
            g.reduceClipRegion(getLocalBounds().reduced(3));
        }
        if(style_==Style::Close) {
            closeGlyph(g,bounds.reduced(3),buttonTextColour(highlight).withMultipliedAlpha(opacity));
            return;
        }
        if(style_==Style::Sequence) {
            buttonLabel(g,labelGlow_,label_,skin_.font(label_.length()>10?13.f:15.f,600),
                        bounds,juce::Justification::centred,highlight,opacity);
            if(playing_) {
                g.setColour(displayInk);
                g.fillRect(bounds.reduced(5,3).withHeight(2));
            }
            return;
        }
        auto textBounds=bounds.reduced(12,0);
        const auto arrow=textBounds.removeFromRight(13).getCentre();
        juce::Path indicator;
        indicator.addTriangle(arrow.x-5,arrow.y-2,arrow.x+5,arrow.y-2,arrow.x,arrow.y+3);
        g.setColour(secondary);
        g.fillPath(indicator);
        if(note_>=0) {
            const auto noteBounds=textBounds.removeFromRight(31);
            noteCaption_.set(skin_.choiceFont,String(note_),noteBounds.getWidth());
            noteCaption_.draw(g,noteBounds,juce::Justification::centredRight,choiceInk,opacity,choiceGlow(state));
            textBounds.removeFromRight(6);
        }
        menuCaption_.set(skin_.choiceFont,label_,textBounds.getWidth());
        menuCaption_.draw(g,textBounds,juce::Justification::centredLeft,choiceInk,opacity,choiceGlow(state));
    }

private:
    void focusGained(FocusChangeType cause) override {
        keyboardFocus_=cause==focusChangedByTabKey;
        juce::Button::focusGained(cause);
    }
    void focusLost(FocusChangeType cause) override {
        keyboardFocus_=false;
        juce::Button::focusLost(cause);
    }

    const SkinArt& skin_;
    const Style style_;
    String label_;
    std::unique_ptr<Caption> labelGlow_;
    ChoiceCaption menuCaption_,noteCaption_;
    int note_=-1;
    bool playing_=false,keyboardFocus_=false;
};

class DrumSequencer::Playhead final : public juce::Component {
public:
    Playhead() {
        setComponentID("drumPlayhead");
        setTitle("Playing step");
        setInterceptsMouseClicks(false,false);
    }

    void paint(juce::Graphics& g) override {
        const auto bounds=getLocalBounds();
        g.setColour(Colour(0x2574dcf8));
        g.fillRect(bounds);
        g.setColour(Colour(0x8f74dcf8));
        g.drawRect(bounds);
        g.setColour(displayInk);
        g.fillRect(bounds.withHeight(3));
    }
};

class DrumSequencer::Toggle final : public juce::Button {
public:
    Toggle(const SkinArt& skin,String label) : juce::Button(std::move(label)),skin_(skin) {
        setClickingTogglesState(true);
        setTitle(getName());
    }

    void paintButton(juce::Graphics& g,bool hover,bool down) override {
        const auto bounds=getLocalBounds().toFloat();
        const bool active=getToggleState();
        const bool rollover=isEnabled() && (hover || down || hasKeyboardFocus(false));
        const auto lens=bounds.withWidth(46.f).withSizeKeepingCentre(12.f,12.f);
        if (!skin_.paintIndicator(g,lens,active,rollover,isEnabled()?1.f:.45f))
            skin_.sprite(g,skin_.lamp(15,active),lens.getPosition().translated(0,-1.5f));
        g.setColour(ink.withMultipliedAlpha(isEnabled()?1.f:.45f));
        g.setFont(skin_.font(15,500));
        g.drawText(getName(),bounds.withTrimmedLeft(46.f),juce::Justification::centredLeft);
    }

private:
    const SkinArt& skin_;
};

class DrumSequencer::Pad final : public juce::Button {
public:
    Pad() : juce::Button("Step") { setOpaque(true); }
    std::function<void(int)> commit, preview;
    std::function<void()> remove;

    void show(int value,bool selected) {
        if(value==value_ && selected==selected_)return;
        value_=value;selected_=selected;
        setDescription(value>0?"Velocity "+String(value):String("Off"));
        repaint();
    }

    void paintButton(juce::Graphics& g,bool hover,bool down) override {
        const auto r=getLocalBounds().toFloat();
        if(!isEnabled()) {
            fill(g,r,Colour(0xff293543),Colour(0xff232d39));
            g.setColour(Colour(0xff465464));
            g.drawRect(r,1);
            return;
        }
        const bool active=value_>0;
        fill(g,r,Colour(active?0xff263849:0xff354352),Colour(active?0xff1d3244:0xff283441));
        g.setColour(Colour(selected_ || hasKeyboardFocus(false)?0xffcfe6f6:
                           active?0xff6895af:hover || down?0xff8ba6bb:0xff60768a));
        g.drawRect(r,selected_?2.0f:1.2f);
        paintButtonHighlight(g,r,buttonHighlightFor(selected_,hover || hasKeyboardFocus(false),down));
        if(active) {
            const auto inside=r.reduced(3);
            const auto level=inside.withTop(inside.getBottom()-inside.getHeight()*float(value_)/127.f);
            fill(g,level,Colour(0xff99e4f6),Colour(0xff3b7396));
            g.setColour(Colour(0x99cff5ff));
            g.fillRect(level.withHeight(std::min(level.getHeight(),1.0f)));
        }
    }

    void mouseDown(const juce::MouseEvent& event) override {
        initialVelocity_=value_?value_:defaultVelocity;
        dragged_=false;
        juce::Button::mouseDown(event);
    }
    void mouseDrag(const juce::MouseEvent& event) override {
        juce::Button::mouseDrag(event);
        if(!isEnabled())return;
        const auto distance=event.getDistanceFromDragStartY();
        if(!dragged_ && std::abs(distance)<3)return;
        dragged_=true;
        const auto next=juce::jlimit(1,127,initialVelocity_-distance*2);
        if(preview && next!=value_)preview(next);
    }
    void mouseUp(const juce::MouseEvent& event) override {
        juce::Button::mouseUp(event);
        if(dragged_ && commit)commit(value_);
        dragged_=false;
    }
    void mouseDoubleClick(const juce::MouseEvent& event) override {
        if (isEnabled() && event.mods.isLeftButtonDown() && !event.mouseWasDraggedSinceMouseDown() &&
            getLocalBounds().toFloat().contains(event.position) && remove)
            remove();
    }
    bool isAdjusting() const noexcept { return dragged_; }

private:
    int value_=0,initialVelocity_=defaultVelocity;
    bool selected_=false,dragged_=false;
};

class DrumSequencer::StepValue final : public juce::Slider {
public:
    StepValue() {
        setSliderStyle(LinearHorizontal);
        setTextBoxStyle(NoTextBox,false,0,0);
        setWantsKeyboardFocus(true);
    }
    void paint(juce::Graphics& g) override {
        const auto area=getLocalBounds().toFloat();
        const auto level=float(getPositionOfValue(getValue()));
        const auto track=area.reduced(8,0).withHeight(7).withCentre(area.getCentre());
        g.setColour(Colour(0xff172637));
        g.fillRect(track);
        g.setColour(Colour(0xff536c81));
        g.drawRect(track);
        if(isEnabled()) {
            g.setColour(Colour(0xff6ca6c4));
            g.fillRect(track.reduced(1).withRight(juce::jlimit(track.getX()+1,track.getRight()-1,level)));
            buttonFace(g,{level-6,area.getCentreY()-10,12,20},false,hasKeyboardFocus(false),isMouseOver());
        }
    }
};

DrumSequencer::DrumSequencer(Processor& processor,const SkinArt& skin,Menu menu,NoteMenu noteMenu,
                             std::function<void()> close)
    : processor_(processor),skin_(skin),menu_(std::move(menu)),noteMenu_(std::move(noteMenu)),close_(std::move(close)),
      info_(processor.drumSequencerInfo()),editingPattern_(info_.sequences.selectedPattern) {
    setComponentID("drumSequencer");
    setWantsKeyboardFocus(true);
    closeButton_=std::make_unique<Choice>(skin,Choice::Style::Close);
    closeButton_->setComponentID("closeDrumSequencer");
    closeButton_->setBounds(1384,160,39,39);
    closeButton_->onClick=close_;
    addAndMakeVisible(*closeButton_);

    retrigger_=std::make_unique<Toggle>(skin,"Retrigger");
    retrigger_->setComponentID("drumRetrigger");
    retrigger_->setDescription("With Key Sync on, restart a running sequence when a key for that sequence is played.");
    retrigger_->setBounds(747,157,158,46);
    retrigger_->onClick=[this]{
        apply(Edit::Kind::Retrigger,0,uint8_t(!info_.sequences.retrigger));
        retrigger_->setToggleState(info_.sequences.retrigger,juce::dontSendNotification);
    };
    addAndMakeVisible(*retrigger_);

    assignedKeys_=std::make_unique<Toggle>(skin,"Assigned Keys");
    assignedKeys_->setComponentID("drumAssignedKeys");
    assignedKeys_->setDescription("Use MIDI key ranges to select a sequence. When off, any key plays the selected sequence.");
    assignedKeys_->setBounds(905,157,178,46);
    assignedKeys_->onClick=[this]{
        apply(Edit::Kind::AssignedKeys,0,uint8_t(!info_.sequences.assignedKeysOnly));
        assignedKeys_->setToggleState(info_.sequences.assignedKeysOnly,juce::dontSendNotification);
    };
    addAndMakeVisible(*assignedKeys_);

    switchAfter_=std::make_unique<Choice>(skin);
    switchAfter_->setComponentID("drumSwitchAfter");
    switchAfter_->setDescription("When to change from the currently playing sequence");
    switchAfter_->setBounds(1186,162,180,choiceBoxHeight);
    switchAfter_->onClick=[this]{showSwitchAfter();};
    addAndMakeVisible(*switchAfter_);

    for(size_t row=0;row<sounds_.size();++row) {
        auto& sound=sounds_[row];
        sound=std::make_unique<Choice>(skin);
        sound->setComponentID("drumSound"+String(int(row+1)));
        sound->setBounds(69,gridY+int(row)*rowHeight+(padHeight-choiceBoxHeight)/2,235,choiceBoxHeight);
        sound->setEnabled(info_.bank!=0);
        sound->onClick=[this,row]{showSounds(row);};
        addAndMakeVisible(*sound);
        for(size_t step=0;step<Edit::Steps;++step) {
            const auto index=row*Edit::Steps+step;
            auto& pad=pads_[index];
            pad=std::make_unique<Pad>();
            pad->setComponentID("drumStep"+String(int(row+1))+":"+String(int(step+1)));
            pad->setTitle("Row "+String(int(row+1))+", step "+String(int(step+1)));
            pad->setBounds(padBounds(row,step));
            pad->onClick=[this,index]{
                if(!pads_[index]->isAdjusting()) {
                    const auto velocity=pattern().velocities[index];
                    setStepVelocity(index,velocity?velocity:defaultVelocity);
                }
            };
            pad->commit=[this,index](int value){setStepVelocity(index,value);};
            pad->preview=[this,index](int value){selectStep(index,value);};
            pad->remove=[this,index]{removeStep(index);};
            addAndMakeVisible(*pad);
        }
    }

    for(uint8_t i=0;i<patterns_.size();++i) {
        auto& choice=patterns_[i];
        choice=std::make_unique<Choice>(skin,Choice::Style::Sequence);
        choice->setComponentID("drumSequence"+String(i+1));
        choice->setBounds(188+i*102,723,96,regularButtonHeight);
        choice->onClick=[this,i]{selectPattern(i);};
        addAndMakeVisible(*choice);
    }
    lowKey_=std::make_unique<Choice>(skin);
    lowKey_->setComponentID("drumSequenceLowNote");
    lowKey_->setDescription("Lowest trigger note for this sequence");
    lowKey_->setBounds(151,776,112,choiceBoxHeight);
    lowKey_->onClick=[this]{showKeys(Edit::Kind::KeyLow);};
    addAndMakeVisible(*lowKey_);
    highKey_=std::make_unique<Choice>(skin);
    highKey_->setComponentID("drumSequenceHighNote");
    highKey_->setDescription("Highest trigger note for this sequence");
    highKey_->setBounds(358,776,112,choiceBoxHeight);
    highKey_->onClick=[this]{showKeys(Edit::Kind::KeyHigh);};
    addAndMakeVisible(*highKey_);
    length_=std::make_unique<Choice>(skin);
    length_->setComponentID("drumSequenceLength");
    length_->setBounds(535,776,78,choiceBoxHeight);
    length_->onClick=[this]{showLengths();};
    addAndMakeVisible(*length_);

    parameter_=std::make_unique<Choice>(skin);
    parameter_->setComponentID("drumStepParameter");
    parameter_->setBounds(980,776,124,choiceBoxHeight);
    parameter_->onClick=[this]{showStepParameters();};
    addAndMakeVisible(*parameter_);
    remove_=std::make_unique<Choice>(skin,Choice::Style::Close);
    remove_->setName("Remove step");
    remove_->setTitle("Remove step");
    remove_->setComponentID("drumRemoveStep");
    remove_->setBounds(1112,776,36,36);
    remove_->onClick=[this]{if(selected_>=0)removeStep(size_t(selected_));};
    addAndMakeVisible(*remove_);

    stepValue_=std::make_unique<StepValue>();
    stepValue_->setComponentID("drumStepValue");
    stepValue_->setBounds(1160,781,172,26);
    stepValue_->onDragStart=[this]{draggingValue_=true;};
    stepValue_->onDragEnd=[this]{
        draggingValue_=false;
        setParameterValue(int(stepValue_->getValue()));
    };
    stepValue_->onValueChange=[this]{
        if(selected_<0)return;
        const auto value=int(stepValue_->getValue());
        if(draggingValue_) {
            if(stepParameters[parameterIndex_].kind==Edit::Kind::Velocity)
                pads_[size_t(selected_)]->show(value,true);
            repaint(stepValueReadout);
        }else setParameterValue(value);
    };
    addAndMakeVisible(*stepValue_);
    playhead_=std::make_unique<Playhead>();
    addChildComponent(*playhead_);
    for (auto* child:getChildren())
        if (dynamic_cast<Choice*>(child) || dynamic_cast<Pad*>(child))
            buttonShadows_.push_back(skin_.buttonShadow(child->getBounds()));
    refreshPattern();
    refreshPlayback();
}

DrumSequencer::~DrumSequencer()=default;

void DrumSequencer::paint(juce::Graphics& g) {
    if(!frame.contains(g.getClipBounds())) {
        g.setColour(Colour(0x78101a28));
        g.fillRect(getLocalBounds().withTop(110).withTrimmedBottom(30));
        juce::DropShadow(Colour(0x99050d18),16,{0,7}).drawForRectangle(g,frame);
    }
    fill(g,frame.toFloat(),Colour(0xff344252),Colour(0xff24303e));
    fill(g,header.toFloat(),Colour(0xff424f60),Colour(0xff2d3948));
    g.setColour(Colour(0xff22303f));g.fillRect(footer);
    const auto shadowClip=g.getClipBounds();
    for (const auto& shadow:buttonShadows_)
        if (shadow.visibleIn(shadowClip)) shadow.paint(g);
    g.setColour(Colour(0xff8fa6ba));g.drawRect(frame,2);
    g.setColour(Colour(0xff63798e));
    g.drawHorizontalLine(header.getBottom(),float(frame.getX()+2),float(frame.getRight()-2));
    g.drawHorizontalLine(footer.getY(),float(frame.getX()+2),float(frame.getRight()-2));
    g.setColour(ink);g.setFont(skin_.font(23,600));
    g.drawText(Name,Rect{69,162,211,37},juce::Justification::centredLeft);
    g.setColour(secondary);g.setFont(skin_.font(15));
    g.drawText(info_.slot?"Part "+String(info_.slot)+"  /  Drum Map":"Drum Map",
               Rect{287,162,280,37},juce::Justification::centredLeft);
    g.drawText("Switch timing",Rect{1085,162,99,36},juce::Justification::centredLeft);
    g.drawText("Drum sound   /   MIDI note",Rect{78,229,226,33},juce::Justification::centredLeft);
    for(size_t step=0;step<Edit::Steps;++step) {
        const auto r=padBounds(0,step).withY(232).withHeight(28);
        if(!g.clipRegionIntersects(r))continue;
        const bool selected=selected_>=0 && step==size_t(selected_)%Edit::Steps;
        if(selected) {
            g.setColour(Colour(0xff203e54));g.fillRect(r);
            g.setColour(Colour(0xff6e99b1));g.drawRect(r);
        }
        g.setColour(step>=pattern().steps?Colour(0xff7d8d9f):selected?displayInk:step%4==0?ink:secondary);
        g.setFont(skin_.font(15,step%4==0?600:400));
        g.drawText(String(int(step+1)),r,juce::Justification::centred);
    }
    g.setColour(secondary);g.setFont(skin_.font(15));
    g.drawText("Sequence",Rect{69,723,111,34},juce::Justification::centredLeft);
    g.drawText("Low note",Rect{69,776,78,36},juce::Justification::centredLeft);
    g.drawText("High note",Rect{278,776,78,36},juce::Justification::centredLeft);
    g.drawText("Steps",Rect{486,776,47,36},juce::Justification::centredLeft);
    const auto summary=selected_>=0?soundName(pattern().notes[size_t(selected_)/Edit::Steps])+
        "   /   Step "+String(selected_%Edit::Steps+1):String("Click a step to edit");
    const auto status=error_.isNotEmpty()?error_:info_.bank?summary:String("Load a Drum Map for this part");
    g.drawFittedText(status,Rect{630,776,334,36},juce::Justification::centredLeft,1);
    buttonFace(g,stepValueReadout.toFloat(),true,false);
    g.setColour(displayInk);g.setFont(skin_.font(16,600));
    String value="-";
    if(stepValue_->isEnabled()) {
        value=String(int(stepValue_->getValue()));
        const auto kind=stepParameters[parameterIndex_].kind;
        if(kind==Edit::Kind::Probability)value+="%";
        if(kind==Edit::Kind::Ratchet)value=String::fromUTF8("×")+value;
    }
    g.drawText(value,stepValueReadout,juce::Justification::centred);
}

bool DrumSequencer::hitTest(int,int y) { return y>=110; }
bool DrumSequencer::keyPressed(const juce::KeyPress& key) {
    if(key!=juce::KeyPress::escapeKey)return false;
    close_();
    return true;
}
void DrumSequencer::mouseDown(const juce::MouseEvent& event) {
    if(!frame.contains(event.getPosition()))close_();
}
void DrumSequencer::focusClose() {
    if(closeButton_->isShowing() && closeButton_->getPeer())closeButton_->grabKeyboardFocus();
}

String DrumSequencer::soundName(uint8_t note) const {
    const auto& name=info_.names[note-Edit::FirstNote];
    return name.isNotEmpty()?name:"Note "+String(note);
}

void DrumSequencer::showSounds(size_t row) {
    juce::StringArray choices;
    for(int note=Edit::FirstNote;note<=Edit::LastNote;++note)
        choices.add(String(note)+"   "+soundName(uint8_t(note)));
    const SafePointer<DrumSequencer> self(this);
    menu_(*sounds_[row],std::move(choices),pattern().notes[row]-Edit::FirstNote,true,[self,row](int index){
        if(!self || index<0 || size_t(index)>=self->info_.names.size())return;
        if(self->apply(Edit::Kind::Note,uint8_t(row),uint8_t(Edit::FirstNote+index))) {
            self->updateSound(row);
            self->repaint(footer);
        }
    });
}

void DrumSequencer::showKeys(Edit::Kind kind) {
    const SafePointer<DrumSequencer> self(this);
    const auto range=info_.sequences.keyRanges[editingPattern_];
    const auto low=kind==Edit::Kind::KeyLow;
    const int current=range.assigned()?(low?range.low:range.high):-1;
    noteMenu_(*(low?lowKey_:highKey_),current,[self,kind](int note){
        const auto value=note<0?Edit::Unassigned:uint8_t(note);
        if(self && self->apply(kind,0,value))
            self->refreshOptions();
    });
}

void DrumSequencer::showSwitchAfter() {
    juce::StringArray choices{switchLabel(Edit::SwitchImmediately),switchLabel(Edit::SwitchAtEnd)};
    for (uint8_t step=1;step<=Edit::Steps;++step) choices.add(switchLabel(step));
    const auto current=info_.sequences.switchAfter;
    const auto row=current==Edit::SwitchImmediately?0:current==Edit::SwitchAtEnd?1:current+1;
    const SafePointer<DrumSequencer> self(this);
    menu_(*switchAfter_,std::move(choices),row,false,[self](int index){
        if (!self || index<0 || index>Edit::Steps+1) return;
        const auto step=index==0?Edit::SwitchImmediately:index==1?Edit::SwitchAtEnd:uint8_t(index-1);
        if (self->apply(Edit::Kind::SwitchAfter,0,step)) self->refreshOptions();
    });
}

void DrumSequencer::showLengths() {
    juce::StringArray choices;
    for(int steps=1;steps<=Edit::Steps;++steps)choices.add(String(steps));
    const SafePointer<DrumSequencer> self(this);
    menu_(*length_,std::move(choices),pattern().steps-1,false,[self](int index){
        if(self && index>=0 && index<Edit::Steps && self->apply(Edit::Kind::Length,0,uint8_t(index+1)))
            self->refreshPattern();
    });
}

void DrumSequencer::showStepParameters() {
    juce::StringArray choices;
    for(const auto& parameter:stepParameters)choices.add(parameter.label);
    const SafePointer<DrumSequencer> self(this);
    menu_(*parameter_,std::move(choices),int(parameterIndex_),false,[self](int index){
        if(!self || index<0 || size_t(index)>=stepParameters.size())return;
        self->parameterIndex_=size_t(index);
        const auto selected=self->selected_;
        self->updateStepEditor(selected<0?0:self->pattern().velocities[size_t(selected)]);
    });
}

void DrumSequencer::selectPattern(uint8_t index) {
    if(editingPattern_==index || !apply(Edit::Kind::SelectPattern,0,index))return;
    editingPattern_=index;
    selected_=-1;
    refreshPattern();
}

void DrumSequencer::refreshPattern() {
    if(selected_>=0 && selected_%Edit::Steps>=pattern().steps)selected_=-1;
    for(size_t row=0;row<sounds_.size();++row)updateSound(row);
    for(size_t index=0;index<pads_.size();++index) {
        pads_[index]->setEnabled(info_.bank && index%Edit::Steps<pattern().steps);
        pads_[index]->show(pattern().velocities[index],selected_==int(index));
    }
    refreshOptions();
    updatePlayhead();
    updateStepEditor(selected_<0?0:pattern().velocities[size_t(selected_)]);
    repaint(frame);
}

void DrumSequencer::refreshPlayback() {
    const auto next=processor_.drumPlayhead(info_.slot);
    if(!next || *next==playback_)return;
    if(next->pattern!=playback_.pattern) {
        if(playback_.pattern<patterns_.size())patterns_[playback_.pattern]->setPlaying(false);
        if(next->pattern<patterns_.size())patterns_[next->pattern]->setPlaying(true);
    }
    playback_=*next;
    updatePlayhead();
}

void DrumSequencer::updatePlayhead() {
    const bool visible=playback_.pattern==editingPattern_ && playback_.step<Edit::Steps;
    if(visible) {
        const auto top=padBounds(0,playback_.step);
        const auto bottom=padBounds(Edit::Rows-1,playback_.step).getBottom();
        playhead_->setBounds(top.withY(232).withBottom(bottom).expanded(2,1));
    }
    playhead_->setVisible(visible);
}

void DrumSequencer::refreshOptions() {
    retrigger_->setToggleState(info_.sequences.retrigger,juce::dontSendNotification);
    assignedKeys_->setToggleState(info_.sequences.assignedKeysOnly,juce::dontSendNotification);
    switchAfter_->show(switchLabel(info_.sequences.switchAfter));
    for(size_t i=0;i<patterns_.size();++i) {
        const auto range=info_.sequences.keyRanges[i];
        auto label=String(int(i+1));
        if (range.assigned()) {
            label+="  "+keyName(range.low);
            if (range.high!=range.low) label+=String::fromUTF8("–")+keyName(range.high);
        }
        patterns_[i]->show(std::move(label));
        patterns_[i]->setToggleState(i==editingPattern_,juce::dontSendNotification);
    }
    const auto range=info_.sequences.keyRanges[editingPattern_];
    lowKey_->show(range.assigned()?keyName(range.low):String("Unset"));
    highKey_->show(range.assigned()?keyName(range.high):String("Unset"));
    length_->show(String(pattern().steps));
}

void DrumSequencer::updateSound(size_t row) {
    const auto note=pattern().notes[row];
    sounds_[row]->show(soundName(note),note);
}

void DrumSequencer::selectStep(size_t index,int velocity) {
    if(selected_!=int(index)) {
        if(selected_>=0) {
            const auto previous=size_t(selected_);
            pads_[previous]->show(pattern().velocities[previous],false);
            repaint(padBounds(0,previous%Edit::Steps).withY(232).withHeight(28));
        }
        selected_=int(index);
        repaint(padBounds(0,index%Edit::Steps).withY(232).withHeight(28));
    }
    pads_[index]->show(velocity,true);
    updateStepEditor(velocity);
}

void DrumSequencer::setStepVelocity(size_t index,int velocity) {
    if(pattern().velocities[index]!=velocity)apply(Edit::Kind::Velocity,uint8_t(index),uint8_t(velocity));
    selectStep(index,pattern().velocities[index]);
}

int DrumSequencer::parameterValue(size_t index) const noexcept {
    const auto kind=stepParameters[parameterIndex_].kind;
    if(kind==Edit::Kind::Probability)return pattern().probabilities[index];
    if(kind==Edit::Kind::Ratchet)return pattern().ratchets[index];
    return pattern().velocities[index];
}

void DrumSequencer::setParameterValue(int value) {
    if(selected_<0 || !stepValue_->isEnabled())return;
    const auto index=size_t(selected_);
    if(parameterValue(index)!=value)apply(stepParameters[parameterIndex_].kind,uint8_t(index),uint8_t(value));
    selectStep(index,pattern().velocities[index]);
}

void DrumSequencer::updateStepEditor(int velocity) {
    const auto& parameter=stepParameters[parameterIndex_];
    const bool active=selected_>=0 && velocity>0;
    parameter_->show(parameter.label);
    stepValue_->setTitle(parameter.label);
    if(stepValue_->getMinimum()!=parameter.minimum || stepValue_->getMaximum()!=parameter.maximum)
        stepValue_->setRange(parameter.minimum,parameter.maximum,1);
    stepValue_->setEnabled(active);
    remove_->setEnabled(active);
    if(active) {
        const auto value=parameter.kind==Edit::Kind::Velocity?velocity:parameterValue(size_t(selected_));
        stepValue_->setValue(value,juce::dontSendNotification);
    }
    repaint(footer);
}

void DrumSequencer::removeStep(size_t index) {
    if(!pattern().velocities[index] || !apply(Edit::Kind::Remove,uint8_t(index),0))return;
    pads_[index]->show(0,false);
    if (selected_==int(index)) {
        selected_=-1;
        repaint(padBounds(0,index%Edit::Steps).withY(232).withHeight(28));
        updateStepEditor(0);
    }
}

bool DrumSequencer::apply(Edit::Kind kind,uint8_t index,uint8_t value) {
    const Edit edit{kind,info_.slot,editingPattern_,index,value};
    juce::String error;
    const bool applied=processor_.editDrumGrid(edit,error);
    if(applied)info_.sequences.apply(edit);
    setError(std::move(error));
    return applied;
}

void DrumSequencer::setError(String error) {
    if(error_==error)return;
    error_=std::move(error);
    repaint(footer);
}

}
