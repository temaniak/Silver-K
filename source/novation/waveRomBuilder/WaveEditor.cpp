#include "WaveEditor.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace waveRom
{
namespace {
const juce::Colour ink(0xff171b20),blue(0xff087fe5),paper(0xffeef2f6),dark(0xff20272e),muted(0xff637486);
void require(bool ok,const char* error){if(!ok)throw std::runtime_error(error);}
juce::String ms(double n,double rate){return juce::String(n*1000/rate,3)+" ms";}
float interpolated(const std::vector<float>& x,double p){p=std::clamp(p,0.,double(x.size()-1));size_t a=size_t(p),b=std::min(a+1,x.size()-1);return float(x[a]+(x[b]-x[a])*(p-a));}
juce::String encodeFloats(const std::vector<float>& x)
{
    juce::MemoryBlock block(x.size()*4,false);auto* out=static_cast<uint8_t*>(block.getData());
    for(auto f:x){uint32_t v;std::memcpy(&v,&f,4);for(int k=0;k<4;++k)*out++=uint8_t(v>>(8*k));}return block.toBase64Encoding();
}
std::vector<float> decodeFloats(const juce::var& encoded)
{
    juce::MemoryBlock block;require(block.fromBase64Encoding(encoded.toString()),"Invalid project audio encoding");
    require(block.getSize()%4==0&&block.getSize()<=16000000,"Project audio is too large or malformed");
    const auto* in=static_cast<const uint8_t*>(block.getData());std::vector<float> x(block.getSize()/4);
    for(auto& f:x){uint32_t v=0;for(int k=0;k<4;++k)v|=uint32_t(*in++)<<(8*k);std::memcpy(&f,&v,4);require(std::isfinite(f),"Project contains non-finite audio");}return x;
}
void label(juce::Label& l,const juce::String& text,float size=15,bool bold=false){l.setText(text,juce::dontSendNotification);l.setColour(juce::Label::textColourId,ink);l.setFont(juce::Font(size,bold?juce::Font::bold:juce::Font::plain));}
void number(juce::Slider& s,double lo,double hi,double step,int decimals){s.setSliderStyle(juce::Slider::IncDecButtons);s.setTextBoxStyle(juce::Slider::TextBoxLeft,false,108,30);s.setRange(lo,hi,step);s.setNumDecimalPlacesToDisplay(decimals);s.setColour(juce::Slider::textBoxTextColourId,ink);s.setColour(juce::Slider::textBoxBackgroundColourId,juce::Colours::white);s.setColour(juce::Slider::textBoxOutlineColourId,juce::Colour(0xffb8c6d4));}
SlotState readWav(juce::AudioFormatManager& formats,const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    require(bool(reader),"Cannot decode WAV");
    require(reader->lengthInSamples>=8&&reader->lengthInSamples<=4000000&&reader->sampleRate>=8000&&reader->sampleRate<=384000,"Use a WAV with 8..4000000 frames at 8..384 kHz");
    juce::AudioBuffer<float> audio(1,int(reader->lengthInSamples));
    require(reader->read(&audio,0,audio.getNumSamples(),0,true,false),"Cannot read WAV samples");
    SlotState state;state.source.assign(audio.getReadPointer(0),audio.getReadPointer(0)+audio.getNumSamples());state.rate=reader->sampleRate;state.sourceName=file.getFileName();return state;
}
}

void Waveform::set(const std::vector<float>* x,double rate,Selection selected,bool edit,bool isOverview)
{
    bool changed=data!=x;data=x;sampleRate=rate;selection=selected;editing=edit;overview=isOverview;seam=false;
    if(!data||data->empty()){left=0;right=1;}else if(changed||overview||right>data->size()||right<=left){left=0;right=double(data->size());}
    repaint();
}
void Waveform::setSeam(const std::vector<float>* x,double rate)
{
    seamCache.clear();if(x&&!x->empty()){auto count=std::min(size_t(80),x->size()/4);seamCache.insert(seamCache.end(),x->end()-int64_t(count),x->end());seamCache.insert(seamCache.end(),x->begin(),x->begin()+int64_t(count));}
    set(&seamCache,rate,{},false);seam=true;left=0;right=double(std::max(size_t(1),seamCache.size()));
}
void Waveform::zoom(double factor)
{
    if(!data||data->empty())return;
    double span=right-left,center=(left+right)/2;
    if(factor==0){span=selection.valid(data->size())?(selection.end-selection.begin)*4:double(data->size());center=(selection.begin+selection.end)/2;}
    else if(factor<0){span=double(data->size());center=span/2;}else span/=factor;
    span=std::clamp(span,std::min(16.,double(data->size())),double(data->size()));left=std::clamp(center-span/2,0.,double(data->size())-span);right=left+span;repaint();
}
void Waveform::navigate(double point){if(!data||data->empty())return;double span=right-left;left=std::clamp(point-span/2,0.,std::max(0.,double(data->size())-span));right=left+span;repaint();}
double Waveform::position(float x) const {return std::clamp(left+(x-12)/(std::max(1,getWidth()-24))*(right-left),left,right);}
void Waveform::paint(juce::Graphics& g)
{
    g.setColour(dark);g.fillRoundedRectangle(getLocalBounds().toFloat(),4);
    auto plot=getLocalBounds().reduced(12,8);if(!overview)plot.removeFromBottom(24);if(editing)plot.removeFromTop(24);
    if(!data||data->empty()){g.setColour(juce::Colour(0xffa5b4c2));g.setFont(16);g.drawFittedText("Load a WAV or open a ROM",plot,juce::Justification::centred,2);return;}
    auto xpos=[&](double p){return float(plot.getX()+(p-left)/(right-left)*plot.getWidth());};
    float center=plot.getCentreY(),scale=plot.getHeight()*.43f;
    g.setColour(juce::Colour(0xff354451));for(int i=0;i<=8;++i){float px=plot.getX()+plot.getWidth()*i/8.f;g.drawVerticalLine(int(px),float(plot.getY()),float(plot.getBottom()));}
    g.drawHorizontalLine(int(center),float(plot.getX()),float(plot.getRight()));
    if(selection.valid(data->size())){g.setColour(blue.withAlpha(.22f));g.fillRect(juce::Rectangle<float>(std::max(float(plot.getX()),xpos(selection.begin)),float(plot.getY()),std::max(0.f,std::min(float(plot.getRight()),xpos(selection.end))-std::max(float(plot.getX()),xpos(selection.begin))),float(plot.getHeight())));}
    const double step=(right-left)/std::max(1,plot.getWidth());
    if(step>2){for(int px=0;px<plot.getWidth();++px){size_t a=size_t(std::clamp(left+px*step,0.,double(data->size()-1))),b=std::min(data->size(),size_t(left+(px+1)*step)+1);float lo=1,hi=-1;for(size_t j=a;j<b;++j){lo=std::min(lo,(*data)[j]);hi=std::max(hi,(*data)[j]);}double p=left+px*step;g.setColour((!selection.valid(data->size())||(p>=selection.begin&&p<selection.end))?juce::Colour(0xff4cb2ff):juce::Colour(0xffa6b4c3));g.drawVerticalLine(plot.getX()+px,center-std::clamp(hi,-1.f,1.f)*scale,center-std::clamp(lo,-1.f,1.f)*scale);}}
    else {juce::Path path;for(int px=0;px<=plot.getWidth();++px){float y=center-std::clamp(interpolated(*data,left+px*step),-1.f,1.f)*scale;if(px==0)path.startNewSubPath(float(plot.getX()),y);else path.lineTo(float(plot.getX()+px),y);}g.setColour(juce::Colour(0xff4cb2ff));g.strokePath(path,juce::PathStrokeType(1.5f));}
    if(seam){g.setColour(juce::Colours::white.withAlpha(.65f));float dashes[]{4,4};g.drawDashedLine(juce::Line<float>(float(plot.getCentreX()),float(plot.getY()),float(plot.getCentreX()),float(plot.getBottom())),dashes,2);}
    if(selection.valid(data->size()))for(int end=0;end<2;++end){float px=xpos(end?selection.end:selection.begin);if(px<plot.getX()||px>plot.getRight())continue;g.setColour(blue);g.drawVerticalLine(int(px),float(plot.getY()),float(plot.getBottom()));g.fillEllipse(px-4,plot.getY()-4.f,8,8);if(editing){auto box=juce::Rectangle<float>(px-34,plot.getY()-23.f,68,20).constrainedWithin(getLocalBounds().toFloat());g.fillRoundedRectangle(box,3);g.setColour(juce::Colours::white);g.setFont(12);g.drawText(end?"End":"Start",box,juce::Justification::centred);}}
    if(!overview){g.setColour(juce::Colour(0xffbcc9d5));g.setFont(11);for(int i=0;i<=4;++i){double p=left+(right-left)*i/4;if(seam)p-=data->size()/2;juce::String t=ms(p,sampleRate);int px=plot.getX()+plot.getWidth()*i/4;g.drawText(t,std::clamp(px-38,4,std::max(4,getWidth()-80)),getHeight()-24,76,18,juce::Justification::centred);}}
}
void Waveform::mouseDown(const juce::MouseEvent& e)
{
    if(!data||data->empty())return;if(overview){if(onNavigate)onNavigate(position(e.position.x));return;}if(!editing)return;
    const auto p=position(e.position.x);dragging=std::abs(p-selection.begin)<std::abs(p-selection.end)?1:2;mouseDrag(e);
}
void Waveform::mouseDrag(const juce::MouseEvent& e)
{
    if(!dragging||!data)return;auto p=position(e.position.x);if(dragging==1)selection.begin=std::clamp(p,0.,selection.end-8);else selection.end=std::clamp(p,selection.begin+8.,double(data->size()));if(onChange)onChange(selection,false);repaint();
}
void Waveform::mouseUp(const juce::MouseEvent&){if(dragging){dragging=0;if(onChange)onChange(selection,true);}}
void Waveform::mouseWheelMove(const juce::MouseEvent& e,const juce::MouseWheelDetails& wheel){if(e.mods.isShiftDown())navigate((left+right)/2-(wheel.deltaY+wheel.deltaX)*(right-left));else zoom(wheel.deltaY>0?1.3:1/1.3);}

WaveEditor::WaveEditor()
{
    setLookAndFeel(&look);look.setColour(juce::ResizableWindow::backgroundColourId,paper);look.setColour(juce::TextButton::buttonColourId,juce::Colour(0xffe4ebf2));look.setColour(juce::TextButton::textColourOffId,ink);look.setColour(juce::ComboBox::backgroundColourId,juce::Colours::white);look.setColour(juce::ComboBox::textColourId,ink);look.setColour(juce::ToggleButton::textColourId,ink);look.setColour(juce::ToggleButton::tickColourId,blue);look.setColour(juce::Slider::thumbColourId,blue);
    for(auto* b:{&loadWav,&play,&exportBank}){b->setColour(juce::TextButton::buttonColourId,blue);b->setColour(juce::TextButton::textColourOffId,juce::Colours::white);}
    for(auto* c:std::initializer_list<juce::Component*>{&slot,&note,&slotLabel,&type,&fileName,&status,&sourceTitle,&resultTitle,&seamTitle,&processingTitle,&beginLabel,&endLabel,&periodLabel,&normalizeLabel,&fadeInLabel,&fadeOutLabel,&noteLabel,&volumeLabel,&progress,&hint,&loadWav,&openBank,&newBank,&audioSettings,&detect,&zoomIn,&zoomOut,&resetView,&play,&stop,&saveProject,&saveSlot,&exportBank,&normalize,&snap,&loop,&original,&result,&begin,&end,&peakDb,&fadeIn,&fadeOut,&gain,&overview,&editor,&output,&seam})addAndMakeVisible(c);
    label(slotLabel,"ROM Slot",17,true);label(sourceTitle,"Cycle Boundaries",21,true);label(resultTitle,"Result",21,true);label(seamTitle,"Loop Seam",18,true);label(processingTitle,"Processing",18,true);label(beginLabel,"Start");label(endLabel,"End");label(fadeInLabel,"Fade In");label(fadeOutLabel,"Fade Out");label(noteLabel,"Note");label(volumeLabel,"Volume");label(type,"");label(fileName,"");label(status,"");label(periodLabel,"");label(progress,"");label(hint,"");
    for(size_t i=0;i<slots().size();++i){if(i==0)slot.addSectionHeading("Tonal");if(i==9)slot.addSectionHeading("Rhythmic");slot.addItem(juce::String(slots()[i].index).paddedLeft('0',2)+"  "+slots()[i].name,int(i+1));}
    slot.setSelectedId(2,juce::dontSendNotification);slot.onChange=[this]{stopAudio();selected=size_t(slot.getSelectedId()-1);refresh(true);};
    for(int i=12;i<=108;++i)note.addItem(noteName(i)+"  |  "+std::to_string(i),i+1);
    note.setSelectedId(61,juce::dontSendNotification);note.onChange=[this]{if(voice.playing())audition();else refresh();};
    number(begin,0,100000, .001,3);number(end,0,100000,.001,3);number(peakDb,-60,-.2,.1,1);number(fadeIn,0,1000,.1,1);number(fadeOut,0,2000,.1,1);
    begin.setTextValueSuffix(" ms");end.setTextValueSuffix(" ms");peakDb.setTextValueSuffix(" dBFS");fadeIn.setTextValueSuffix(" ms");fadeOut.setTextValueSuffix(" ms");
    for(auto* s:{&begin,&end,&peakDb,&fadeIn,&fadeOut})s->onValueChange=[this]{if(!updating&&!busy)updateProcessing();};
    normalize.onClick=[this]{updateProcessing();};snap.setToggleState(true,juce::dontSendNotification);
    original.setRadioGroupId(7);result.setRadioGroupId(7);result.setToggleState(true,juce::dontSendNotification);original.onClick=result.onClick=[this]{if(voice.playing())audition();};loop.onClick=[this]{if(voice.playing())audition();};
    gain.setSliderStyle(juce::Slider::LinearHorizontal);gain.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);gain.setRange(0,1,.001);gain.setValue(.35);gain.onValueChange=[this]{volume.store(float(gain.getValue()),std::memory_order_relaxed);};
    loadWav.onClick=[this]{choose(0);};openBank.onClick=[this]{choose(1);};saveProject.onClick=[this]{choose(2);};exportBank.onClick=[this]{choose(3);};saveSlot.onClick=[this]{commitSlot();};detect.onClick=[this]{process(true);};play.onClick=[this]{audition();};stop.onClick=[this]{stopAudio();};zoomIn.onClick=[this]{editor.zoom(1.6);};zoomOut.onClick=[this]{editor.zoom(1/1.6);};resetView.onClick=[this]{editor.zoom(-1);};
    audioSettings.onClick=[this]{auto* selector=new juce::AudioDeviceSelectorComponent(deviceManager,0,0,1,2,false,false,true,false);selector->setSize(520,340);juce::DialogWindow::LaunchOptions options;options.content.setOwned(selector);options.dialogTitle="Audio Output";options.dialogBackgroundColour=paper;options.escapeKeyTriggersCloseButton=true;options.useNativeTitleBar=true;options.componentToCentreAround=this;options.launchAsync();};
    newBank.onClick=[this]{auto clear=[this]{stopAudio();bank=Bank();states={};loadedRom=juce::File();project=juce::File();modified=false;status.setText("New Bank",juce::dontSendNotification);if(onChanged)onChanged();refresh(true);};if(!modified){clear();return;}juce::Component::SafePointer<WaveEditor> safe(this);juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon).withTitle("New Bank").withMessage("Replace unsaved changes with a new empty bank?").withButton("New Bank").withButton("Cancel"),[safe,clear](int answer){if(safe&&answer==1)clear();});};
    editor.onChange=[this](Selection s,bool done){selectionChanged(s,done);};overview.onNavigate=[this](double p){editor.navigate(p);};
    formats.registerBasicFormats();setSize(1320,900);setAudioChannels(0,2);startTimerHz(20);refresh(true);
}
WaveEditor::~WaveEditor(){stopTimer();++previewGeneration;shutdownAudio();jobs.removeAllJobs(true,-1);voice.collect();setLookAndFeel(nullptr);}
void WaveEditor::prepareToPlay(int,double rate){deviceRate.store(rate,std::memory_order_relaxed);}
void WaveEditor::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    if(!info.buffer)return;auto* const* outputs=info.buffer->getArrayOfWritePointers();float* pointers[8]{};int channels=std::min(8,info.buffer->getNumChannels());for(int i=0;i<channels;++i)pointers[i]=outputs[i]+info.startSample;voice.render(pointers,channels,info.numSamples,volume.load(std::memory_order_relaxed));
}
void WaveEditor::timerCallback(){voice.collect();play.setButtonText(voice.playing()?"Playing...":"Play");}
void WaveEditor::paint(juce::Graphics& g){g.fillAll(paper);g.setColour(juce::Colour(0xffced7e1));for(int y:{72,136,getHeight()-130,getHeight()-66})g.drawHorizontalLine(y,0,float(getWidth()));g.drawVerticalLine(int(getWidth()*.65),152,float(getHeight()-145));}
void WaveEditor::resized()
{
    const int w=getWidth(),h=getHeight(),split=int(w*.65),rx=split+18,rw=w-rx-20,mainHeight=h-540;
    slotLabel.setBounds(20,18,126,36);slot.setBounds(150,18,280,36);type.setBounds(442,18,w-850,36);audioSettings.setBounds(w-375,18,125,36);openBank.setBounds(w-235,18,215,36);
    loadWav.setBounds(20,88,168,34);fileName.setBounds(200,88,w-610,34);detect.setBounds(w-155,88,135,34);status.setBounds(w-405,88,240,34);
    sourceTitle.setBounds(20,151,split-265,30);resetView.setBounds(split-238,151,104,28);zoomOut.setBounds(split-126,151,45,28);zoomIn.setBounds(split-74,151,45,28);
    overview.setBounds(20,193,split-40,70);editor.setBounds(20,275,split-40,mainHeight);
    const int fields=275+mainHeight+12;beginLabel.setBounds(20,fields,67,32);begin.setBounds(88,fields,176,32);endLabel.setBounds(285,fields,65,32);end.setBounds(350,fields,176,32);snap.setBounds(20,fields+43,190,28);periodLabel.setBounds(215,fields+43,split-250,28);hint.setBounds(20,fields+76,split-40,24);
    resultTitle.setBounds(rx,151,rw,30);output.setBounds(rx,193,rw,std::max(140,mainHeight/2));
    bool tonal=slots()[selected].tonal();int sy=193+std::max(140,mainHeight/2)+13;
    seamTitle.setBounds(rx,sy,rw,28);seam.setBounds(rx,sy+33,rw,std::max(70,mainHeight/4));
    int py=tonal?sy+std::max(70,mainHeight/4)+78:sy+15;processingTitle.setBounds(rx,py,rw,28);normalize.setBounds(rx,py+35,165,32);peakDb.setBounds(rx+165,py+35,std::max(120,rw-165),32);
    fadeInLabel.setBounds(rx,py+87,90,32);fadeIn.setBounds(rx+105,py+87,rw-105,32);fadeOutLabel.setBounds(rx,py+131,90,32);fadeOut.setBounds(rx+105,py+131,rw-105,32);
    const int transport=h-113;original.setBounds(20,transport,103,34);result.setBounds(125,transport,110,34);play.setBounds(242,transport,108,34);stop.setBounds(358,transport,67,34);noteLabel.setBounds(439,transport,45,34);note.setBounds(485,transport,130,34);loop.setBounds(627,transport,90,34);volumeLabel.setBounds(w-270,transport,95,34);gain.setBounds(w-170,transport,150,34);
    saveProject.setBounds(20,h-49,180,34);newBank.setBounds(211,h-49,110,34);progress.setBounds(330,h-49,w-710,34);saveSlot.setBounds(w-365,h-49,160,34);exportBank.setBounds(w-190,h-49,170,34);
}
void WaveEditor::refresh(bool resetViewport)
{
    updating=true;const auto& specification=slots()[selected];auto& s=states[selected];bool tonal=specification.tonal(),has=!s.source.empty();
    type.setText(tonal?"Tonal | Cycle":"Rhythmic | Attack and End",juce::dontSendNotification);sourceTitle.setText(tonal?"Cycle Boundaries":"Sample Boundaries",juce::dontSendNotification);
    fileName.setText(has?s.sourceName+"  |  "+juce::String(s.rate,0)+" Hz":"Choose a WAV or drop it here",juce::dontSendNotification);
    begin.setRange(0,std::max(1.,s.source.size()*1000/s.rate),.001);end.setRange(0,std::max(1.,s.source.size()*1000/s.rate),.001);
    begin.setValue(s.selection.begin*1000/s.rate,juce::dontSendNotification);end.setValue(s.selection.end*1000/s.rate,juce::dontSendNotification);normalize.setToggleState(s.processing.normalize,juce::dontSendNotification);peakDb.setValue(s.processing.peakDb,juce::dontSendNotification);fadeIn.setValue(s.processing.fadeInMs,juce::dontSendNotification);fadeOut.setValue(s.processing.fadeOutMs,juce::dontSendNotification);
    for(auto* c:std::initializer_list<juce::Component*>{&seam,&seamTitle,&snap})c->setVisible(tonal);
    for(auto* c:std::initializer_list<juce::Component*>{&fadeIn,&fadeOut,&fadeInLabel,&fadeOutLabel})c->setVisible(!tonal);
    overview.set(&s.source,s.rate,s.selection,false,true);editor.set(&s.source,s.rate,s.selection,true);
    const auto resultRate=tonal?std::max(size_t(1),s.result.size())*noteHz(note.getSelectedId()-1):(!s.result.empty()&&s.selection.valid(s.source.size())?s.result.size()*s.rate/(s.selection.end-s.selection.begin):32000)*std::pow(2.,(note.getSelectedId()-61)/12.);
    output.set(&s.result,resultRate,{},false);output.zoom(-1);seam.setSeam(&s.result,resultRate);
    periodLabel.setText(has?(tonal?"Period ":"Length ")+ms(s.selection.end-s.selection.begin,s.rate):"",juce::dontSendNotification);
    hint.setText(has?(tonal?"Seam delta "+juce::String(seamError(s.result),5)+" | Drag the boundaries to edit":"Attack and end | WAV uses the left channel"):"The ROM slot determines the editing method",juce::dontSendNotification);
    int ready=0;bool validResults=true;for(size_t i=0;i<states.size();++i){ready+=states[i].occupied;if(states[i].edited&&states[i].result.size()!=size_t(slots()[i].samples))validResults=false;}progress.setText(juce::String(ready)+" / 24 | "+juce::String(bankSize)+" bytes",juce::dontSendNotification);
    for(auto* c:std::initializer_list<juce::Component*>{&slot,&loadWav,&openBank,&newBank,&saveProject,&exportBank,&note,&original,&result,&loop})c->setEnabled(!busy);
    for(auto* c:std::initializer_list<juce::Component*>{&begin,&end,&detect,&normalize,&peakDb,&fadeIn,&fadeOut,&editor})c->setEnabled(has&&!busy);
    saveSlot.setEnabled(has&&!busy&&!s.result.empty());play.setEnabled(has&&!busy&&!s.result.empty());saveProject.setEnabled(!busy&&validResults);exportBank.setEnabled(!busy&&validResults);
    detect.setEnabled(has&&!busy);editor.setEnabled(has&&!busy);begin.setEnabled(has&&!busy);end.setEnabled(has&&!busy);
    if(resetViewport){editor.zoom(0);loop.setToggleState(tonal,juce::dontSendNotification);}updating=false;resized();repaint();
}
void WaveEditor::run(std::function<void()> work,std::function<void(const juce::String&)> finish)
{
    busy=true;status.setText("Processing...",juce::dontSendNotification);refresh();juce::Component::SafePointer<WaveEditor> safe(this);
    jobs.addJob(std::function<void()>([safe,work=std::move(work),finish=std::move(finish)]() mutable {juce::String error;try{work();}catch(const std::exception& e){error=juce::String::fromUTF8(e.what());}juce::MessageManager::callAsync([safe,error,finish=std::move(finish)]() mutable {if(!safe)return;safe->busy=false;finish(error);safe->refresh();});}));
}
void WaveEditor::markChanged(){modified=true;if(onChanged)onChanged();}
void WaveEditor::failure(const juce::String& error){status.setText("Check Input",juce::dontSendNotification);juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,"Wave ROM Builder",error);}
void WaveEditor::stopAudio(){++previewGeneration;voice.stop();}
void WaveEditor::selectionChanged(Selection chosen,bool done)
{
    auto& s=states[selected];if(!chosen.valid(s.source.size()))return;
    if(done&&snap.getToggleState()&&slots()[selected].tonal()){
        auto snapPoint=[&](double point){double best=point,distance=s.rate*.0015;for(int64_t i=std::max(int64_t(1),int64_t(point-distance));i<std::min(int64_t(s.source.size()-1),int64_t(point+distance));++i)if(s.source[size_t(i-1)]<=0&&s.source[size_t(i)]>0){double p=i-1-double(s.source[size_t(i-1)])/(s.source[size_t(i)]-s.source[size_t(i-1)]);if(std::abs(p-point)<distance){distance=std::abs(p-point);best=p;}}return best;};
        Selection snapped{snapPoint(chosen.begin),snapPoint(chosen.end),chosen.confidence};if(snapped.valid(s.source.size()))chosen=snapped;
    }
    if(done)s.selection=chosen;updating=true;begin.setValue(chosen.begin*1000/s.rate,juce::dontSendNotification);end.setValue(chosen.end*1000/s.rate,juce::dontSendNotification);updating=false;overview.set(&s.source,s.rate,chosen,false,true);
    if(done)process(false);
}
void WaveEditor::updateProcessing()
{
    if(updating||busy)return;auto& s=states[selected];if(s.source.empty())return;
    Selection choice{begin.getValue()*.001*s.rate,end.getValue()*.001*s.rate,0};
    if(!choice.valid(s.source.size())){status.setText("Select at least 8 samples",juce::dontSendNotification);return;}
    s.selection=choice;s.processing={normalize.getToggleState(),peakDb.getValue(),fadeIn.getValue(),fadeOut.getValue()};process(false);
}
void WaveEditor::process(bool find)
{
    if(busy||states[selected].source.empty())return;stopAudio();auto work=std::make_shared<SlotState>(states[selected]);auto index=selected;
    run([work,index,find]{if(find)work->selection=slots()[index].tonal()?findCycle(work->source,work->rate):findTransient(work->source,work->rate);work->result=prepare(work->source,work->rate,work->selection,slots()[index],work->processing);work->edited=true;work->occupied=true;},[this,work,index,find](const juce::String& error){if(error.isNotEmpty()){if(!find){states[index].result.clear();states[index].edited=true;markChanged();}failure(error);return;}states[index]=std::move(*work);markChanged();status.setText(find?(states[index].selection.confidence<.8?"Check Detected Boundaries":"Boundaries Found"):"Changes Applied",juce::dontSendNotification);if(find)refresh(true);});
}
void WaveEditor::audition()
{
    if(busy||states[selected].source.empty())return;const auto& s=states[selected];bool raw=original.getToggleState(),tonal=slots()[selected].tonal();auto pcm=std::make_shared<std::vector<float>>(raw?s.source:s.result);int midi=note.getSelectedId()-1;double rate=deviceRate.load();double sourceRate=raw?s.rate:(!s.selection.valid(s.source.size())?32000:s.result.size()*s.rate/(s.selection.end-s.selection.begin));bool repeating=loop.getToggleState();auto prepared=std::make_shared<std::unique_ptr<PreviewBuffer>>();auto token=++previewGeneration;
    run([pcm,prepared,raw,tonal,midi,rate,sourceRate,repeating]{*prepared=!raw&&tonal?tonalPreview(*pcm,midi,rate):samplePreview(*pcm,sourceRate,raw?60:midi,rate,repeating);if(!raw&&tonal)(*prepared)->loop=repeating;},[this,prepared,token](const juce::String& error){if(token!=previewGeneration)return;if(error.isNotEmpty()){failure(error);return;}if(!voice.publish(std::move(*prepared)))failure("Preview queue is busy. Press Play again.");else status.setText("Preview Ready",juce::dontSendNotification);});
}
bool WaveEditor::isInterestedInFileDrag(const juce::StringArray& files){return !busy&&files.size()==1&&juce::File(files[0]).hasFileExtension("wav;bin;rom;skwave");}
void WaveEditor::filesDropped(const juce::StringArray& files,int,int){if(isInterestedInFileDrag(files))open(juce::File(files[0]));}
void WaveEditor::open(const juce::File& file)
{
    if(busy)return;if(file.hasFileExtension("wav")){loadWavFile(file);return;}
    auto load=[this,file]{try{if(file.hasFileExtension("skwave"))loadProjectFile(file);else loadRomFile(file);}catch(const std::exception& e){failure(e.what());}};
    if(modified){juce::Component::SafePointer<WaveEditor> safe(this);juce::AlertWindow::showAsync(juce::MessageBoxOptions().withIconType(juce::MessageBoxIconType::QuestionIcon).withTitle("Open Another Bank?").withMessage("Unsaved changes will be replaced. Save your project first to keep them.").withButton("Open").withButton("Cancel"),[safe,load](int result){if(safe&&result==1)load();});}else load();
}
void WaveEditor::choose(int role)
{
    if(busy)return;bool save=role>=2;juce::String title=role==0?"Load WAV":role==1?"Open Wave ROM or Project":role==2?"Save Project":"Build Wave ROM";
    auto root=juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Silver-K/ROMs");
    auto initial=role==2?(project.existsAsFile()?project:root.getChildFile("my-waves.skwave")):role==3?root.getChildFile("my-wave-rom.bin"):root;
    chooser=std::make_unique<juce::FileChooser>(title,initial,role==0?"*.wav":role==1?"*.bin;*.rom;*.skwave":role==2?"*.skwave":"*.bin");
    juce::Component::SafePointer<WaveEditor> safe(this);chooser->launchAsync((save?juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::warnAboutOverwriting:juce::FileBrowserComponent::openMode)|juce::FileBrowserComponent::canSelectFiles,[safe,role](const juce::FileChooser& c){if(!safe)return;auto file=c.getResult();if(file==juce::File{})return;try{if(role<2)safe->open(file);else if(role==2)safe->saveProjectFile(file.withFileExtension("skwave"));else safe->exportRom(file.withFileExtension("bin"));}catch(const std::exception& e){safe->failure(e.what());}});
}
void WaveEditor::loadWavFile(const juce::File& file)
{
    stopAudio();auto index=selected;auto work=std::make_shared<SlotState>();auto warning=std::make_shared<juce::String>();
    run([this,file,index,work,warning]{*work=readWav(formats,file);try{work->selection=slots()[index].tonal()?findCycle(work->source,work->rate):findTransient(work->source,work->rate);}catch(const std::exception& e){*warning=juce::String::fromUTF8(e.what());work->selection={0,std::min(double(work->source.size()),work->rate*.01),0};}work->result=prepare(work->source,work->rate,work->selection,slots()[index],work->processing);work->occupied=true;work->edited=true;},[this,index,work,warning](const juce::String& error){if(error.isNotEmpty()){failure(error);return;}states[index]=std::move(*work);markChanged();status.setText(warning->isEmpty()?"Boundaries Found":"Adjust Boundaries Manually",juce::dontSendNotification);refresh(true);if(warning->isNotEmpty())failure(*warning+"\nAdjust the boundaries in the waveform. The original WAV remains in the editor.");});
}
void WaveEditor::loadRomFile(const juce::File& file)
{
    require(file.getSize()==int64_t(bankSize),"Wave ROM must contain exactly 129024 bytes");juce::MemoryBlock bytes;require(file.loadFileAsData(bytes),"Cannot read ROM");Bank next;const auto* p=static_cast<const uint8_t*>(bytes.getData());next.load({p,p+bytes.getSize()});std::array<SlotState,24> nextStates;
    for(size_t i=0;i<24;++i){auto& s=nextStates[i];s.source=next.decode(i);s.result=s.source;s.rate=slots()[i].tonal()?s.source.size()*noteHz(60):32000;s.selection={0,double(s.source.size()),1};s.processing.normalize=false;s.processing.fadeOutMs=0;s.sourceName=file.getFileName()+" | "+slots()[i].name;s.occupied=std::any_of(s.source.begin(),s.source.end(),[](float v){return v!=0;});}
    stopAudio();bank=std::move(next);states=std::move(nextStates);loadedRom=file;project=juce::File();modified=false;status.setText("ROM Decoded | Unchanged",juce::dontSendNotification);if(onChanged)onChanged();refresh(true);
}
Bank WaveEditor::composed() const {Bank next=bank;for(size_t i=0;i<24;++i)if(states[i].edited){require(states[i].result.size()==size_t(slots()[i].samples),"A slot has no valid prepared result");next.put(i,states[i].result);}return next;}
void WaveEditor::commitSlot(){if(busy||states[selected].result.empty())return;bank.put(selected,states[selected].result);states[selected].edited=false;markChanged();status.setText("Slot Saved to Current Bank",juce::dontSendNotification);}
void WaveEditor::writeAtomic(const juce::File& target,const void* bytes,size_t size)
{
    require(target.getParentDirectory().createDirectory().wasOk(),"Cannot create destination folder");juce::TemporaryFile temp(target);require(temp.getFile().replaceWithData(bytes,size)&&temp.overwriteTargetFileWithTemporary(),"Cannot write output file");
}
void WaveEditor::exportRom(const juce::File& file)
{
    require(file!=loadedRom,"Choose a new ROM filename to preserve the imported original");auto next=composed();writeAtomic(file,next.bytes().data(),next.bytes().size());bank=std::move(next);for(auto& s:states)s.edited=false;modified=false;status.setText("ROM Built | 129024 bytes",juce::dontSendNotification);if(onChanged)onChanged();
}
juce::var WaveEditor::projectData() const
{
    size_t total=0;for(const auto& s:states)total+=s.source.size();require(total<=32000000,"Project limit is 32000000 source frames; shorten source recordings or export the ROM");
    auto* root=new juce::DynamicObject;root->setProperty("schema",1);root->setProperty("selected",int(selected));root->setProperty("auditionNote",note.getSelectedId()-1);root->setProperty("auditionLoop",loop.getToggleState());root->setProperty("auditionOriginal",original.getToggleState());juce::MemoryBlock raw(bank.bytes().data(),bankSize);root->setProperty("bank",raw.toBase64Encoding());juce::Array<juce::var> entries;
    for(const auto& s:states){auto* row=new juce::DynamicObject;row->setProperty("source",encodeFloats(s.source));row->setProperty("rate",s.rate);row->setProperty("name",s.sourceName);row->setProperty("begin",s.selection.begin);row->setProperty("end",s.selection.end);row->setProperty("normalize",s.processing.normalize);row->setProperty("peakDb",s.processing.peakDb);row->setProperty("fadeIn",s.processing.fadeInMs);row->setProperty("fadeOut",s.processing.fadeOutMs);row->setProperty("occupied",s.occupied);row->setProperty("edited",s.edited);entries.add(juce::var(row));}root->setProperty("slots",entries);return juce::var(root);
}
void WaveEditor::restore(const juce::var& value)
{
    require(value.isObject()&&int(value["schema"])==1,"Unsupported project schema");juce::MemoryBlock bytes;require(bytes.fromBase64Encoding(value["bank"].toString())&&bytes.getSize()==bankSize,"Invalid project ROM");const auto* p=static_cast<const uint8_t*>(bytes.getData());Bank next;next.load({p,p+bytes.getSize()});auto* rows=value["slots"].getArray();require(rows&&rows->size()==24,"Project must contain 24 slots");std::array<SlotState,24> nextStates;size_t total=0;
    for(size_t i=0;i<24;++i){const auto& row=rows->getReference(int(i));require(row.isObject(),"Invalid project slot");auto& s=nextStates[i];s.source=decodeFloats(row["source"]);total+=s.source.size();require(total<=32000000,"Project audio exceeds memory limit");s.rate=double(row["rate"]);require(std::isfinite(s.rate)&&s.rate>=8000&&s.rate<=38400000,"Invalid project sample rate");s.selection={double(row["begin"]),double(row["end"]),0};s.processing={bool(row["normalize"]),double(row["peakDb"]),double(row["fadeIn"]),double(row["fadeOut"])};require(std::isfinite(s.processing.peakDb)&&s.processing.peakDb>=-60&&s.processing.peakDb<=-.2&&std::isfinite(s.processing.fadeInMs)&&std::isfinite(s.processing.fadeOutMs)&&s.processing.fadeInMs>=0&&s.processing.fadeInMs<=1000&&s.processing.fadeOutMs>=0&&s.processing.fadeOutMs<=2000,"Invalid project processing settings");s.sourceName=row["name"].toString();s.occupied=bool(row["occupied"]);s.edited=bool(row["edited"]);if(s.source.empty()){require(!s.edited,"Empty edited slot");continue;}require(s.selection.valid(s.source.size()),"Project selection is outside source audio");s.result=s.edited?prepare(s.source,s.rate,s.selection,slots()[i],s.processing):next.decode(i);}
    auto selection=int(value["selected"]);require(selection>=0&&selection<24,"Invalid selected project slot");int auditionNote=value.hasProperty("auditionNote")?int(value["auditionNote"]):60;require(auditionNote>=12&&auditionNote<=108,"Invalid project preview note");stopAudio();bank=std::move(next);states=std::move(nextStates);selected=size_t(selection);slot.setSelectedId(selection+1,juce::dontSendNotification);note.setSelectedId(auditionNote+1,juce::dontSendNotification);original.setToggleState(bool(value["auditionOriginal"]),juce::dontSendNotification);result.setToggleState(!original.getToggleState(),juce::dontSendNotification);loadedRom=juce::File();modified=false;if(onChanged)onChanged();refresh(true);if(value.hasProperty("auditionLoop"))loop.setToggleState(bool(value["auditionLoop"]),juce::dontSendNotification);
}
void WaveEditor::loadProjectFile(const juce::File& file){require(file.getSize()>0&&file.getSize()<=192*1024*1024,"Project is empty or too large");juce::var data;auto parsed=juce::JSON::parse(file.loadFileAsString(),data);require(parsed.wasOk(),"Cannot parse project JSON");restore(data);project=file;status.setText("Project Opened",juce::dontSendNotification);}
void WaveEditor::saveProjectFile(const juce::File& file){auto json=juce::JSON::toString(projectData());auto text=json.toRawUTF8();writeAtomic(file,text,size_t(json.getNumBytesAsUTF8()));project=file;modified=false;status.setText("Project Saved with Source Audio",juce::dontSendNotification);if(onChanged)onChanged();}

