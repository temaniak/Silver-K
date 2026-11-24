#include "silverSoulTrain.h"
#include "silverBrowser.h"
#include "silverEditor.h"
#include "silverPatch.h"
#if JUCE_MAC
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#elif JUCE_WINDOWS
#include <windows.h>
#endif
#include <iostream>
#include <stdexcept>
using namespace silverK;
static void check(bool value,const juce::String& reason){if(!value)throw std::runtime_error(reason.toStdString());}
static void pump(double seconds=.1){
 const auto end=juce::Time::getMillisecondCounterHiRes()+seconds*1000;
 while(juce::Time::getMillisecondCounterHiRes()<end){
#if JUCE_MAC
  CFRunLoopRunInMode(kCFRunLoopDefaultMode,.01,true);
#elif JUCE_WINDOWS
  MSG message{};
  while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
   TranslateMessage(&message);
   DispatchMessageW(&message);
  }
  juce::Thread::sleep(1);
#endif
 }
}
static juce::Component* findComponent(juce::Component& root,const juce::String& id){
 if(root.getComponentID()==id)return &root;
 for(auto* child:root.getChildren())
  if(auto* match=findComponent(*child,id))return match;
 return nullptr;
}
static void checkDefaultSkin(){
 auto verifySoulTrain=[](juce::AudioProcessorEditor& editor){
  auto* web=findComponent(editor,"soulTrainWeb");
  check(web && web->isVisible(),"SoulTrain did not open as the default");
 };
 auto choose=[](juce::AudioProcessorEditor& editor,int skin){
  auto* menu=dynamic_cast<juce::ComboBox*>(findComponent(editor,"skinSelect"));
  check(menu && menu->getItemText(0)=="SoulTrain (Default)","Default skin missing from Setup");
  menu->setSelectedId(skin,juce::sendNotificationSync);
  pump(.15);
 };
 {
  Processor p;
  check(p.skinChoice==Processor::defaultSkin,"Use isolated fresh preferences for the default-skin check");
  std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
  verifySoulTrain(*editor);
  choose(*editor,2);
  auto* web=findComponent(*editor,"soulTrainWeb");
  check(p.skinChoice=="Silver-K" && web && !web->isVisible(),"Original skin selection did not take effect");
 }
 {
  Processor p;
  check(p.skinChoice=="Silver-K","Original skin selection was not saved");
  std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
  check(findComponent(*editor,"soulTrainWeb")==nullptr,"Original choice did not survive reopening");
  choose(*editor,3);
  check(p.skinChoice=="Silver Flat","Experimental grey skin was not selected");
  choose(*editor,1);
  check(p.skinChoice==Processor::defaultSkin,"Default skin was not selected");
  verifySoulTrain(*editor);
 }
 {
  Processor p;
  check(p.skinChoice==Processor::defaultSkin,"Default skin selection was not saved");
  std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
  verifySoulTrain(*editor);
 }
 std::cout<<"PASS: SoulTrain default, original skin switch, and saved selection on reopening\n";
}
static juce::var js(SoulTrainWeb& web,const juce::String& code){
 struct Result { bool done=false; juce::var value; };
 const auto result=std::make_shared<Result>();
 web.evaluate(code,[result](juce::var value){result->value=std::move(value);result->done=true;});
 for(int n=0;n<400 && !result->done;++n)pump(.01);
 check(result->done,"Native browser script timed out");
 return result->value;
}
static void waitJS(SoulTrainWeb& web,const juce::String& code){
 for(int n=0;n<220;++n){if(bool(js(web,code)))return;pump(.05);}
 throw std::runtime_error("Native browser condition failed: "+code.toStdString()
   +"; body="+js(web,"document.body.innerText").toString().substring(0,400).toStdString());
}
static void command(SoulTrainWeb& web,const juce::var& data){js(web,"window.silverSend("+juce::JSON::toString(data,true)+");true");pump(.08);}
static juce::var msg(std::initializer_list<std::pair<juce::String,juce::var>> list){auto* o=new juce::DynamicObject;for(auto& v:list)o->setProperty(v.first,v.second);return o;}
static void page(SoulTrainWeb& web,const juce::String& name){js(web,"Array.from(document.querySelectorAll('.a-pages button')).find(b=>b.textContent==="+juce::JSON::toString(name=="Performance"?juce::String("Perf"):name)+").click();true");pump(.35);}
static void checkFirmwareOptions(SoulTrainWeb& web,Processor& processor){
 const juce::String trigger="document.querySelector('[role=combobox][aria-label=\"On firmware change\"]')";
 check(!bool(js(web,"Boolean("+trigger+"?.getClientRects().length)")),"WebKit firmware option escaped Setup");
 page(web,"Setup");
 waitJS(web,"Boolean("+trigger+"?.getClientRects().length)");
 check(bool(js(web,"(()=>{const button="+trigger+";const rect=button.getBoundingClientRect();return button.contains(document.elementFromPoint(rect.x+rect.width/2,rect.y+rect.height/2));})()")),
       "WebKit firmware option does not receive pointer input");
 for(int mode:{1,0}){
  js(web,trigger+".click();true");
  const juce::String options="document.querySelectorAll('[role=listbox][aria-label=\"On firmware change\"] [role=option]')";
  waitJS(web,options+".length===2");
  js(web,options+"["+juce::String(mode)+"].click();true");
  waitJS(web,trigger+".dataset.value==='"+juce::String(mode)+"'");
  check(int(processor.firmwareSwitch())==mode,"WebKit firmware option did not reach the processor");
 }
 page(web,"Main");
 std::cout<<"PASS: WebKit firmware switch mode in "<<(processor.isKStation()?"K-Station":"KS")<<" Setup\n";
}
static void capture([[maybe_unused]] SoulTrainWeb& web,[[maybe_unused]] const juce::File& file){
#if JUCE_MAC
 auto* native=dynamic_cast<juce::NSViewComponent*>(web.getChildComponent(0));check(native,"No native WebKit view");auto* view=(__bridge WKWebView*)native->getView();__block bool done=false;
 [view takeSnapshotWithConfiguration:nil completionHandler:^(NSImage* image,NSError* error){if(image){NSBitmapImageRep* rep=[[NSBitmapImageRep alloc] initWithData:image.TIFFRepresentation];NSData* png=[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];[png writeToFile:[NSString stringWithUTF8String:file.getFullPathName().toRawUTF8()] atomically:YES];}done=true;}];
 for(int n=0;n<300&&!done;++n)pump(.01);check(file.existsAsFile(),"WebKit screenshot failed");
#endif
}
static std::vector<uint8_t> fixture(silverK::Model model,int bank=0){
 using silverK::Model;std::vector<uint8_t> bytes(model==Model::KStation?142:270,0);const uint8_t header[]{0xf0,0,0x20,0x29,1,uint8_t(model==Model::KStation?0x41:0x42),0x7f,uint8_t(bank?1:0),0,0x11,uint8_t(model==Model::KStation?1:2),uint8_t(bank),7};std::copy_n(header,13,bytes.begin());bytes.back()=0xf7;
 const auto controls=model==Model::KStation?kControls():std::vector<Control>(std::begin(ksPlugin::controls),std::end(ksPlugin::controls));for(const auto& c:controls)if(c.patchOffset>=0){auto& b=bytes[13+c.patchOffset];b=uint8_t((b&~c.patchMask)|((c.defaultValue<<c.patchShift)&c.patchMask));}
 if(bytes.size()==270){std::fill_n(bytes.begin()+164,16,' ');const std::string name="UI test bank "+std::to_string(bank);std::copy(name.begin(),name.end(),bytes.begin()+164);}return bytes;
}
static juce::var browserData(Browser& browser){browser.pollLibrary();return browser.webSnapshot();}
static int run(const juce::StringArray& args){try{
 check(args.size()>=2,"Pass compiled frontend directory, then optional K / KS firmware / KS flash / waves");
#if JUCE_MAC
 [NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];[NSApp finishLaunching];
#endif
 juce::ScopedJuceInitialiser_GUI gui;
 const auto testRoot=juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA",juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("soultrain-test-data").getFullPathName()));testRoot.createDirectory();const auto captures=testRoot.getChildFile("captures");captures.createDirectory();
 checkDefaultSkin();
 Processor p;p.prepareToPlay(48000,256);p.setNonRealtime(true);juce::String error;check(p.setKSControllerMode(1,error),error);Browser browser(p);
 SoulTrainWeb::Callbacks cb;cb.browserSnapshot=[&]{return browserData(browser);};cb.browserAction=[&](const juce::var& v){browser.webAction(v);};
 cb.skinMenu=[]{
  juce::Array<juce::var> choices;
  choices.add(msg({{"value",1},{"label","SoulTrain (Default)"}}));
  choices.add(msg({{"value",2},{"label","Old version skin"}}));
  choices.add(msg({{"value",3},{"label","Experimental grey"}}));
  return msg({{"choices",choices},{"value",1}});
 };
 juce::String setupAction;
 cb.nativeAction=[&](const juce::String& action){
  setupAction=action;
  if(action.startsWith("soulTrainUnavailable:"))std::cerr<<action<<'\n';
  if(action.startsWith("firmware:")){
   const auto index=size_t(action.substring(9).getIntValue());
   if(index<p.firmwares.size())p.beginFirmwareLoad(p.firmwares[index].file);
  }
 };
 auto* web=new SoulTrainWeb(p,std::move(cb),juce::File(args[1]));juce::DocumentWindow window("SoulTrain native integration check",juce::Colours::black,0);window.setUsingNativeTitleBar(true);window.setContentOwned(web,true);window.setBounds(0,0,1200,790);window.setVisible(true);web->setBounds(0,0,1200,760);
 waitJS(*web,"Boolean(document.querySelector('.a-pages')&&window.silverReceive)");pump(.5);
 check(bool(js(*web,"document.body.classList.contains('native-mode')")),"Native bridge flag missing");waitJS(*web,"document.querySelector('[aria-label=\"Setup page\"]').parentElement.hidden===false");
 const juce::String setup="document.querySelector('[aria-label=\"Setup page\"]')";
 waitJS(*web,setup+".innerText.includes('KS factory Data MIDI') && "+setup+".innerText.includes('Patches')");
 check(!bool(js(*web,"[..."+setup+".querySelectorAll('button')].some(b=>/^(Load firmware|Wave ROM|KS Data|Load skin|Demo song)/i.test(b.textContent))")),"WebKit still offers a removed Setup action");
 check(bool(js(*web,"(()=>{const heading=getComputedStyle("+setup+".querySelector('.a-shared-heading'));const footer=getComputedStyle(document.querySelector('.a-footer'));return heading.borderBottomColor==='rgba(0, 0, 0, 0)'&&footer.borderTopColor==='rgba(0, 0, 0, 0)'&&footer.boxShadow==='none';})()")),"Setup still has a horizontal divider");
 const juce::String skinTrigger="document.querySelector('[role=combobox][aria-label=\"Skin\"]')";
 waitJS(*web,skinTrigger+"?.textContent.includes('SoulTrain (Default)')");
 js(*web,skinTrigger+".click();true");
 const juce::String skinOptions="document.querySelectorAll('[role=listbox][aria-label=\"Skin\"] [role=option]')";
 waitJS(*web,skinOptions+".length===3");
 js(*web,skinOptions+"[2].click();true");pump(.12);
 check(setupAction=="skin:3","WebKit skin selection did not reach the native bridge");
 js(*web,"[..."+setup+".querySelectorAll('button')].find(b=>b.textContent.startsWith('Choose Silver-K folder')).click();true");pump(.12);
 check(setupAction=="dataFolder","WebKit did not ask for the Silver-K folder");
 js(*web,"[..."+setup+".querySelectorAll('button')].find(b=>b.textContent==='Rescan folders').click();true");pump(.12);
 check(setupAction=="rescanData","WebKit did not ask for a complete scan");
 capture(*web,captures.getChildFile("Setup-data-folder.png"));
 page(*web,"Browser");waitJS(*web,"document.querySelector('[role=combobox][aria-label=\"Instrument\"]:not(:disabled)')!==null");
 js(*web,"document.querySelector('[role=combobox][aria-label=\"Instrument\"]').click();true");
 waitJS(*web,"document.querySelectorAll('[role=listbox][aria-label=\"Instrument\"] [role=option]').length===6");
 js(*web,"document.querySelector('[role=combobox][aria-label=\"Instrument\"]').click();true");
 const auto file=testRoot.getChildFile("Synthetic-ui-library.syx");std::vector<uint8_t> all;for(int bank:{1,2}){auto b=fixture(silverK::Model::KS,bank);all.insert(all.end(),b.begin(),b.end());}auto k=fixture(silverK::Model::KStation);all.insert(all.end(),k.begin(),k.end());check(file.replaceWithData(all.data(),all.size()),"Write synthetic library");p.patchLibrary.addPath(file);
 int source=0;for(int n=0;n<200&&!source;++n){auto data=browserData(browser);for(const auto& item:*data["sources"].getArray())if(item["label"].toString().contains(file.getFileName()))source=int(item["value"]);pump(.025);}check(source>1,"Added library not listed");
 command(*web,msg({{"type","browser"},{"action","source"},{"value",source}}));for(int n=0;n<200&&browserData(browser)["rows"].size()!=3;++n)pump(.025);check(browserData(browser)["rows"].size()==3,"Real library rows missing");
 auto rows=browserData(browser)["rows"];const auto id=rows[0]["id"].toString();
 command(*web,msg({{"type","browser"},{"action","favourite"},{"id",id},{"value",true}}));command(*web,msg({{"type","browser"},{"action","tags"},{"id",id},{"value","native, tested"}}));command(*web,msg({{"type","browser"},{"action","favouritesOnly"},{"value",true}}));
 for(int n=0;n<200&&browserData(browser)["rows"].size()!=1;++n)pump(.025);check(browserData(browser)["rows"].size()==1,"Favourite filter failed");for(int n=0;n<200&&!browserData(browser)["rows"][0]["tags"].toString().contains("tested");++n)pump(.025);check(browserData(browser)["rows"][0]["tags"].toString().contains("tested"),"Tag edit failed");
 command(*web,msg({{"type","browser"},{"action","favouritesOnly"},{"value",false}}));command(*web,msg({{"type","browser"},{"action","bank"},{"value",2}}));for(int n=0;n<100&&browserData(browser)["rows"].size()!=1;++n)pump(.025);check(browserData(browser)["rows"].size()==1,"KS bank filter failed");
 waitJS(*web,"document.querySelector('[role=combobox][aria-label=\"KS bank\"]').dataset.value==='2'&&document.querySelectorAll('.a-native-browser tbody tr').length===1");capture(*web,captures.getChildFile("Browser-native.png"));command(*web,msg({{"type","browser"},{"action","bank"},{"value",1}}));
 const juce::String nameHeader="document.querySelector('.a-patch-table th[data-column=name]')";
 const auto sortButton=nameHeader+".querySelector('button')";
 waitJS(*web,"document.querySelectorAll('.a-patch-row').length===3");
 check(bool(js(*web,"(()=>{const button="+sortButton+";const style=getComputedStyle(button);"
        "const rect=button.getBoundingClientRect();return style.boxShadow==='none' && style.backgroundImage==='none' && "
        "style.borderLeftWidth==='0px' && button.contains(document.elementFromPoint(rect.right-3,rect.y+rect.height/2));})()")),
       "WebKit column header must look flat and accept clicks across its width");
 const juce::String firstName="document.querySelector('.a-patch-row td[data-column=name]').textContent";
 js(*web,sortButton+".click();true");
 waitJS(*web,nameHeader+".getAttribute('aria-sort')==='ascending'");
 const auto ascendingFirst=js(*web,firstName).toString();
 js(*web,sortButton+".click();true");
 waitJS(*web,nameHeader+".getAttribute('aria-sort')==='descending'");
 check(js(*web,firstName).toString()!=ascendingFirst,"WebKit sort direction did not reorder the rows");
 capture(*web,captures.getChildFile("Browser-sort-descending.png"));
 std::cout<<"PASS: WebKit flat header accepts a full-cell click and sorts in both directions\n";
 std::cout<<"PASS: WKWebView resource loading, native bridge, real library add/index, source and KS bank filters, tags and favourite stars\n";
 if(args.size()>=6){check(p.setFlashRom(juce::File(args[4]),error),error);check(p.setWaveRom(juce::File(args[5]),error),error);
 for(int model=2;model<=3;++model){check(p.loadFirmware(juce::File(args[model]),error),error);pump(.5);page(*web,"Main");
 juce::AudioBuffer<float> audio(4,256);juce::MidiBuffer midi;for(int n=0;n<150;++n){audio.clear();p.processBlock(audio,midi);}p.refreshControls();pump(.2);
 checkFirmwareOptions(*web,p);
 double energy=0;juce::MidiBuffer notes;notes.addEvent(juce::MidiMessage::noteOn(1,60,juce::uint8(100)),0);
 for(int n=0;n<90;++n){audio.clear();p.processBlock(audio,notes);notes.clear();for(int channel=0;channel<2;++channel)for(int sample=0;sample<audio.getNumSamples();++sample){const double v=audio.getSample(channel,sample);check(std::isfinite(v),"Non-finite audio");energy+=v*v;}}
 check(energy>1e-7,"No audio from loaded model");notes.addEvent(juce::MidiMessage::allNotesOff(1),0);p.processBlock(audio,notes);
 juce::MemoryBlock initialState;p.getStateInformation(initialState);check(testRoot.getChildFile(p.isKStation()?"K.state":"KS.state").replaceWithData(initialState.getData(),initialState.getSize()),"Save local wrapper fixture");std::cout<<"PASS: "<<(p.isKStation()?"K":"KS")<<" offline note/audio smoke, energy="<<energy<<"\n";
 const int parts=p.isKStation()?1:4;
 for(int owner=1;owner<=parts;++owner)for(const auto& c:p.visibleControls()){
  const auto id=semanticId(c.id);const auto parameterId=p.isKStation()?"k_"+juce::String(c.id):Processor::parameterId(c,unsigned(owner-1));auto* target=p.parameters.getParameter(parameterId);check(target,"Missing APVTS parameter "+parameterId);const auto& range=target->getNormalisableRange();const float raw=range.snapToLegalValue((range.start+range.end)*.5f);
  web->receive(msg({{"type","parameter"},{"id",id},{"part",owner},{"value",raw}}));check(std::abs(target->convertFrom0to1(target->getValue())-raw)<.01f,"Parameter routing "+parameterId);
 }
 auto* cutoff=p.parameters.getParameter(p.isKStation()?"k_cutoff":"cutoff");cutoff->setValueNotifyingHost(cutoff->convertTo0to1(73));pump(.25);check(int(js(*web,"Number(document.querySelector('[data-param=\"cutoff\"] [role=\"slider\"]').getAttribute('aria-valuenow'))"))==73,"Host automation did not update rendered cutoff");
 // Run real JS -> native message, then inspect the host parameter.
 command(*web,msg({{"type","parameter"},{"id","cutoff"},{"part",1},{"value",51}}));check(int(cutoff->convertFrom0to1(cutoff->getValue()))==51,"Native browser parameter message lost");
 capture(*web,captures.getChildFile(p.isKStation()?"K-native.png":"KS-native.png"));page(*web,"Extra");capture(*web,captures.getChildFile(p.isKStation()?"K-Extra-native.png":"KS-Extra-native.png"));if(!p.isKStation()){page(*web,"Performance");
 command(*web,msg({{"type","performance"},{"value",true}}));
 for(int n=0;n<220 && !p.performanceMode();++n){
  audio.clear();
  p.processBlock(audio,midi);
  if(n%8==7)p.refreshControls();
 }
 p.refreshControls();pump(.15);check(p.performanceMode(),"Performance toggle lost");
 for(int n=1;n<=4;++n)command(*web,msg({{"type","partActive"},{"part",n},{"value",true}}));
 command(*web,msg({{"type","part"},{"value",2}}));check(p.currentPart()==1,"Selected part not routed");
 for(int n=0;n<320;++n){audio.clear();p.processBlock(audio,midi);}p.refreshControls();pump(.4);
  check(bool(js(*web,"!document.querySelector('.a-performance-page select, .a-footer-synth select')")),
        "A plugin picker would open a native WebKit menu inside the AU host");
  const auto presetFile=testRoot.getChildFile("Synthetic-performance-preset-menu.syx");
  std::vector<uint8_t> presetData;
  const auto cutoffSpec=std::find_if(std::begin(ksPlugin::controls),std::end(ksPlugin::controls),
      [](const auto& c){return juce::String(c.id)=="cutoff";});
  check(cutoffSpec!=std::end(ksPlugin::controls) && cutoffSpec->patchOffset>=0,"Missing KS cutoff fixture control");
  for(unsigned n=0;n<40;++n){
   auto bytes=fixture(silverK::Model::KS,1);
   const auto name=("Test Preset "+juce::String(n).paddedLeft('0',2)).toStdString();
   bytes[12]=uint8_t(n);
   std::fill_n(bytes.begin()+13+151,16,' ');
   std::copy(name.begin(),name.end(),bytes.begin()+13+151);
   auto& parameter=bytes[13+cutoffSpec->patchOffset];
   parameter=uint8_t((parameter&~cutoffSpec->patchMask)|(((40+n)<<cutoffSpec->patchShift)&cutoffSpec->patchMask));
   presetData.insert(presetData.end(),bytes.begin(),bytes.end());
  }
  check(presetFile.replaceWithData(presetData.data(),presetData.size()),"Write Performance preset fixture");
  p.patchLibrary.addPath(presetFile);
  int presetSource=0;
  for(int attempt=0;attempt<200 && !presetSource;++attempt){
   const auto snapshot=browserData(browser);
   for(const auto& entry:*snapshot["sources"].getArray())
    if(entry["label"].toString().contains(presetFile.getFileName()))presetSource=int(entry["value"]);
   pump(.025);
  }
  check(presetSource>1,"Performance preset source did not load");
  command(*web,msg({{"type","browser"},{"action","source"},{"value",presetSource}}));
  for(int attempt=0;attempt<200 && browserData(browser)["rows"].size()!=40;++attempt)pump(.025);
  check(browserData(browser)["rows"].size()==40,"Performance preset fixture count");
  const auto before=p.partNames();
  for(const auto part:{2,4}){
   const auto label="Part "+juce::String(part)+" preset";
   const auto button="button[role=combobox][aria-label='"+label+"']";
   const auto list="[role=listbox][aria-label='"+label+"']";
   const auto select=[&](const juce::String& selector){return "document.querySelector("+juce::JSON::toString(selector)+")";};
   waitJS(*web,"Boolean("+select(button+":not(:disabled)")+")");
   const auto popup=select(list);
   const auto ready="((menu)=>menu && menu.children.length===40 && menu.scrollHeight>menu.clientHeight)("+popup+")";
   for(int attempt=0;attempt<15 && !bool(js(*web,ready));++attempt){
    if(!bool(js(*web,"Boolean("+popup+")")))js(*web,select(button)+".click();true");
    pump(.08);
   }
   waitJS(*web,ready);
   if(part==2){
    js(*web,"((menu)=>{menu.scrollTop=menu.scrollHeight;return true})("+popup+")");
    waitJS(*web,popup+"?.scrollTop>0");
   }else{
    js(*web,popup+".dispatchEvent(new KeyboardEvent('keydown',{key:'Home',bubbles:true}));true");
    waitJS(*web,"((menu)=>menu.getAttribute('aria-activedescendant')===menu.firstElementChild.id)("+popup+")");
   }
   const auto option=part==2?"lastElementChild":"firstElementChild";
   const auto selected=js(*web,popup+"."+option+".dataset.value").toString();
   const auto snapshot=browserData(browser);
   juce::String expected;
   for(const auto& row:*snapshot["rows"].getArray())if(row["id"].toString()==selected)expected=row["name"].toString();
   check(expected.startsWith("Test Preset "),"Missing selected Performance preset");
   if(part==2)js(*web,popup+".lastElementChild.click();true");
   else js(*web,popup+".dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',bubbles:true}));true");
   waitJS(*web,"!"+popup);
   pump(.1);
   const auto previous=p.partNames();
   for(int block=0;block<150;++block){audio.clear();p.processBlock(audio,midi);}
   p.refreshControls();pump(.15);
   const auto after=p.partNames();
   for(unsigned i=0;i<after.size();++i)
    check(after[i]==(i==unsigned(part-1)?expected:previous[i]),"Performance preset changed the wrong part or failed to load");
  }
  check(p.partNames()[0]==before[0] && p.partNames()[2]==before[2],"Untargeted Performance parts changed");
  std::cout<<"PASS: scrollable Performance preset picker loads separate parts by mouse and keyboard\n";
  capture(*web,captures.getChildFile("KS-Performance-native.png"));}page(*web,"Setup");capture(*web,captures.getChildFile(p.isKStation()?"K-Setup-native.png":"KS-Setup-native.png"));
 juce::MemoryBlock state;p.getStateInformation(state);check(state.getSize()>0,"State missing");p.setStateInformation(state.getData(),int(state.getSize()));pump(.1);std::cout<<"PASS: "<<(p.isKStation()?"K 154":"KS 182 x 4")<<" real parameters, JS/native/automation roundtrip and state\n";
 }
 page(*web,"Browser");
 const juce::String footerPicker="document.querySelector('button[aria-label=\"Select firmware\"]')";
 for(int i=0;i<4;++i){
  const bool wantK=(i%2)==0;
  const auto item=std::find_if(p.firmwares.begin(),p.firmwares.end(),[&](const auto& entry){return entry.kStation==wantK;});
  check(item!=p.firmwares.end(),"Missing WebKit firmware option");
  const int row=int(item-p.firmwares.begin());
  js(*web,footerPicker+".click();true");
  const juce::String options="document.querySelectorAll('[role=listbox][aria-label=\"Select firmware\"] [role=option]')";
  waitJS(*web,options+".length==="+juce::String(p.firmwares.size()));
  const auto revision=p.firmwareLoadRevision();
  js(*web,options+"["+juce::String(row)+"].click();true");
  for(int poll=0;poll<400 && p.firmwareLoadRevision()==revision;++poll)pump(.025);
  check(p.firmwareLoadRevision()!=revision && p.firmwareReady() && p.isKStation()==wantK,p.status());
  pump(.25);
  const auto active=js(*web,"document.querySelector('.a-pages [aria-current=page]')?.textContent").toString();
  check(active=="Browser","WebKit left Browser on firmware switch: "+active);
 }
 std::cout<<"PASS: WebKit Browser remains selected through four firmware switches\n";
 }
 command(*web,msg({{"type","browser"},{"action","removeSource"}}));pump(.5);check(file.existsAsFile(),"Remove source deleted original data");
 window.setVisible(false);window.clearContentComponent();p.releaseResources();std::cout<<"PASS: source removal retains file; native browser shutdown\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#if JUCE_WINDOWS
int wmain(int argc,wchar_t** argv){
 juce::StringArray args;
 for(int i=0;i<argc;++i)args.add(juce::String(argv[i]));
 return run(args);
}
#else
int main(int argc,char** argv){
 @autoreleasepool{
  juce::StringArray args;
  for(int i=0;i<argc;++i)args.add(juce::String::fromUTF8(argv[i]));
  return run(args);
 }
}
#endif
