#include "silverSoulTrain.h"
#include "silverBrowser.h"
#include "silverEditor.h"
#include "silverPatch.h"
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>
#include <iostream>
#include <stdexcept>
using namespace silverK;
static void check(bool value,const juce::String& reason){if(!value)throw std::runtime_error(reason.toStdString());}
static void pump(double seconds=.1){const auto end=juce::Time::getMillisecondCounterHiRes()+seconds*1000;while(juce::Time::getMillisecondCounterHiRes()<end)CFRunLoopRunInMode(kCFRunLoopDefaultMode,.01,true);}
static juce::var js(SoulTrainWeb& web,const juce::String& code){bool done=false;juce::var result;web.evaluate(code,[&](juce::var value){result=std::move(value);done=true;});for(int n=0;n<400&&!done;++n)pump(.01);check(done,"WebKit script timed out");return result;}
static void waitJS(SoulTrainWeb& web,const juce::String& code){for(int n=0;n<160;++n){if(bool(js(web,code)))return;pump(.05);}throw std::runtime_error("WebKit condition failed: "+code.toStdString()+"; body="+js(web,"document.body.innerText").toString().substring(0,400).toStdString());}
static void command(SoulTrainWeb& web,const juce::var& data){js(web,"window.silverSend("+juce::JSON::toString(data,true)+");true");pump(.08);}
static juce::var msg(std::initializer_list<std::pair<juce::String,juce::var>> list){auto* o=new juce::DynamicObject;for(auto& v:list)o->setProperty(v.first,v.second);return o;}
static void page(SoulTrainWeb& web,const juce::String& name){js(web,"Array.from(document.querySelectorAll('.a-pages button')).find(b=>b.textContent==="+juce::JSON::toString(name)+").click();true");pump(.35);}
static void capture(SoulTrainWeb& web,const juce::File& file){
 auto* native=dynamic_cast<juce::NSViewComponent*>(web.getChildComponent(0));check(native,"No native WebKit view");auto* view=(__bridge WKWebView*)native->getView();__block bool done=false;
 [view takeSnapshotWithConfiguration:nil completionHandler:^(NSImage* image,NSError* error){if(image){NSBitmapImageRep* rep=[[NSBitmapImageRep alloc] initWithData:image.TIFFRepresentation];NSData* png=[rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];[png writeToFile:[NSString stringWithUTF8String:file.getFullPathName().toRawUTF8()] atomically:YES];}done=true;}];
 for(int n=0;n<300&&!done;++n)pump(.01);check(file.existsAsFile(),"WebKit screenshot failed");
}
static std::vector<uint8_t> fixture(silverK::Model model,int bank=0){
 using silverK::Model;std::vector<uint8_t> bytes(model==Model::KStation?142:270,0);const uint8_t header[]{0xf0,0,0x20,0x29,1,uint8_t(model==Model::KStation?0x41:0x42),0x7f,uint8_t(bank?1:0),0,0x11,uint8_t(model==Model::KStation?1:2),uint8_t(bank),7};std::copy_n(header,13,bytes.begin());bytes.back()=0xf7;
 const auto controls=model==Model::KStation?kControls():std::vector<Control>(std::begin(ksPlugin::controls),std::end(ksPlugin::controls));for(const auto& c:controls)if(c.patchOffset>=0){auto& b=bytes[13+c.patchOffset];b=uint8_t((b&~c.patchMask)|((c.defaultValue<<c.patchShift)&c.patchMask));}
 if(bytes.size()==270){std::fill_n(bytes.begin()+164,16,' ');const std::string name="UI test bank "+std::to_string(bank);std::copy(name.begin(),name.end(),bytes.begin()+164);}return bytes;
}
static juce::var browserData(Browser& browser){browser.pollLibrary();return browser.webSnapshot();}
int main(int argc,char** argv){@autoreleasepool{try{
 check(argc>=2,"Pass compiled frontend directory, then optional K / KS firmware / KS flash / waves");[NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory];[NSApp finishLaunching];juce::ScopedJuceInitialiser_GUI gui;
 const auto testRoot=juce::File(juce::SystemStats::getEnvironmentVariable("SILVER_K_DATA","/tmp/soultrain-test-data"));testRoot.createDirectory();const auto captures=testRoot.getChildFile("captures");captures.createDirectory();
 Processor p;p.prepareToPlay(48000,256);p.setNonRealtime(true);juce::String error;check(p.setKSControllerMode(1,error),error);Browser browser(p);
 SoulTrainWeb::Callbacks cb;cb.browserSnapshot=[&]{return browserData(browser);};cb.browserAction=[&](const juce::var& v){browser.webAction(v);};
 auto* web=new SoulTrainWeb(p,std::move(cb),juce::File(juce::String::fromUTF8(argv[1])));juce::DocumentWindow window("SoulTrain native integration check",juce::Colours::black,0);window.setUsingNativeTitleBar(true);window.setContentOwned(web,true);window.setBounds(0,0,1200,790);window.setVisible(true);web->setBounds(0,0,1200,760);
 waitJS(*web,"Boolean(document.querySelector('.a-pages')&&window.silverReceive)");pump(.5);
 check(bool(js(*web,"document.body.classList.contains('native-mode')")),"Native bridge flag missing");waitJS(*web,"document.querySelector('[aria-label=\"Setup page\"]').parentElement.hidden===false");
 page(*web,"Browser");waitJS(*web,"document.querySelector('[aria-label=\"Instrument\"]').options.length===6");
 const auto file=testRoot.getChildFile("Synthetic-ui-library.syx");std::vector<uint8_t> all;for(int bank:{1,2}){auto b=fixture(silverK::Model::KS,bank);all.insert(all.end(),b.begin(),b.end());}auto k=fixture(silverK::Model::KStation);all.insert(all.end(),k.begin(),k.end());check(file.replaceWithData(all.data(),all.size()),"Write synthetic library");p.patchLibrary.addPath(file);
 int source=0;for(int n=0;n<200&&!source;++n){auto data=browserData(browser);for(const auto& item:*data["sources"].getArray())if(item["label"].toString().contains(file.getFileName()))source=int(item["value"]);pump(.025);}check(source>1,"Added library not listed");
 command(*web,msg({{"type","browser"},{"action","source"},{"value",source}}));for(int n=0;n<200&&browserData(browser)["rows"].size()!=3;++n)pump(.025);check(browserData(browser)["rows"].size()==3,"Real library rows missing");
 auto rows=browserData(browser)["rows"];const auto id=rows[0]["id"].toString();
 command(*web,msg({{"type","browser"},{"action","favourite"},{"id",id},{"value",true}}));command(*web,msg({{"type","browser"},{"action","tags"},{"id",id},{"value","native, tested"}}));command(*web,msg({{"type","browser"},{"action","favouritesOnly"},{"value",true}}));
 for(int n=0;n<200&&browserData(browser)["rows"].size()!=1;++n)pump(.025);check(browserData(browser)["rows"].size()==1,"Favourite filter failed");for(int n=0;n<200&&!browserData(browser)["rows"][0]["tags"].toString().contains("tested");++n)pump(.025);check(browserData(browser)["rows"][0]["tags"].toString().contains("tested"),"Tag edit failed");
 command(*web,msg({{"type","browser"},{"action","favouritesOnly"},{"value",false}}));command(*web,msg({{"type","browser"},{"action","bank"},{"value",2}}));for(int n=0;n<100&&browserData(browser)["rows"].size()!=1;++n)pump(.025);check(browserData(browser)["rows"].size()==1,"KS bank filter failed");
 waitJS(*web,"document.querySelector('[aria-label=\"KS bank\"]').value==='2'&&document.querySelectorAll('.a-native-browser tbody tr').length===1");capture(*web,captures.getChildFile("Browser-native.png"));command(*web,msg({{"type","browser"},{"action","bank"},{"value",1}}));
 std::cout<<"PASS: WKWebView resource loading, native bridge, real library add/index, source and KS bank filters, tags and favourite stars\n";
 if(argc>=6){check(p.setFlashRom(juce::File(juce::String::fromUTF8(argv[4])),error),error);check(p.setWaveRom(juce::File(juce::String::fromUTF8(argv[5])),error),error);
 for(int model=2;model<=3;++model){check(p.loadFirmware(juce::File(juce::String::fromUTF8(argv[model])),error),error);pump(.5);page(*web,"Main");
 juce::AudioBuffer<float> audio(4,256);juce::MidiBuffer midi;for(int n=0;n<150;++n){audio.clear();p.processBlock(audio,midi);}p.refreshControls();pump(.2);
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
 command(*web,msg({{"type","parameter"},{"id","cutoff"},{"part",1},{"value",51}}));check(int(cutoff->convertFrom0to1(cutoff->getValue()))==51,"WebKit parameter message lost");
 capture(*web,captures.getChildFile(p.isKStation()?"K-native.png":"KS-native.png"));page(*web,"Extra");capture(*web,captures.getChildFile(p.isKStation()?"K-Extra-native.png":"KS-Extra-native.png"));page(*web,"Performance");if(!p.isKStation()){
 command(*web,msg({{"type","performance"},{"value",true}}));for(int n=0;n<40;++n){audio.clear();p.processBlock(audio,midi);}p.refreshControls();pump(.15);check(p.performanceMode(),"Performance toggle lost");
 for(int n=1;n<=4;++n)command(*web,msg({{"type","partActive"},{"part",n},{"value",true}}));
 command(*web,msg({{"type","part"},{"value",2}}));check(p.currentPart()==1,"Selected part not routed");
 for(int n=0;n<60;++n){audio.clear();p.processBlock(audio,midi);}p.refreshControls();pump(.4);
 }capture(*web,captures.getChildFile(p.isKStation()?"K-Performance-native.png":"KS-Performance-native.png"));page(*web,"Setup");capture(*web,captures.getChildFile(p.isKStation()?"K-Setup-native.png":"KS-Setup-native.png"));
 juce::MemoryBlock state;p.getStateInformation(state);check(state.getSize()>0,"State missing");p.setStateInformation(state.getData(),int(state.getSize()));pump(.1);std::cout<<"PASS: "<<(p.isKStation()?"K 154":"KS 182 x 4")<<" real parameters, JS/native/automation roundtrip, state and page screenshots\n";
 }}
 command(*web,msg({{"type","browser"},{"action","removeSource"}}));pump(.5);check(file.existsAsFile(),"Remove source deleted original data");
 window.setVisible(false);window.clearContentComponent();p.releaseResources();std::cout<<"PASS: source removal retains file; WebKit shutdown\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}}
