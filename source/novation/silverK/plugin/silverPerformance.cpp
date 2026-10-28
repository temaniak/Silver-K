#include "silverPerformance.h"
#include "silverBrowser.h"
#include "silverSkin.h"
namespace silverK {
namespace {

juce::String prefix(unsigned part){return "performance:part"+juce::String(part+1)+":";}
juce::String noteName(int note){return juce::MidiMessage::getMidiNoteName(note,true,true,4)+" / "+juce::String(note);}
const ksPlugin::Control& spec(const char* id){for(const auto& c:ksPlugin::controls)if(juce::String(c.id)==id)return c;jassertfalse;return ksPlugin::controls[0];}
void panel(const juce::Component& component,juce::Graphics& g,juce::Rectangle<float> r,bool selected=false){
 const float rounding=skinIsFlat(component)?0.f:3.f;
 if(skinIsFlat(component))g.setColour(skinColour(component,0xffeef1f5));else g.setGradientFill(juce::ColourGradient(skinColour(component,0xffeef1f5),r.getX(),r.getY(),skinColour(component,0xffe4eaf0),r.getRight(),r.getBottom(),false));g.fillRoundedRectangle(r,rounding);
 g.setColour(selected?skinColour(component,0xff087fe5):skinColour(component,0xffbdc7cf));g.drawRoundedRectangle(r.reduced(.5f),rounding,1);if(selected)g.fillRect(r.getX()+1,r.getY()+1,r.getWidth()-2,2.f);
}
struct Parameter final:juce::Component {
 const ksPlugin::Control& s;juce::RangedAudioParameter& p;
 juce::Label caption,value;juce::Slider slider;juce::ComboBox menu;bool gesture=false;juce::String kind;
 Parameter(Processor& processor,unsigned part,const char* id,const char* title,juce::String type):s(spec(id)),p(*processor.parameters.getParameter(Processor::parameterId(s,part))),kind(type){
  const auto key=prefix(part)+id;setComponentID(key);addAndMakeVisible(caption);caption.setComponentID(key+":caption");caption.setText(title,juce::dontSendNotification);setSkinColour(caption,juce::Label::textColourId,juce::Colour(0xff171b20));caption.setFont(juce::Font(12.f));caption.setBorderSize(juce::BorderSize<int>(0));caption.setJustificationType(kind=="menu"?juce::Justification::centredLeft:juce::Justification::centred);
  if(kind=="menu"){
   addAndMakeVisible(menu);menu.setComponentID(key+":menu");
   for(int v=s.minimum;v<=s.maximum;++v)menu.addItem(s.enumCount?s.enumLabels[v]:juce::String(v+s.displayOffset),v-int(s.minimum)+1);
   menu.onChange=[this]{setRaw(menu.getSelectedId()-1+s.minimum);};
  }else{
   addAndMakeVisible(slider);addAndMakeVisible(value);slider.setComponentID(key+":slider");value.setComponentID(key+":value");
   slider.setRange(s.minimum,s.maximum,1);slider.setSliderStyle(kind=="fader"?juce::Slider::LinearVertical:juce::Slider::RotaryVerticalDrag);slider.setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);slider.setRotaryParameters(juce::MathConstants<float>::pi*4.f/3.f,juce::MathConstants<float>::pi*8.f/3.f,true);slider.setDoubleClickReturnValue(true,s.defaultValue);
   slider.getProperties().set("standKnob",true);slider.getProperties().set("rotaryStyle","led");slider.getProperties().set("rotaryDiameter",72);
   value.setFont(juce::Font(12.f));value.setJustificationType(juce::Justification::centred);setSkinColour(value,juce::Label::textColourId,juce::Colour(0xff171b20));value.setEditable(false,true,false);value.onTextChange=[this]{setRaw(value.getText().getIntValue()-s.displayOffset);refresh();};
   slider.onDragStart=[this]{gesture=true;p.beginChangeGesture();};slider.onDragEnd=[this]{p.endChangeGesture();gesture=false;};slider.onValueChange=[this]{setRaw(juce::roundToInt(slider.getValue()));};
  }
  setTitle("Part "+juce::String(part+1)+" "+title);refresh();
 }
 ~Parameter()override{if(gesture)p.endChangeGesture();}
 int raw()const{return juce::roundToInt(p.convertFrom0to1(p.getValue()));}
 void setRaw(int v){if(!gesture)p.beginChangeGesture();p.setValueNotifyingHost(p.convertTo0to1(float(juce::jlimit(int(s.minimum),int(s.maximum),v))));if(!gesture)p.endChangeGesture();}
 void refresh(){const auto v=raw();if(kind=="menu")menu.setSelectedId(v-int(s.minimum)+1,juce::dontSendNotification);else{if(!slider.isMouseButtonDown())slider.setValue(v,juce::dontSendNotification);if(!value.isBeingEdited())value.setText(juce::String(v+s.displayOffset),juce::dontSendNotification);}}
 void resized()override{
  if(kind=="menu"){caption.setBounds(0,0,getWidth(),16);menu.setBounds(0,20,getWidth(),28);}
  else if(kind=="fader"){caption.setBounds(0,0,getWidth(),22);slider.setBounds((getWidth()-64)/2,22,64,getHeight()-44);value.setBounds(0,getHeight()-20,getWidth(),20);}
  else{slider.setBounds(0,0,getWidth(),76);caption.setBounds(0,76,getWidth(),16);value.setBounds(0,92,getWidth(),18);}
  applySkin(*this,false);
 }
};
struct NoteButton final:juce::Component {
 juce::Label caption;juce::TextButton button;
 NoteButton(unsigned part,bool high){setComponentID(prefix(part)+(high?"part_high":"part_low"));addAndMakeVisible(caption);addAndMakeVisible(button);caption.setComponentID(getComponentID()+":caption");caption.setText(high?"High note":"Low note",juce::dontSendNotification);caption.setFont(juce::Font(12.f));setSkinColour(caption,juce::Label::textColourId,juce::Colour(0xff171b20));caption.setBorderSize(juce::BorderSize<int>(0));button.setComponentID(getComponentID()+":open");}
 void resized()override{caption.setBounds(0,0,getWidth(),16);button.setBounds(0,20,getWidth(),28);applySkin(*this,false);}
};
struct Channel final:juce::Component {
 Processor& p;unsigned part;juce::TextButton on{"On"},previous{"<"},next{">"},edit{"Edit sound"};
 juce::Label title;juce::ComboBox program;juce::Component controls;
 Parameter midi,output,transpose,fine,velocity,volume;NoteButton low,high;std::function<void()> onSelect;
 Channel(Processor& processor,unsigned n):p(processor),part(n),midi(p,n,"part_channel","MIDI channel","menu"),output(p,n,"part_output","Output","menu"),transpose(p,n,"part_transpose","Transpose","knob"),fine(p,n,"part_fine","Fine tuning","knob"),velocity(p,n,"part_velocity","Velocity curve","menu"),volume(p,n,"volume","Volume","fader"),low(n,false),high(n,true){
  setComponentID(prefix(part)+"strip");setTitle("Part "+juce::String(n+1));setWantsKeyboardFocus(true);
  for(auto* c:std::initializer_list<juce::Component*>{&on,&title,&controls})addAndMakeVisible(*c);
  for(auto* c:std::initializer_list<juce::Component*>{&program,&previous,&next,&edit,&midi,&output,&transpose,&fine,&velocity,&volume,&low,&high})controls.addAndMakeVisible(*c);
  title.setComponentID(prefix(part)+"title");controls.setComponentID(prefix(part)+"controls");title.setText("Part "+juce::String(n+1),juce::dontSendNotification);title.setFont(juce::Font(17.f,juce::Font::bold));setSkinColour(title,juce::Label::textColourId,juce::Colour(0xff171b20));title.setAccessible(false);
  on.getProperties().set("led",true);on.setClickingTogglesState(true);on.setComponentID(prefix(part)+"active");program.setComponentID(prefix(part)+"preset");program.getProperties().set("performancePreset",true);program.setTextWhenNothingSelected("Select preset");previous.setComponentID(prefix(part)+"previous");next.setComponentID(prefix(part)+"next");edit.setComponentID(prefix(part)+"edit");
 }
 void mouseDown(const juce::MouseEvent& e)override{if(e.mods.isLeftButtonDown()&&onSelect)onSelect();}
 std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler()override{
  struct Handler final:juce::AccessibilityHandler {
   Channel& channel;
   explicit Handler(Channel& c):AccessibilityHandler(c,juce::AccessibilityRole::group,juce::AccessibilityActions().addAction(juce::AccessibilityActionType::press,[&c]{if(c.onSelect)c.onSelect();})),channel(c){}
   juce::AccessibleState getCurrentState()const override{auto state=AccessibilityHandler::getCurrentState().withSelectable();return channel.p.currentPart()==channel.part?state.withSelected():state;}
  };
  return std::make_unique<Handler>(*this);
 }
 bool keyPressed(const juce::KeyPress& key)override{if((key==juce::KeyPress::returnKey||key==juce::KeyPress::spaceKey)&&onSelect){onSelect();return true;}return false;}
 void paint(juce::Graphics& g)override{if(paintSkinBackground(*this,g)||!skinDrawPanels(*this))return;
  panel(*this,g,getLocalBounds().toFloat(),p.currentPart()==part);
  const bool active=(p.activeParts()&(1u<<part))!=0;const auto lcd=juce::Rectangle<float>(20,68,getWidth()-40.f,76);
  g.setColour(skinColour(*this,0xff1c2a38));g.fillRoundedRectangle(lcd,skinIsFlat(*this)?0.f:3.f);auto face=lcd.reduced(4);if(skinIsFlat(*this))g.setColour(skinColour(*this,0xff0867df));else g.setGradientFill(juce::ColourGradient(skinColour(*this,0xff0867df),face.getX(),face.getY(),skinColour(*this,0xff004fbc),face.getX(),face.getBottom(),false));g.fillRect(face);
  g.setColour(skinColour(*this,0xfff1f4f7));g.setFont(11.f);g.drawText("Preset",34,78,getWidth()-68,16,juce::Justification::centredLeft);
  g.setColour(skinColour(*this,0xff171b20).withAlpha(.65f));g.setFont(12.f);for(const auto& row:std::initializer_list<std::pair<int,const char*>>{{158,"Routing"},{252,"Keyboard range"},{366,"Tuning"}})g.drawText(row.second,20,row.first,getWidth()-40,16,juce::Justification::centredLeft);
  g.setColour(skinColour(*this,0xffbdc7cf));for(int y:{238,352,512,574})g.drawHorizontalLine(y,20.f,getWidth()-20.f);
  const auto raw=[this](const char* id){auto* v=p.parameters.getRawParameterValue(Processor::parameterId(spec(id),part));return v?int(v->load()):0;};
  g.setColour(skinColour(*this,0xffc5cdd6));g.fillRect(20,338,getWidth()-40,8);g.setColour(skinColour(*this,0xff087fe5));g.fillRect(20.f+float(raw("part_low"))*(getWidth()-40)/128.f,338.f,float(raw("part_high")-raw("part_low")+1)*(getWidth()-40)/128.f,8.f);
  const int faderHeight=getHeight()-700;g.setFont(11.f);g.setColour(skinColour(*this,0xff171b20).withAlpha(active?.6f:.3f));for(int i=0;i<5;++i)g.drawText(juce::String(i==4?0:127-i*32),getWidth()/2+48,603+i*(faderHeight-1)/4,36,14,juce::Justification::centredLeft);
 }
 void resized()override{
  title.setBounds(20,16,164,40);on.setBounds(getWidth()-86,20,66,32);controls.setBounds(getLocalBounds());
  const int width=getWidth()-40,half=(width-14)/2;
  program.setBounds(30,99,width-50,30);previous.setBounds(getWidth()-55,79,25,22);next.setBounds(getWidth()-55,105,25,22);
  midi.setBounds(20,183,half,48);output.setBounds(34+half,183,half,48);low.setBounds(20,277,half,48);high.setBounds(34+half,277,half,48);
  transpose.setBounds(20+(half-100)/2,391,100,110);fine.setBounds(34+half+(half-100)/2,391,100,110);velocity.setBounds(20,526,width,48);
  volume.setBounds((getWidth()-100)/2,588,100,getHeight()-656);edit.setBounds(20,getHeight()-50,width,34);
  controls.toBack();applySkin(*this,false);
 }
 void refresh(bool enabled,const juce::String& name){
  controls.setEnabled(enabled);on.setEnabled(enabled);const bool active=(p.activeParts()&(1u<<part))!=0;on.setToggleState(active,juce::dontSendNotification);on.setButtonText(active?"On":"Off");volume.setAlpha(active?1.f:.45f);program.setAlpha(active?1.f:.45f);
  for(auto* c:{&midi,&output,&transpose,&fine,&velocity,&volume})c->refresh();
  low.button.setButtonText(noteName(int(p.parameters.getRawParameterValue(Processor::parameterId(spec("part_low"),part))->load()))+"  v");high.button.setButtonText(noteName(int(p.parameters.getRawParameterValue(Processor::parameterId(spec("part_high"),part))->load()))+"  v");
  if(!program.isPopupActive())program.setText(name.isEmpty()?"Init Program":name,juce::dontSendNotification);repaint();
 }
};
struct NoteEditor final:juce::Component {
 juce::Label heading,manualLabel,status;juce::ComboBox manual;juce::TextButton learn{"Learn MIDI"},close{"x"};std::function<void()> onClose;
 NoteEditor(){setComponentID("performanceNoteEditor");for(auto* c:std::initializer_list<juce::Component*>{&heading,&manualLabel,&status,&manual,&learn,&close})addAndMakeVisible(*c);
  for(auto* l:{&heading,&manualLabel,&status}){setSkinColour(*l,juce::Label::textColourId,juce::Colour(0xff171b20));l->setFont(skinFont(*this,l==&heading?13.f:11.5f));l->setBorderSize(juce::BorderSize<int>(0));}
  manualLabel.setText("Choose manually",juce::dontSendNotification);for(int n=0;n<128;++n)manual.addItem(noteName(n),n+1);
  manual.setComponentID("performanceNoteManual");learn.setComponentID("performanceNoteLearn");close.setComponentID("performanceNoteClose");status.setComponentID("performanceNoteStatus");learn.getProperties().set("led",true);close.onClick=[this]{if(onClose)onClose();};setVisible(false);
 }
 void paint(juce::Graphics& g)override{if(paintSkinBackground(*this,g)||!skinDrawPanels(*this))return;panel(*this,g,getLocalBounds().toFloat());g.setColour(skinColour(*this,0xff087fe5));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(.5f),3,1);g.setColour(skinColour(*this,0xff171b20).withAlpha(.65f));g.setFont(11.f);g.drawText("or play a MIDI key",12,112,getWidth()-24,18,juce::Justification::centred);}
 void resized()override{heading.setBounds(12,10,getWidth()-52,24);close.setBounds(getWidth()-36,10,24,24);manualLabel.setBounds(12,46,getWidth()-24,16);manual.setBounds(12,66,getWidth()-24,28);learn.setBounds(12,140,getWidth()-24,32);status.setBounds(12,184,getWidth()-24,44);applySkin(*this,false);}
 bool keyPressed(const juce::KeyPress& key)override{if(key==juce::KeyPress::escapeKey){if(onClose)onClose();return true;}return false;}
};
}
struct PerformancePanel::Impl final:juce::Component {
 PerformancePanel& owner;Processor& p;Browser& browser;juce::TextButton power{"Performance"};juce::Label message;std::array<std::unique_ptr<Channel>,4> channels;NoteEditor notes;
 std::vector<pluginLib::patchDB::PatchPtr> programs;unsigned notePart=0;bool high=false;uint64_t learning=0;int shownMode=-1;
 Impl(PerformancePanel& panel,Processor& processor,Browser& b):owner(panel),p(processor),browser(b){
  setComponentID("performanceCanvas");message.setComponentID("performanceMessage");
  addAndMakeVisible(power);power.setComponentID("performancePageSwitch");power.getProperties().set("led",true);power.setClickingTogglesState(true);power.onClick=[this]{juce::String error;if(!p.setPerformanceMode(power.getToggleState(),error))message.setText(error,juce::dontSendNotification);refresh();};
  addAndMakeVisible(message);setSkinColour(message,juce::Label::textColourId,juce::Colour(0xff171b20));message.setFont(juce::Font(12.f));
  for(unsigned n=0;n<4;++n){auto& c=channels[n];c=std::make_unique<Channel>(p,n);addAndMakeVisible(*c);
   c->onSelect=[this,n]{if(!p.firmwareReady()||p.isKStation()||!p.performanceMode()||p.currentPart()==n)return;closeNotes();p.selectPart(n);refresh();};c->on.onClick=[this,n]{juce::String error;if(!p.setPartActive(n,channels[n]->on.getToggleState(),error))message.setText(error,juce::dontSendNotification);refresh();};
   c->edit.onClick=[this,n]{closeNotes();if(owner.onEdit)owner.onEdit(n);};
   c->previous.onClick=[this,n]{step(n,-1);};c->next.onClick=[this,n]{step(n,1);};
   c->program.onChange=[this,n]{const int index=channels[n]->program.getSelectedId()-1;if(index<0||index>=int(programs.size()))return;juce::String error;if(!browser.activateProgram(n,programs[size_t(index)],error))message.setText(error,juce::dontSendNotification);refresh();};
   c->low.button.onClick=[this,n]{openNotes(n,false);};c->high.button.onClick=[this,n]{openNotes(n,true);};
  }
  addChildComponent(notes);notes.onClose=[this]{closeNotes();};notes.manual.onChange=[this]{cancelLearn();applyNote(notes.manual.getSelectedId()-1);notes.status.setText("Selected: "+noteName(notes.manual.getSelectedId()-1),juce::dontSendNotification);};
  notes.learn.onClick=[this]{if(learning){cancelLearn();notes.status.setText("Learning cancelled",juce::dontSendNotification);}else{learning=p.beginNoteLearn();notes.learn.setToggleState(true,juce::dontSendNotification);notes.learn.setButtonText("Cancel learning");notes.status.setText("Play one note on any MIDI input/channel",juce::dontSendNotification);}};
  addMouseListener(this,true);
 }
 ~Impl()override{cancelLearn();removeMouseListener(this);}
 void step(unsigned part,int direction){juce::String error;if(!browser.stepProgram(part,direction,error))message.setText(error,juce::dontSendNotification);refresh();}
 void cancelLearn(){if(learning)p.cancelNoteLearn(learning);learning=0;notes.learn.setToggleState(false,juce::dontSendNotification);notes.learn.setButtonText("Learn MIDI");}
 void closeNotes(){cancelLearn();notes.setVisible(false);}
 void openNotes(unsigned part,bool isHigh){cancelLearn();notePart=part;high=isHigh;const auto& c=*channels[part];auto* param=p.parameters.getRawParameterValue(Processor::parameterId(spec(high?"part_high":"part_low"),part));notes.manual.setSelectedId(int(param->load())+1,juce::dontSendNotification);notes.heading.setText("Part "+juce::String(part+1)+" / "+(high?"High note":"Low note"),juce::dontSendNotification);notes.status.setText("The next Note On selects this note",juce::dontSendNotification);notes.setBounds(c.getX()+20,c.getY()+278,c.getWidth()-40,238);notes.setVisible(true);notes.toFront(true);applySkin(notes);notes.manual.grabKeyboardFocus();}
 void setRaw(const char* id,int raw){auto* parameter=p.parameters.getParameter(Processor::parameterId(spec(id),notePart));parameter->beginChangeGesture();parameter->setValueNotifyingHost(parameter->convertTo0to1(float(raw)));parameter->endChangeGesture();}
 void applyNote(int note){if(note<0||note>127)return;auto raw=[this](const char* id){return int(p.parameters.getRawParameterValue(Processor::parameterId(spec(id),notePart))->load());};if(high&&note<raw("part_low"))setRaw("part_low",note);if(!high&&note>raw("part_high"))setRaw("part_high",note);setRaw(high?"part_high":"part_low",note);notes.manual.setSelectedId(note+1,juce::dontSendNotification);refresh();}
 void mouseDown(const juce::MouseEvent& event)override{
  if(!event.mods.isLeftButtonDown())return;
  if(notes.isVisible()&&notes.getBounds().contains(event.getEventRelativeTo(this).getPosition()))return;
  if(notes.isVisible())closeNotes();
  for(auto& c:channels)if(event.eventComponent==c.get()||c->isParentOf(event.eventComponent)){if(c->onSelect)c->onSelect();break;}
 }
 void refresh(){
  const int mode=p.performanceMode()?1:0;if(shownMode!=mode){shownMode=mode;if(owner.onModeChanged)owner.onModeChanged();}
  const bool ready=p.firmwareReady()&&!p.isFirmwareLoading()&&!p.isKStation(),enabled=ready&&p.performanceMode();power.setEnabled(ready);power.setToggleState(p.performanceMode(),juce::dontSendNotification);power.setButtonText(p.performanceMode()?"Performance On":"Performance Off");
  if(!enabled)closeNotes();
  if(learning){const int note=p.takeLearnedNote(learning);if(note>=0){cancelLearn();applyNote(note);notes.status.setText("Learned: "+noteName(note),juce::dontSendNotification);}}
  bool popup=false;for(const auto& c:channels)popup|=c->program.isPopupActive();
  if(!popup){auto next=browser.filteredPrograms();if(next!=programs){programs=std::move(next);for(auto& c:channels){c->program.clear(juce::dontSendNotification);for(size_t i=0;i<programs.size();++i)c->program.addItem(patchName(*programs[i]),int(i)+1);}}}
  const auto names=p.partNames();for(unsigned n=0;n<4;++n)channels[n]->refresh(enabled,names[n]);
  if(p.isKStation())message.setText("Four-part Performance is available with KS firmware",juce::dontSendNotification);
  else if(!p.performanceMode())message.setText("Enable Performance to edit all four parts",juce::dontSendNotification);
  else if(message.getText().startsWith("Enable Performance")||message.getText().startsWith("Four-part"))message.setText({},juce::dontSendNotification);
  repaint();
 }
 void paint(juce::Graphics& g)override{if(paintSkinBackground(*this,g)||!skinDrawPanels(*this))return;panel(*this,g,{0,0,1254,60});g.setColour(skinColour(*this,0xff171b20));g.setFont(skinFont(*this,20.f,juce::Font::bold));g.drawText("Performance",20,0,170,60,juce::Justification::centredLeft);g.setFont(12.f);g.setColour(skinColour(*this,0xff171b20).withAlpha(.65f));g.drawText("4 parts",190,0,70,60,juce::Justification::centredLeft);}
 void resized()override{power.setBounds(1074,14,160,32);message.setBounds(275,14,780,32);for(unsigned n=0;n<4;++n)channels[n]->setBounds(int(n)*315,66,309,838);applySkin(*this,false);}
};
PerformancePanel::PerformancePanel(Processor& p,Browser& b):impl(std::make_unique<Impl>(*this,p,b)){setComponentID("performancePanel");addAndMakeVisible(*impl);impl->setBounds(0,0,1254,904);}
PerformancePanel::~PerformancePanel()=default;
void PerformancePanel::resized(){
 const auto* look=dynamic_cast<const SkinLookAndFeel*>(&getLookAndFeel());const auto size=look?look->package.layout["performanceCanvas"]:juce::var{};
 const int width=size.isArray()?int(size[0]):1254,height=size.isArray()?int(size[1]):904;
 impl->setSize(width,height);impl->setTransform(juce::AffineTransform::scale(float(getWidth())/width,float(getHeight())/height));applySkin(*this,false);
}
void PerformancePanel::refresh(){if(isVisible())impl->refresh();else impl->closeNotes();}
void PerformancePanel::closeNoteEditor(){impl->closeNotes();}
}
