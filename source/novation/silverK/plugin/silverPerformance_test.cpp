#include "silverPerformance.h"
#include "silverBrowser.h"
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace silverK;
static void check(bool v,const juce::String& why){if(!v)throw std::runtime_error(why.toStdString());}
static juce::Component* find(juce::Component& c,const juce::String& id){if(c.getComponentID()==id)return &c;for(auto* child:c.getChildren())if(auto* v=find(*child,id))return v;return nullptr;}
static juce::TextButton& button(juce::Component& c,const juce::String& id){auto* b=dynamic_cast<juce::TextButton*>(find(c,id));check(b&&bool(b->onClick),"Missing button: "+id);return *b;}
static void click(juce::Component& c,const juce::String& id){button(c,id).onClick();}
static int raw(Processor& p,unsigned part,const char* id){auto* v=p.parameters.getRawParameterValue((part?"part"+juce::String(part+1)+"_":juce::String{})+id);check(v,"Parameter missing");return int(v->load());}
static void render(Processor& p,const juce::MidiBuffer& input={},int blocks=80){juce::AudioBuffer<float> audio(4,256);for(int n=0;n<blocks;++n){juce::MidiBuffer midi;if(!n)midi=input;audio.clear();p.processBlock(audio,midi);}p.refreshControls();}
static void shot(juce::AudioProcessorEditor& e,const juce::String& name){auto path=juce::SystemStats::getEnvironmentVariable("SILVER_K_UI_CAPTURE",{});if(path.isEmpty())return;auto folder=juce::File(path);folder.createDirectory();auto file=folder.getChildFile(name+".png");auto stream=file.createOutputStream();juce::PNGImageFormat png;check(stream&&png.writeImageToStream(e.createComponentSnapshot(e.getLocalBounds()),*stream),"Snapshot");}
static void midiTests(){MidiNoteLearn learn;auto token=learn.arm();const uint8_t ignore[][3]{{0x90,60,0},{0x80,60,100},{0xb0,64,127},{0xf8,0,0},{0x90,128,100}};for(const auto& m:ignore)learn.observe(m,3);check(learn.take(token)==-1,"Non-Note On learned");const uint8_t first[]{0x9f,0,1},second[]{0x90,127,127};learn.observe(first,3);learn.observe(second,3);check(learn.take(token)==0&&learn.take(token)==-1,"Capture must be one-shot including note 0");token=learn.arm();learn.cancel(token);learn.observe(second,3);check(learn.take(token)==-1,"Cancelled target changed");const auto old=learn.arm(),next=learn.arm();learn.cancel(old);learn.observe(second,3);check(learn.take(old)==-1&&learn.take(next)==127,"Stale target/cancel affected new target");token=learn.arm();learn.observe(first,3);learn.cancel(token);check(learn.take(token)==-1,"Cancel after capture failed");std::cout<<"PASS: lock-free MIDI Learn, raw range, any channel, invalid/zero/release/CC filtering, one-shot and stale cancellation\n";}
static void programs(Processor& p,juce::AudioProcessorEditor& editor,PerformancePanel& panel){
 auto* browser=dynamic_cast<Browser*>(find(editor,"patchBrowser"));check(browser,"Browser missing");const auto factory=p.presets();check(!factory.empty(),"Factory fixture missing");
 const auto folder=p.dataFolder.getParentDirectory().getChildFile("Performance-navigation");folder.createDirectory();
 for(int n=0;n<2;++n){auto data=factory.front().data;data[13+7]=uint8_t(72+n*4);std::fill_n(data.begin()+13+151,16,' ');const char* name=n?"Perf Test B":"Perf Test A";std::copy_n(name,11,data.begin()+13+151);check(folder.getChildFile(n?"B.syx":"A.syx").replaceWithData(data.data(),data.size()),"Write preset fixture");}
 p.patchLibrary.addPath(folder);auto* source=dynamic_cast<juce::ComboBox*>(find(*browser,"patchSource"));check(source,"Source filter missing");bool selected=false;
 for(int t=0;t<300&&!selected;++t){browser->pollLibrary();for(int i=0;i<source->getNumItems();++i)if(source->getItemText(i)==folder.getFileName()){source->setSelectedId(source->getItemId(i),juce::dontSendNotification);source->onChange();selected=true;break;}std::this_thread::sleep_for(std::chrono::milliseconds(10));}
 for(int t=0;t<300&&browser->filteredPrograms().size()!=2;++t){browser->pollLibrary();std::this_thread::sleep_for(std::chrono::milliseconds(10));}check(browser->filteredPrograms().size()==2,"Filtered program list not indexed");panel.refresh();
 const auto selectedPart=p.currentPart();
 for(unsigned part=0;part<4;++part){std::array<int,4> before{};for(unsigned n=0;n<4;++n)before[n]=raw(p,n,"osc1_semitone");click(panel,"performance:part"+juce::String(part+1)+":next");render(p);panel.refresh();check(raw(p,part,"osc1_semitone")==72,"Part Next did not load Program A");for(unsigned n=0;n<4;++n)if(n!=part)check(raw(p,n,"osc1_semitone")==before[n],"Program loading changed another part");check(p.currentPart()==selectedPart,"Per-part preset navigation changed editor selection");}
 const int directions[]{1,-1,-1}, pitches[]{76,72,76};for(int i=0;i<3;++i){click(panel,directions[i]>0?"performance:part1:next":"performance:part1:previous");render(p);panel.refresh();check(raw(p,0,"osc1_semitone")==pitches[i],"Per-part wrapping failed");}
 auto* menu=dynamic_cast<juce::ComboBox*>(find(panel,"performance:part3:preset"));check(menu&&menu->getNumItems()==2,"Channel menu differs from browser filter");menu->setSelectedId(2,juce::dontSendNotification);menu->onChange();render(p);panel.refresh();check(raw(p,2,"osc1_semitone")==76,"Channel preset menu did not load sound");check(raw(p,1,"volume")==77&&raw(p,1,"part_low")==36&&p.activeParts()==7,"Preset loading lost part metadata");
 std::cout<<"PASS: each part loads only its own sound, filtered preset menu and independent Previous/Next/wrap, routing/range/volume preserved\n";
}
static void pressStrip(juce::Component& target,bool right=false){
 const auto now=juce::Time::getCurrentTime();const auto pos=target.getLocalBounds().getCentre().toFloat();
 juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(),pos,juce::ModifierKeys(right?juce::ModifierKeys::rightButtonModifier:juce::ModifierKeys::leftButtonModifier),1.f,0,0,0,0,&target,&target,now,pos,now,1,false);
 target.mouseDown(event);
 // Mirror the recursive listener attached by the Performance panel.
 for(auto* parent=target.getParentComponent();parent;parent=parent->getParentComponent()){if(parent->getComponentID()=="performancePanel")break;parent->mouseDown(event);}
}
static void verify(Processor& p,bool ready){
 std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());click(*editor,"pagePerf");auto* panel=dynamic_cast<PerformancePanel*>(find(*editor,"performancePanel"));check(panel&&panel->isVisible(),"Mixer page missing");auto& header=button(*editor,"performanceSwitch");check(header.isVisible(),"Header Performance switch removed");check(header.getBounds().getY()<83,"Header switch moved out of top row");
 for(int n=1;n<=4;++n){auto prefix="performance:part"+juce::String(n)+":";check(find(*panel,prefix+"strip"),"Part strip missing");check(!find(*panel,prefix+"select"),"Redundant numbered selection square remains");for(const char* id:{"part_channel","part_output","part_transpose","part_fine","part_velocity","volume","part_low","part_high"})check(find(*panel,prefix+id),"Part control missing");}
 if(!ready){check(!button(*panel,"performancePageSwitch").isEnabled(),"Performance available without KS firmware");return;}
 header.setToggleState(true,juce::dontSendNotification);header.onClick();render(p,{},200);panel->refresh();check(p.performanceMode(),"Performance not enabled");
 const auto initialPart=p.currentPart();auto* part2=find(*panel,"performance:part2:strip");auto* part3=find(*panel,"performance:part3:part_channel");
 pressStrip(*part2);check(p.currentPart()==1,"Clicking the part surface did not select it");
 pressStrip(*part3);check(p.currentPart()==2,"Clicking a descendant did not select its part");
 pressStrip(*part2,true);check(p.currentPart()==2,"Opening Size via right click changed selection");
 auto accessibility=part2->createAccessibilityHandler();check(accessibility&&accessibility->getActions().invoke(juce::AccessibilityActionType::press),"Part surface has no accessibility selection action");check(p.currentPart()==1,"Accessible part selection failed");
 p.selectPart(initialPart);panel->refresh();
 auto* level=dynamic_cast<juce::Slider*>(find(*panel,"performance:part2:volume:slider"));check(level,"Part fader");level->setValue(77,juce::sendNotificationSync);render(p);panel->refresh();check(raw(p,1,"volume")==77&&raw(p,0,"volume")==127&&raw(p,2,"volume")==127,"Part volumes share a binding");
 auto& active=button(*panel,"performance:part4:active");active.setToggleState(false,juce::dontSendNotification);active.onClick();render(p);panel->refresh();check(p.activeParts()==7,"Part Off did not reach firmware");
 click(*panel,"performance:part2:part_low:open");auto* manual=dynamic_cast<juce::ComboBox*>(find(*panel,"performanceNoteManual"));check(manual,"Manual notes missing");manual->setSelectedId(85,juce::dontSendNotification);manual->onChange();render(p);panel->refresh();check(raw(p,1,"part_low")==84,"Manual Low Note binding");
 click(*panel,"performance:part2:part_high:open");manual->setSelectedId(37,juce::dontSendNotification);manual->onChange();render(p);panel->refresh();check(raw(p,1,"part_low")==36&&raw(p,1,"part_high")==36,"Exact note/range edge invariant");
 click(*panel,"performance:part3:part_high:open");click(*panel,"performanceNoteLearn");juce::MidiBuffer ignored;ignored.addEvent(juce::MidiMessage::controllerEvent(1,64,127),0);ignored.addEvent(juce::MidiMessage::noteOn(1,60,uint8_t(0)),1);render(p,ignored);panel->refresh();check(button(*panel,"performanceNoteLearn").getToggleState(),"Learn captured CC or Note Off");
 juce::MidiBuffer notes;notes.addEvent(juce::MidiMessage::noteOn(16,72,uint8_t(100)),0);notes.addEvent(juce::MidiMessage::noteOn(16,74,uint8_t(100)),1);render(p,notes);panel->refresh();render(p);panel->refresh();check(raw(p,2,"part_high")==72&&raw(p,1,"part_high")==36,"Learn did not apply first raw note to requested part");check(!button(*panel,"performanceNoteLearn").getToggleState(),"Learning remained armed");shot(*editor,"KS-Performance-MIDI-Learn");
 click(*panel,"performance:part1:part_low:open");click(*panel,"performanceNoteLearn");click(*editor,"pageMain");render(p,notes);click(*editor,"pagePerf");panel->refresh();check(raw(p,0,"part_low")==0,"Hidden page still learning");
 click(*panel,"performance:part3:edit");check(p.currentPart()==2&&!panel->isVisible(),"Edit sound did not select/open part");click(*editor,"pagePerf");panel->refresh();shot(*editor,"KS-Performance-Mixer");
 programs(p,*editor,*panel);
 juce::MemoryBlock state;p.getStateInformation(state);Processor restored;restored.prepareToPlay(48000,256);restored.setNonRealtime(true);restored.setStateInformation(state.getData(),int(state.getSize()));render(restored,{},200);check(restored.performanceMode()&&restored.activeParts()==7&&raw(restored,1,"volume")==77&&raw(restored,2,"part_high")==72,"Performance state did not restore");
 auto& power=button(*panel,"performancePageSwitch");power.setToggleState(false,juce::dontSendNotification);power.onClick();render(p);panel->refresh();juce::Timer::callPendingTimersSynchronously();check(!p.performanceMode(),"Page Performance Off failed");check(!header.getToggleState(),"Header/page mode switches not synchronized");
 std::cout<<"PASS: four native strips, independent part parameters/active masks, manual notes, MIDI through processBlock, page cancellation, Edit sound and state restore\n";
}
int main(int argc,char** argv){try{juce::ScopedJuceInitialiser_GUI gui;midiTests();Processor p;p.skinChoice="Silver-K";verify(p,false);if(argc==2){p.prepareToPlay(48000,256);p.setNonRealtime(true);juce::String error;check(p.loadFirmware(juce::File(argv[1]),error),error);verify(p,true);}return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
