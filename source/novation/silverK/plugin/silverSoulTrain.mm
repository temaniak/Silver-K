#include "silverSoulTrain.h"
#include "silverPatch.h"
#import <Cocoa/Cocoa.h>
#import <WebKit/WebKit.h>

@interface NKSilverSoulTrainHandler : NSObject<WKScriptMessageHandler,WKURLSchemeHandler,WKNavigationDelegate>
@property(nonatomic,assign) silverK::SoulTrainWeb* owner;
@property(nonatomic,strong) NSString* resourcePath;
@end
@implementation NKSilverSoulTrainHandler
-(void)userContentController:(WKUserContentController*)controller didReceiveScriptMessage:(WKScriptMessage*)message {
 if(!self.owner || !message.frameInfo.mainFrame || ![message.frameInfo.request.URL.scheme isEqualToString:@"silver-soultrain"])return;
 if(![NSJSONSerialization isValidJSONObject:message.body])return;
 NSData* data=[NSJSONSerialization dataWithJSONObject:message.body options:0 error:nil];
 self.owner->receive(juce::JSON::parse(juce::String::fromUTF8((const char*)data.bytes,int(data.length))));
}
-(void)webView:(WKWebView*)view startURLSchemeTask:(id<WKURLSchemeTask>)task {
 const auto root=juce::File(juce::String::fromUTF8(self.resourcePath.UTF8String));
 juce::String relative=juce::String::fromUTF8(task.request.URL.path.UTF8String);while(relative.startsWithChar('/'))relative=relative.substring(1);
 if(relative.isEmpty())relative="index.html";
 const auto file=root.getChildFile(relative);
 if(!file.isAChildOf(root) || !file.existsAsFile()) {[task didFailWithError:[NSError errorWithDomain:@"SilverSoulTrain" code:404 userInfo:nil]];return;}
 NSString* mime=@"application/octet-stream";const auto ext=file.getFileExtension().toLowerCase();
 if(ext==".html")mime=@"text/html";else if(ext==".js"||ext==".mjs")mime=@"text/javascript";else if(ext==".css")mime=@"text/css";else if(ext==".json")mime=@"application/json";else if(ext==".svg")mime=@"image/svg+xml";else if(ext==".png")mime=@"image/png";else if(ext==".ttf")mime=@"font/ttf";
 NSData* data=[NSData dataWithContentsOfFile:[NSString stringWithUTF8String:file.getFullPathName().toRawUTF8()]];
 const bool text=ext==".html"||ext==".js"||ext==".mjs"||ext==".css"||ext==".json"||ext==".svg";
 NSString* type=text?[mime stringByAppendingString:@"; charset=utf-8"]:mime;
 [task didReceiveResponse:[[NSHTTPURLResponse alloc] initWithURL:task.request.URL statusCode:200 HTTPVersion:@"HTTP/1.1" headerFields:@{@"Content-Type":type}]];[task didReceiveData:data];[task didFinish];
}
-(void)webView:(WKWebView*)view stopURLSchemeTask:(id<WKURLSchemeTask>)task {}
-(void)webView:(WKWebView*)view didFinishNavigation:(WKNavigation*)navigation {if(self.owner)self.owner->loaded();}
-(void)webView:(WKWebView*)view decidePolicyForNavigationAction:(WKNavigationAction*)action decisionHandler:(void (^)(WKNavigationActionPolicy))decision {
 decision([action.request.URL.scheme isEqualToString:@"silver-soultrain"]?WKNavigationActionPolicyAllow:WKNavigationActionPolicyCancel);
}
@end
namespace silverK {
struct SoulTrainWeb::Impl {
 juce::NSViewComponent component;
 NKSilverSoulTrainHandler* __strong handler;
 WKWebView* __strong view;
 Impl(SoulTrainWeb& owner,juce::File root) {
  handler=[NKSilverSoulTrainHandler new];handler.owner=&owner;
  if(root==juce::File{}){NSBundle* bundle=[NSBundle bundleForClass:[NKSilverSoulTrainHandler class]];root=juce::File(juce::String::fromUTF8(bundle.resourcePath.UTF8String)).getChildFile("SoulTrain");}
  handler.resourcePath=[NSString stringWithUTF8String:root.getFullPathName().toRawUTF8()];
  WKWebViewConfiguration* configuration=[WKWebViewConfiguration new];
  configuration.websiteDataStore=[WKWebsiteDataStore nonPersistentDataStore];
  [configuration setURLSchemeHandler:handler forURLScheme:@"silver-soultrain"];
  [configuration.userContentController addScriptMessageHandler:handler name:@"silver"];
  NSString* script=@"window.__SILVER_NATIVE__=true;window.silverSend=(data)=>window.webkit.messageHandlers.silver.postMessage(data);";
  [configuration.userContentController addUserScript:[[WKUserScript alloc] initWithSource:script injectionTime:WKUserScriptInjectionTimeAtDocumentStart forMainFrameOnly:YES]];
  view=[[WKWebView alloc] initWithFrame:NSMakeRect(0,0,1486,942) configuration:configuration];view.navigationDelegate=handler;
  [view setValue:@NO forKey:@"drawsBackground"];
  component.setView((__bridge void*)view);owner.addAndMakeVisible(component);
  [view loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"silver-soultrain://ui/index.html"]]];
 }
 ~Impl(){handler.owner=nullptr;view.navigationDelegate=nil;[view stopLoading];[view.configuration.userContentController removeScriptMessageHandlerForName:@"silver"];component.setView(nullptr);}
};
SoulTrainWeb::SoulTrainWeb(Processor& p,Callbacks cb,juce::File root):processor(p),callbacks(std::move(cb)){setComponentID("soulTrainWeb");impl=std::make_unique<Impl>(*this,root);startTimerHz(20);}
SoulTrainWeb::~SoulTrainWeb(){stopTimer();for(const auto& item:gestures)item.second->endChangeGesture();gestures.clear();impl.reset();}
void SoulTrainWeb::resized(){impl->component.setBounds(getLocalBounds());}
void SoulTrainWeb::loaded(){const juce::Component::SafePointer<SoulTrainWeb> self(this);evaluate("Boolean(window.silverReceive)",[self](juce::var value){if(self&&bool(value)){self->ready=true;self->lastSnapshot.clear();self->timerCallback();}});}
void SoulTrainWeb::evaluate(const juce::String& code,std::function<void(juce::var)> completion){
 [impl->view evaluateJavaScript:[NSString stringWithUTF8String:code.toRawUTF8()] completionHandler:^(id value,NSError* failure){if(!completion)return;if(failure){completion({});return;}if([NSJSONSerialization isValidJSONObject:value]){NSData* data=[NSJSONSerialization dataWithJSONObject:value options:NSJSONWritingFragmentsAllowed error:nil];completion(juce::JSON::parse(juce::String::fromUTF8((const char*)data.bytes,int(data.length))));}else if([value isKindOfClass:[NSString class]])completion(juce::String::fromUTF8([(NSString*)value UTF8String]));else if([value isKindOfClass:[NSNumber class]])completion([(NSNumber*)value doubleValue]);else completion({});}];
}
juce::String SoulTrainWeb::key(const juce::String& id,int owner)const{return (processor.isKStation()?"K:":"KS:")+juce::String(owner)+":"+semanticId(id);}
juce::RangedAudioParameter* SoulTrainWeb::parameter(const juce::String& id,int owner){
 if(owner<1||owner>4||(processor.isKStation()&&owner!=1))return nullptr;
 for(const auto& spec:processor.visibleControls())if(semanticId(spec.id)==id)return processor.parameters.getParameter(processor.isKStation()?"k_"+juce::String(spec.id):Processor::parameterId(spec,unsigned(owner-1)));
 return nullptr;
}
void SoulTrainWeb::receive(const juce::var& message){
 jassert(juce::MessageManager::getInstance()->isThisTheMessageThread());
 const auto type=message["type"].toString();
 if(type=="hello"){ready=true;lastSnapshot.clear();}
 else if(type=="page"){includeLibrary=message["value"].toString()=="Browser"||message["value"].toString()=="Performance";lastSnapshot.clear();lastLibrary.clear();}
 else if(type=="browser"){if(callbacks.browserAction)callbacks.browserAction(message);}
 else if(type=="drumEdit"){processor.setDrumEdit(bool(message["value"]));}
 else if(type=="native"){if(callbacks.nativeAction)callbacks.nativeAction(message["action"].toString());}
 else if(type=="part"){const int n=int(message["value"]);if(n>=1&&n<=4&&(!processor.isKStation())&&(n==1||processor.performanceMode()))processor.selectPart(unsigned(n-1));}
 else if(type=="parameter"||type=="gestureBegin"||type=="gestureEnd"){
  const auto id=message["id"].toString();const int part=int(message["part"]);auto* target=parameter(id,part);if(!target || !processor.firmwareReady() || processor.isFirmwareLoading())return;const auto name=key(id,part);
  if(type=="gestureBegin"){if(!gestures.count(name)){target->beginChangeGesture();gestures[name]=target;}}
  else if(type=="gestureEnd"){auto it=gestures.find(name);if(it!=gestures.end()){it->second->endChangeGesture();gestures.erase(it);}}
  else{const double raw=double(message["value"]);if(!std::isfinite(raw))return;const auto& range=target->getNormalisableRange();const float value=range.snapToLegalValue(juce::jlimit(range.start,range.end,float(raw)));const bool own=!gestures.count(name);if(own)target->beginChangeGesture();target->setValueNotifyingHost(target->convertTo0to1(value));if(own)target->endChangeGesture();}
 }
 else if(type=="clock"){const int n=int(message["value"]);if(n>=0&&n<=2)processor.setClockSource(ClockSource(n));}
 else if(type=="performance"){juce::String e;if(!processor.setPerformanceMode(bool(message["value"]),e))error=e;}
 else if(type=="partActive"){juce::String e;const int n=int(message["part"]);if(n>=1&&n<=4&&!processor.setPartActive(unsigned(n-1),bool(message["value"]),e))error=e;}
 else if(type=="setting"){
  const auto name=message["name"].toString();const int v=int(message["value"]);
  if(name=="buffer"&&v>=0&&v<=3)processor.setBufferProfile(v);
  else if(name=="channel"&&v>=1&&v<=16)processor.setEditorChannel(uint8_t(v-1));
  else if(name=="twoCore"){juce::String e;if(!processor.setKSTwoCoreProcessing(bool(message["value"]),e))error=e;}
  else if(name=="controller"){juce::String e;if(!processor.setKSControllerMode(v,e))error=e;}
 }
 lastSnapshot.clear();
}
juce::var SoulTrainWeb::snapshot(bool library){
 auto* o=new juce::DynamicObject;o->setProperty("model",processor.isKStation()?"K":"KS");o->setProperty("part",int(processor.currentPart()+1));o->setProperty("ready",processor.firmwareReady());o->setProperty("loading",processor.isFirmwareLoading());o->setProperty("performance",processor.performanceMode());o->setProperty("clock",int(processor.clockSource()));o->setProperty("status",error.isEmpty()?(callbacks.nativeStatus?callbacks.nativeStatus():processor.status()):error);o->setProperty("name",processor.currentName());o->setProperty("buffer",processor.bufferProfile());o->setProperty("channel",int(processor.editorChannel()+1));o->setProperty("twoCore",processor.ksTwoCoreProcessing());o->setProperty("controller",processor.ksControllerMode());o->setProperty("controllerReference",ks::boardComparisonAvailable());
 juce::Array<juce::var> active,names;const auto partNames=processor.partNames();for(unsigned n=0;n<4;++n){active.add((processor.activeParts()&(1<<n))!=0);names.add(partNames[n]);}o->setProperty("enabled",active);o->setProperty("partNames",names);
 auto* values=new juce::DynamicObject;for(const auto& spec:processor.visibleControls())for(int part=1;part<=(processor.isKStation()?1:4);++part)if(auto* p=processor.parameters.getParameter(processor.isKStation()?"k_"+juce::String(spec.id):Processor::parameterId(spec,unsigned(part-1))))values->setProperty(key(spec.id,part),juce::roundToInt(p->convertFrom0to1(p->getValue())));o->setProperty("values",juce::var(values));
 juce::Array<juce::var> firmwares;for(size_t n=0;n<processor.firmwares.size();++n){const auto& f=processor.firmwares[n];auto* item=new juce::DynamicObject;item->setProperty("value",int(n));item->setProperty("label",f.file.getFileName());item->setProperty("model",f.kStation?"K":"KS");item->setProperty("selected",f.file==processor.osFile);firmwares.add(juce::var(item));}o->setProperty("firmwares",firmwares);
 o->setProperty("uiScale",processor.editorScale.load());o->setProperty("demoSong",processor.demoSongActive());const auto drum=processor.drumEditInfo();o->setProperty("drumBank",int(drum.bank));o->setProperty("drumKey",int(drum.note));o->setProperty("drumEdit",processor.drumEditEnabled());
 if(library&&callbacks.browserSnapshot){const auto data=callbacks.browserSnapshot();const auto json=juce::JSON::toString(data,true);if(json!=lastLibrary){o->setProperty("browser",data);lastLibrary=json;}}return juce::var(o);
}
void SoulTrainWeb::timerCallback(){if(!ready||!isShowing())return;const bool library=includeLibrary&&((tick++%5)==0);const auto json=juce::JSON::toString(snapshot(library),true);if(json==lastSnapshot)return;lastSnapshot=json;evaluate("window.silverReceive&&window.silverReceive("+json+");");}
}
