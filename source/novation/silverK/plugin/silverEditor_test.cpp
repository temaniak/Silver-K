#include "silverBrowser.h"
#include "silverEditor.h"
#include <thread>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif
using namespace silverK;
static void check(bool value,const juce::String& message){if(!value)throw std::runtime_error(message.toStdString());}
static juce::Component* find(juce::Component& root,const juce::String& id){if(root.getComponentID()==id)return &root;for(auto* c:root.getChildren())if(auto* f=find(*c,id))return f;return nullptr;}
static void click(juce::Component& root,const juce::String& id){auto* b=dynamic_cast<juce::TextButton*>(find(root,id));check(b&&bool(b->onClick),"Missing action: "+id);b->onClick();}
static void collect(juce::Component& root,std::set<juce::String>& ids){auto id=root.getComponentID();if(id.startsWith("control:"))ids.insert(semanticId(id.substring(8)));for(auto* c:root.getChildren())collect(*c,ids);}
static juce::Component* control(juce::Component& root,Processor& p,const juce::String& id){auto* spec=p.spec(id);if(!spec)for(auto& s:p.visibleControls())if(semanticId(s.id)==id){spec=p.spec(s.id);break;}check(spec!=nullptr,"No parameter: "+id);auto* c=find(root,"control:"+juce::String(spec->id));check(c!=nullptr,"Missing control: "+id);return c;}
static juce::Slider* slider(juce::Component& root,Processor& p,const juce::String& id){auto* c=control(root,p,id);for(auto* x:c->getChildren())if(auto* s=dynamic_cast<juce::Slider*>(x))return s;throw std::runtime_error("No slider: "+id.toStdString());}
static void snapshot(juce::AudioProcessorEditor& e,const juce::String& suffix){auto path=juce::SystemStats::getEnvironmentVariable("SILVER_K_UI_CAPTURE",{});if(path.isEmpty())return;auto folder=juce::File(path);folder.createDirectory();auto file=folder.getChildFile(suffix+".png");file.deleteFile();auto stream=file.createOutputStream();juce::PNGImageFormat png;check(stream&&png.writeImageToStream(e.createComponentSnapshot(e.getLocalBounds()),*stream),"Snapshot");}
static void render(Processor& p,int blocks=100){juce::AudioBuffer<float> audio(4,256);juce::MidiBuffer midi;for(int n=0;n<blocks;++n){audio.clear();p.processBlock(audio,midi);}p.refreshControls();}
static void lighting(juce::Slider& s){
 const auto initial=s.getValue();s.setValue(s.getMinimum(),juce::dontSendNotification);auto low=s.createComponentSnapshot(s.getLocalBounds());s.setValue(s.getMaximum(),juce::dontSendNotification);auto high=s.createComponentSnapshot(s.getLocalBounds());
 const int cx=low.getWidth()/2,cy=low.getHeight()/2;const int diameter=int(s.getProperties()["rotaryDiameter"]);
 // Stay inside the grip and outside both the pointer and the LED glow/AA edge.
 for(int y=0;y<low.getHeight();++y)for(int x=0;x<low.getWidth();++x){const float dx=x+.5f-cx,dy=y+.5f-cy,radius=std::sqrt(dx*dx+dy*dy);if(radius>diameter*.34f&&radius<diameter*.38f)check(low.getPixelAt(x,y)==high.getPixelAt(x,y),"Knob grip/light moved with value");}
 for(int y=cy-9;y<cy+9;++y)for(int x=cx-9;x<cx+9;++x)if((x-cx)*(x-cx)+(y-cy)*(y-cy)<64)check(low.getPixelAt(x,y)==high.getPixelAt(x,y),"Knob lighting/material rotated with its pointer");
 auto blue=[](const juce::Image& image){int count=0;for(int y=0;y<image.getHeight();++y)for(int x=0;x<image.getWidth();++x){auto c=image.getPixelAt(x,y);count+=c.getBlue()>c.getRed()+40&&c.getGreen()>c.getRed()+40;}return count;};
 if(s.getProperties().contains("bipolarZero")){
  s.setValue(s.proportionOfLengthToValue(double(s.getProperties()["bipolarZero"])),juce::dontSendNotification);auto zero=s.createComponentSnapshot(s.getLocalBounds());
  check(blue(low)>blue(zero)+50 && blue(high)>blue(zero)+50,"Bipolar arc must grow from zero in both directions");
  const auto left=juce::Rectangle<int>(0,0,cx,low.getHeight()),right=juce::Rectangle<int>(cx,0,low.getWidth()-cx,low.getHeight());
  check(blue(low.getClippedImage(left))>blue(high.getClippedImage(left))+50,"Negative LEDs are not on the left");
  check(blue(high.getClippedImage(right))>blue(low.getClippedImage(right))+50,"Positive LEDs are not on the right");
 }else check(blue(high)>blue(low)+50,"LED arc did not follow the knob value");s.setValue(initial,juce::dontSendNotification);
}
static void knobReadout(juce::Component& main,Processor& p){
 auto* c=control(main,p,"osc1_semitone");auto* s=slider(main,p,"osc1_semitone");
 auto* label=dynamic_cast<juce::Label*>(find(*c,"caption:"+juce::String(p.spec("osc1_semitone")->id)));
 check(label && label->getText()=="Semitone" && label->isEditableOnSingleClick(),"Idle knob caption/single-click input");
 check(s->getTextBoxPosition()==juce::Slider::NoTextBox,"Knob still has a second value line");
 const auto before=p.uiParameter("osc1_semitone")->getValue();const auto now=juce::Time::getCurrentTime();
 const juce::MouseEvent down(juce::Desktop::getInstance().getMainMouseSource(),{20,20},juce::ModifierKeys::leftButtonModifier,1,0,0,0,0,s,s,now,{20,20},now,1,false);
 c->mouseDown(down);check(label->getText()==juce::String(int(std::round(p.uiParameter("osc1_semitone")->convertFrom0to1(before)))+p.spec("osc1_semitone")->displayOffset),"Click without drag did not reveal the value");
 check(p.uiParameter("osc1_semitone")->getValue()==before,"Inspecting a knob changed the sound");
 label->showEditor();check(label->getCurrentTextEditor()!=nullptr,"Caption did not open value entry");
 label->getCurrentTextEditor()->setText("-7",false);label->hideEditor(false);
 check(int(p.uiParameter("osc1_semitone")->convertFrom0to1(p.uiParameter("osc1_semitone")->getValue()))==57,"Signed manual entry lost display offset");
 label->showEditor();label->getCurrentTextEditor()->setText("invalid",false);label->hideEditor(false);
 check(int(p.uiParameter("osc1_semitone")->convertFrom0to1(p.uiParameter("osc1_semitone")->getValue()))==57,"Invalid entry changed the sound");
#if JUCE_MAC
 // Console JUCE tests have no NSApplication. Pump the CF source used by its
 // message queue so the production readout timer runs as it does in a host.
 const auto until=juce::Time::getMillisecondCounterHiRes()+1450;
 while(juce::Time::getMillisecondCounterHiRes()<until)CFRunLoopRunInMode(kCFRunLoopDefaultMode,.02,true);
 check(label->getText()=="Semitone","Knob did not restore its caption");
#endif
 p.uiParameter("osc1_semitone")->setValueNotifyingHost(before);
 for(const char* id:{"mod_attack","amp_attack"})check(slider(main,p,id)->getTextBoxPosition()!=juce::Slider::NoTextBox,"Envelope values must remain visible");
}
static void verify(Processor& p){
 std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());click(*e,"pageMain");auto* main=find(*e,"mainPanel");check(main&&main->getWidth()==1280&&main->getHeight()==1000,"Canvas dimensions");
 auto* diagnostics=find(*e,"performanceDiagnostics");check(diagnostics&&!diagnostics->isVisible(),"Diagnostics escaped Setup");
 check(find(*e,"drumEdit") && !find(*e,"drumEdit")->isVisible(),"Drum Edit exposed without a Drum Map");
 auto* demo=find(*e,"demoSong");check(demo&&!demo->isVisible(),"Demo escaped Setup");
 auto* controller=dynamic_cast<juce::ComboBox*>(find(*e,"ksController"));check(controller&&!controller->isVisible(),"Controller selector escaped Setup");
 click(*e,"pageSetup");check(diagnostics->isVisible()&&diagnostics->getParentComponent()->getLocalBounds().contains(diagnostics->getBounds()),"Diagnostics missing/outside Setup");
 check(demo->isVisible()==!p.isKStation(),"Demo visible in wrong model");check(!demo->getBounds().intersects(controller->getBounds()),"Demo/controller overlap");
 check(controller->isVisible()&&controller->getParentComponent()->getLocalBounds().contains(controller->getBounds()),"Controller missing/outside Setup");
 check(controller->isEnabled()==(ks::boardComparisonAvailable()&&!p.isKStation()) && controller->getSelectedId()==p.ksControllerMode()+1,"Controller model/selection mismatch");
 if(!ks::boardComparisonAvailable())check(controller->getNumItems()==1&&controller->getItemId(0)==2,"Production controller selector must contain only native C++");
 auto* channel=find(*e,"controlChannel");auto* buffer=find(*e,"audioBuffer");check(channel&&buffer&&!channel->getBounds().intersects(buffer->getBounds())&&!controller->getBounds().intersects(buffer->getBounds()),"Setup controls overlap");
 auto report=juce::JSON::parse(p.performanceReport());check(report["schema"].toString()=="silver-k.performance.v1"&&report["metrics"].isObject(),"Invalid diagnostics JSON");
 check(p.performanceDiagnosticsMode()==0,"Diagnostics enabled by default");
 if(!p.firmwareReady())check(!p.setPerformanceDiagnostics(1),"Recording started without firmware");
 snapshot(*e,"Diagnostics-Setup");click(*e,"pageMain");
 check(!find(*e,"pageExtra")->isVisible(),"Global Extra visible");check(!find(*e,"oscSelect1")->isVisible(),"OSC selector visible");
 for(const char* domain:{"osc1","osc2","osc3","mixer","filter","envelopes","lfo","voice","arp","fx"}){
  auto* button=dynamic_cast<juce::TextButton*>(find(*main,"extra:"+juce::String(domain)));
  const bool program=juce::String(domain)=="voice";
  check(button && button->getButtonText()==(program?"Options":"O") && button->getWidth()==(program?62:30) && button->getTooltip().isNotEmpty(),"Extra buttons must be compact O buttons; Program retains Options");
 }
 for(const char* id:{"osc1_semitone","osc2_semitone","osc3_semitone","cutoff","resonance","osc1_level","osc2_level","osc3_level","noise_level","ring_level","fm_attack","fm_decay","lfo1_rate","lfo1_delay","volume","arp_rate","arp_gate","portamento_time"}){auto* s=slider(*main,p,id);check(int(s->getProperties()["rotaryDiameter"])==72,"Knob diameter: "+juce::String(id));check(s->getProperties()["rotaryStyle"].toString()=="led","Lost native LED arc: "+juce::String(id));}
 check(control(*main,p,"osc1_semitone")->getY()==control(*main,p,"osc1_level")->getY(),"Mixer first alignment");check(control(*main,p,"osc3_semitone")->getY()==control(*main,p,"ring_level")->getY(),"Mixer last alignment");
 auto* volume=control(*main,p,"volume"),*glide=control(*main,p,"portamento_time"),*tempo=control(*main,p,"arp_rate"),*gate=control(*main,p,"arp_gate");check(glide->getY()==tempo->getY()&&volume->getY()==gate->getY()&&volume->getY()>glide->getY()&&volume->getX()==glide->getX(),"Master control order");
 check(bool(find(*main,"control:lfo1_start_phase"))==!p.isKStation(),"LFO phase model binding");check(bool(find(*main,"control:lfo2_rate"))==p.isKStation(),"Both K LFOs must be visible");check(find(*main,"lfoSelect1")->isVisible()==!p.isKStation(),"KS-only LFO selector");
 auto* performance=dynamic_cast<juce::TextButton*>(find(*e,"performanceSwitch"));check(performance&&performance->isVisible(),"Header Performance switch missing");check(performance->isEnabled()==(p.firmwareReady()&&!p.isKStation()),"Performance availability by model");
 lighting(*slider(*main,p,"osc1_semitone"));lighting(*slider(*main,p,"volume"));
 check(bool(slider(*main,p,"volume")->getProperties()["blueVolume"]),"Volume must have a blue cap");
 check(!bool(slider(*main,p,"arp_rate")->getProperties()["blueVolume"]),"Only Volume should have a blue cap");
 for(int o=1;o<=3;++o){const auto id="osc"+juce::String(o)+"_wave";auto* c=control(*main,p,id);auto* spec=p.spec(id);check(spec!=nullptr,"Wave spec");int buttons=0,menus=0;for(auto* child:c->getChildren()){buttons+=dynamic_cast<juce::TextButton*>(child)!=nullptr;menus+=dynamic_cast<juce::ComboBox*>(child)!=nullptr;}
  check(p.isKStation()?(buttons==4&&menus==0):(buttons==0&&menus==1),"Model waveform control");if(p.isKStation())for(int n=0;n<4;++n){click(*main,juce::String(spec->id)+":"+juce::String(n));check(int(p.uiParameter(id)->convertFrom0to1(p.uiParameter(id)->getValue()))==n,"Wave button parameter binding");}}
 const auto model=p.isKStation()?juce::String("K"):juce::String("KS");snapshot(*e,model+"-Main");std::set<juce::String> reachable;collect(*main,reachable);
 knobReadout(*main,p);
 for(int o=1;o<=3;++o){
  const auto osc="osc"+juce::String(o)+"_";int previous=-1;
  for(const auto& suffix:{juce::String("semitone"),juce::String("fine"),juce::String("lfo1_pitch"),juce::String("bend_depth"),juce::String("pulse_width"),juce::String("env_pitch")}){
   const auto* c=control(*main,p,osc+suffix);check(c->getX()>previous,"Oscillator knob order");previous=c->getX();
  }
  auto* pwControl=control(*main,p,osc+"pulse_width");auto* selector=find(*main,"pwmSource:"+juce::String(o)+":0");
  check(selector && selector->getY()<pwControl->getY() && selector->getX()>=pwControl->getX(),"PWM selector is not above its knob");
 }
 for(const char* d:{"osc1","osc2","osc3","mixer","filter","envelopes","lfo","voice","arp"}){click(*e,"extra:"+juce::String(d));auto* extra=find(*e,"localExtra");check(extra&&extra->isVisible(),"Extra unavailable: "+juce::String(d));collect(*extra,reachable);for(auto* child:extra->getChildren())if(child->getComponentID()!="extraBack")for(auto* c:child->getChildren())check(child->getLocalBounds().contains(c->getBounds()),"Extra overflow: "+c->getComponentID());
  check(find(*main,"fmSettings")->isVisible(),"FM hidden by Extra");if(juce::String(d)=="voice"){
   check(!find(*e,"programDisplay")->isVisible(),"Options overlaps LCD");
   auto* clock=dynamic_cast<juce::ComboBox*>(find(*extra,"clockSource"));check(clock && clock->isShowing()==extra->isShowing() && clock->getParentComponent()->isVisible(),"Clock source missing from Program Options");
   for(int n:{2,3,1}){clock->setSelectedId(n,juce::dontSendNotification);clock->onChange();check(int(p.clockSource())==n-1,"Clock source binding");}
   check(!clock->getParentComponent()->getBounds().intersects(extra->getChildren()[1]->getBounds()),"Clock row overlaps Options controls");
   snapshot(*e,model+"-Options");
  }
  if(juce::String(d)=="lfo")check(extra->getTitle().contains("LFO 1"),"LFO Extra has no numbered title");
  if(juce::String(d).startsWith("osc")){for(const char* suffix:{"pulse_width","lfo2_pwm","env_pwm"})check(!find(*extra,"control:"+juce::String(d)+"_"+suffix),"Duplicate PWM in Extra");snapshot(*e,model+"-"+d+"-Extra");}
  click(*e,"extraBack");check(find(*e,"programDisplay")->isVisible(),"LCD not restored");}
 click(*e,"fmSettings");collect(*find(*e,"localExtra"),reachable);check(find(*find(*e,"localExtra"),"control:fm_trigger")!=nullptr,"FM trigger missing");click(*e,"extraBack");
 for(int o=1;o<=3;++o){for(int n=0;n<3;++n){auto id="osc"+juce::String(o)+"_"+juce::StringArray{"pulse_width","lfo2_pwm","env_pwm"}[n];auto* param=p.uiParameter(id);check(param!=nullptr,"PWM parameter");param->setValueNotifyingHost(param->convertTo0to1(float(70+n)));click(*e,"pwmSource:"+juce::String(o)+":"+juce::String(n));collect(*main,reachable);check(int(slider(*main,p,id)->getValue())==70+n,"PWM binding reset");auto* caption=dynamic_cast<juce::Label*>(find(*control(*main,p,id),"caption:"+juce::String(p.spec(id)->id)));check(caption && caption->getText()==juce::StringArray{"PW Position","LFO 2 PWM","Env PWM"}[n],"PWM caption does not follow the source");}for(int n=0;n<3;++n){auto id="osc"+juce::String(o)+"_"+juce::StringArray{"pulse_width","lfo2_pwm","env_pwm"}[n];auto* param=p.uiParameter(id);check(int(param->convertFrom0to1(param->getValue()))==70+n,"PWM independent values lost");}}
 if(!p.isKStation()){click(*e,"lfoSelect2");for(int o=1;o<=3;++o)slider(*main,p,"osc"+juce::String(o)+"_lfo1_pitch");collect(*main,reachable);check(!find(*main,"control:lfo1_rate")&&find(*main,"control:lfo2_rate"),"Wrong LFO bindings");click(*e,"extra:lfo");check(find(*e,"localExtra")->getTitle()==juce::String("LFO 2 ")+juce::String::charToString(0x2014)+" Extra","LFO 2 Extra title");collect(*find(*e,"localExtra"),reachable);click(*e,"extraBack");click(*e,"lfoSelect1");}
 auto* fx=dynamic_cast<juce::ComboBox*>(find(*e,"effectSelect"));check(fx&&fx->getNumItems()==7,"Effect menu");for(int n=1;n<=7;++n){fx->setSelectedId(n,juce::dontSendNotification);fx->onChange();collect(*main,reachable);check(!find(*main,"extra:fx")->isVisible(),"Obsolete effect Extra");if(n==3){slider(*main,p,"chorus_wheel");control(*main,p,"chorus_start");for(const char* id:{"chorus","chorus_rate","chorus_depth","chorus_feedback","chorus_centre","chorus_wheel"})slider(*main,p,id);snapshot(*e,model+"-Chorus");}}
 for(const auto& s:p.visibleControls())if(juce::String(s.group)!="Part")check(reachable.count(semanticId(s.id))!=0,"Unreachable parameter: "+juce::String(s.id));
 std::cout<<"PASS: "<<model<<" layout, native rotary/LED arcs, PWM/LFO bindings, all sound controls reachable, Extra containment\n";
}
static void navigation(Processor& p){
 render(p);std::unique_ptr<juce::AudioProcessorEditor> e(p.createEditor());click(*e,"pageMain");auto* b=dynamic_cast<Browser*>(find(*e,"patchBrowser"));check(b!=nullptr,"Native browser");
 auto folder=p.dataFolder.getParentDirectory().getChildFile(p.isKStation()?"Navigation-K":"Navigation-KS");folder.createDirectory();auto sound=folder.getChildFile("base.syx");juce::String error;check(p.saveCurrentPatch(sound,error),error);juce::MemoryBlock saved;check(sound.loadFileAsData(saved),"Read navigation sound");sound.deleteFile();auto* pitch=p.spec("osc1_semitone");check(pitch&&pitch->patchOffset>=0,"Navigation pitch mapping");
 for(int n=0;n<2;++n){auto patch=saved;auto* bytes=static_cast<uint8_t*>(patch.getData());bytes[13+pitch->patchOffset]=uint8_t((bytes[13+pitch->patchOffset]&~pitch->patchMask)|(((72+4*n)<<pitch->patchShift)&pitch->patchMask));if(!p.isKStation()){std::fill_n(bytes+13+151,16,' ');const char* name=n?"Navigation B":"Navigation A";std::copy_n(name,12,bytes+13+151);}check(folder.getChildFile(n?"Navigation B.syx":"Navigation A.syx").replaceWithData(bytes,patch.getSize()),"Write navigation preset");}
 p.patchLibrary.addPath(folder);auto* source=dynamic_cast<juce::ComboBox*>(find(*b,"patchSource"));auto* table=dynamic_cast<juce::TableListBox*>(find(*b,"patchTable"));check(source&&table,"Navigation browser filters");bool selected=false;
 for(int n=0;n<300&&!selected;++n){b->pollLibrary();for(int i=0;i<source->getNumItems();++i)if(source->getItemText(i)==folder.getFileName()){source->setSelectedId(source->getItemId(i),juce::dontSendNotification);source->onChange();selected=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
 for(int n=0;n<300&&table->getTableListBoxModel()->getNumRows()!=2;++n){b->pollLibrary();std::this_thread::sleep_for(std::chrono::milliseconds(10));}check(table->getTableListBoxModel()->getNumRows()==2,"Navigation fixtures indexed");table->deselectAllRows();
 for(int n=0;n<4;++n){click(*e,n==3?"previousPreset":"nextPreset");render(p);const auto name=n%2?"Navigation B":"Navigation A";check(p.currentPatch==name,"Previous/Next did not load/wrap the real preset");auto* value=p.uiParameter("osc1_semitone");check(int(value->convertFrom0to1(value->getValue()))==(n%2?76:72),"Preset navigation changed only the label, not the sound parameter");}
 std::cout<<"PASS: "<<(p.isKStation()?"K":"KS")<<" Previous/Next load sound parameters, use browser source filter, and wrap both directions\n";
 if(!p.isKStation()){auto* mode=dynamic_cast<juce::TextButton*>(find(*e,"performanceSwitch"));mode->setToggleState(true,juce::dontSendNotification);mode->onClick();render(p,200);check(p.performanceMode(),"Header Performance On did not reach firmware");click(*e,"partTab2");check(p.currentPart()==1,"Part selection in Performance");check(find(*e,"partTab4")->isEnabled(),"Performance part tabs remain disabled");snapshot(*e,"KS-Performance");mode->setToggleState(false,juce::dontSendNotification);mode->onClick();render(p,200);check(!p.performanceMode()&&p.currentPart()==0,"Header Performance Off did not return to Single mode");std::cout<<"PASS: header Performance On/Off, real part selection and Single mode return\n";}
}
static void sizing(){
 Processor p;
 check(p.clockSource()==ClockSource::Host,"New instances must retain host sync by default");
 for(auto mode:{ClockSource::Internal,ClockSource::Midi,ClockSource::Host}){
  p.setClockSource(mode);juce::MemoryBlock saved;p.getStateInformation(saved);Processor restored;restored.setStateInformation(saved.getData(),int(saved.getSize()));
  check(restored.clockSource()==mode,"Clock source lost in host/Standalone state");
  auto xml=juce::AudioProcessor::getXmlFromBinary(saved.getData(),int(saved.getSize()));check(xml!=nullptr,"Clock state XML");
  xml->removeAttribute("clockSource");juce::AudioProcessor::copyXmlToBinary(*xml,saved);restored.setStateInformation(saved.getData(),int(saved.getSize()));check(restored.clockSource()==ClockSource::Host,"Old states must retain host sync");
  xml->setAttribute("clockSource",99);juce::AudioProcessor::copyXmlToBinary(*xml,saved);restored.setStateInformation(saved.getData(),int(saved.getSize()));check(restored.clockSource()==ClockSource::Host,"Invalid clock source must fall back to Host");
 }
 {
  Editor e(p);check(e.getWidth()==960&&e.getHeight()==750,"Default size must be 75%");
  for(double scale:{.5,.75,1.,.82,1.5}){e.setUiScale(scale);check(e.getWidth()==juce::roundToInt(1280*scale)&&e.getHeight()==juce::roundToInt(1000*scale),"Menu/custom scale dimensions");}
  e.setUiScale(.82);const auto bounds=e.getBounds();e.setUiScale(.1);e.setUiScale(std::nan(""));check(e.getBounds()==bounds,"Invalid custom scale changed size");
  e.setBoundsConstrained({0,0,900,900});check(std::abs(double(e.getWidth())/e.getHeight()-1.28)<.003,"Drag resize lost aspect ratio");
  e.setUiScale(.82);
 }
 {Editor e(p);check(e.getWidth()==1050&&e.getHeight()==820,"Size lost when closing/reopening editor");}
 juce::MemoryBlock state;p.getStateInformation(state);Processor restored;restored.setStateInformation(state.getData(),int(state.getSize()));
 {Editor e(restored);check(e.getWidth()==1050&&e.getHeight()==820,"Size lost in saved host/Standalone state");}
 auto xml=juce::AudioProcessor::getXmlFromBinary(state.getData(),int(state.getSize()));check(xml!=nullptr,"Sizing state XML");xml->removeAttribute("editorScale");juce::AudioProcessor::copyXmlToBinary(*xml,state);restored.setStateInformation(state.getData(),int(state.getSize()));
 {Editor e(restored);check(e.getWidth()==1050&&e.getHeight()==820,"Old state must retain the user scale preference");}
 xml->setAttribute("editorScale",99.);juce::AudioProcessor::copyXmlToBinary(*xml,state);restored.setStateInformation(state.getData(),int(state.getSize()));
 {Editor e(restored);check(e.getWidth()==960,"Corrupt state produced an oversized editor");}
 std::cout<<"PASS: scale presets/custom, invalid input, drag aspect ratio, editor reopen and host state persistence\n";
}
int main(int argc,char** argv){try{juce::ScopedJuceInitialiser_GUI gui;sizing();Processor p;p.skinChoice="Auto";verify(p);if(argc>=3){if(argc>=5){juce::String error;check(p.setFlashRom(juce::File(argv[3]),error),error);check(p.setWaveRom(juce::File(argv[4]),error),error);}p.prepareToPlay(48000,256);p.setNonRealtime(true);for(int n=1;n<=2;++n){juce::String error;check(p.loadFirmware(juce::File(argv[n]),error),error);verify(p);navigation(p);}}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