void WaveEditor::smoke(const juce::File& directory)
{
    require(directory.createDirectory().wasOk(),"Cannot create smoke directory");
    auto image=[&](const char* name){refresh(true);auto shot=createComponentSnapshot(getLocalBounds(),true,1.);juce::PNGImageFormat png;auto stream=directory.getChildFile(name).createOutputStream();require(bool(stream)&&png.writeImageToStream(shot,*stream),"Cannot write UI capture");};
    auto& tonal=states[1];tonal.source.resize(24000);tonal.rate=48000;tonal.sourceName="synthetic-harpi.wav";
    for(size_t i=0;i<tonal.source.size();++i){double p=2*3.14159265358979323846*130.81278265*i/tonal.rate;tonal.source[i]=float(.5*std::sin(p)+.18*std::sin(2*p)+.08*std::sin(3*p));}
    auto source=tonal.source;auto wavFile=directory.getChildFile("synthetic-stereo.wav");{juce::AudioBuffer<float> stereo(2,int(source.size()));for(int i=0;i<stereo.getNumSamples();++i){stereo.setSample(0,i,source[size_t(i)]);stereo.setSample(1,i,-source[size_t(i)]);}juce::WavAudioFormat wav;auto stream=wavFile.createOutputStream();require(bool(stream),"Cannot write synthetic WAV");std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.release(),48000,2,24,{},0));require(bool(writer)&&writer->writeFromAudioSampleBuffer(stereo,0,stereo.getNumSamples()),"Cannot encode synthetic WAV");}
    tonal=readWav(formats,wavFile);require(tonal.source.size()==source.size()&&std::abs(tonal.source[123]-source[123])<2e-7,"WAV reader changed left-channel samples or mixed antiphase channels");tonal.sourceName="synthetic-harpi.wav";
    tonal.selection=findCycle(tonal.source,tonal.rate);tonal.result=prepare(tonal.source,tonal.rate,tonal.selection,slots()[1],tonal.processing);tonal.occupied=tonal.edited=true;selected=1;slot.setSelectedId(2,juce::dontSendNotification);status.setText("Cycle Found Automatically",juce::dontSendNotification);image("harpi.png");
    auto& drum=states[12];drum.rate=48000;drum.source.resize(24000);drum.sourceName="synthetic-snare.wav";uint32_t seed=7;for(size_t i=1440;i<drum.source.size();++i){seed=seed*1664525+1013904223;drum.source[i]=float((double(seed)/4294967296.*2-1)*std::exp(-double(i-1440)/1800)*.7);}drum.selection=findTransient(drum.source,drum.rate);drum.processing.fadeInMs=.3;drum.processing.fadeOutMs=8;drum.result=prepare(drum.source,drum.rate,drum.selection,slots()[12],drum.processing);drum.occupied=drum.edited=true;selected=12;slot.setSelectedId(13,juce::dontSendNotification);image("snare.png");
    auto expected=composed().bytes();note.setSelectedId(70,juce::dontSendNotification);auto path=directory.getChildFile("roundtrip.skwave");saveProjectFile(path);loadProjectFile(path);require(note.getSelectedId()==70&&composed().bytes()==expected,"Project roundtrip changed ROM or note");
    auto invalid=projectData();invalid.getDynamicObject()->setProperty("schema",999);bool rejected=false;try{restore(invalid);}catch(const std::exception&){rejected=true;}require(rejected&&composed().bytes()==expected,"Invalid project replaced working data");
    auto rom=directory.getChildFile("roundtrip.bin");exportRom(rom);loadRomFile(rom);require(composed().bytes()==expected,"ROM import/export changed bytes");image("decoded-rom.png");exportRom(directory.getChildFile("roundtrip-copy.bin"));
    require(directory.getChildFile("roundtrip.bin").hasIdenticalContentTo(directory.getChildFile("roundtrip-copy.bin")),"Exported ROM differs after reopening");modified=false;
    juce::String report="{\"status\":\"PASS\",\"wav_left_channel\":true,\"project_audio_and_note_roundtrip\":true,\"reject_invalid_project_without_data_loss\":true,\"rom_byte_exact_roundtrip\":true,\"screens\":[\"harpi.png\",\"snare.png\",\"decoded-rom.png\"]}\n";writeAtomic(directory.getChildFile("verification.json"),report.toRawUTF8(),size_t(report.getNumBytesAsUTF8()));
}
}
