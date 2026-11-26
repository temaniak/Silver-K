#include "silverSoulTrain.h"
#include "silverPatch.h"

namespace silverK {
void SoulTrainWeb::closeBridge(){
 stopTimer();
 for(const auto& item:gestures)item.second->endChangeGesture();
 gestures.clear();
}
void SoulTrainWeb::loaded(){const juce::Component::SafePointer<SoulTrainWeb> self(this);evaluate("Boolean(window.silverReceive)",[self](juce::var value){if(self&&bool(value)){self->ready=true;self->lastSnapshot.clear();self->timerCallback();}});}
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
  else if(name=="firmwareSwitch")processor.setFirmwareSwitch(Processor::FirmwareSwitch(v));
  else if(name=="twoCore"){juce::String e;if(!processor.setKSTwoCoreProcessing(bool(message["value"]),e))error=e;}
  else if(name=="controller"){juce::String e;if(!processor.setKSControllerMode(v,e))error=e;}
 }
 lastSnapshot.clear();
}
juce::var SoulTrainWeb::snapshot(bool library){
 auto* o=new juce::DynamicObject;o->setProperty("model",processor.isKStation()?"K":"KS");o->setProperty("part",int(processor.currentPart()+1));o->setProperty("ready",processor.firmwareReady());o->setProperty("loading",processor.isFirmwareLoading());o->setProperty("performance",processor.performanceMode());o->setProperty("clock",int(processor.clockSource()));o->setProperty("status",error.isEmpty()?(callbacks.nativeStatus?callbacks.nativeStatus():processor.status()):error);o->setProperty("name",processor.currentName());o->setProperty("buffer",processor.bufferProfile());o->setProperty("twoCore",processor.ksTwoCoreProcessing());o->setProperty("controller",processor.ksControllerMode());o->setProperty("controllerReference",processor.ksControllerChoices());
 o->setProperty("firmwareSwitch",int(processor.firmwareSwitch()));
 o->setProperty("dataFolder",processor.dataFolder.getFullPathName());
 o->setProperty("dataFolderStatus",processor.dataFolderStatus());
 if(callbacks.skinMenu)o->setProperty("skinMenu",callbacks.skinMenu());
 juce::Array<juce::var> active,names;const auto partNames=processor.partNames();for(unsigned n=0;n<4;++n){active.add((processor.activeParts()&(1<<n))!=0);names.add(partNames[n]);}o->setProperty("enabled",active);o->setProperty("partNames",names);
 auto* values=new juce::DynamicObject;for(const auto& spec:processor.visibleControls())for(int part=1;part<=(processor.isKStation()?1:4);++part)if(auto* p=processor.parameters.getParameter(processor.isKStation()?"k_"+juce::String(spec.id):Processor::parameterId(spec,unsigned(part-1))))values->setProperty(key(spec.id,part),juce::roundToInt(p->convertFrom0to1(p->getValue())));o->setProperty("values",juce::var(values));
 juce::Array<juce::var> firmwares;for(size_t n=0;n<processor.firmwares.size();++n){const auto& f=processor.firmwares[n];auto* item=new juce::DynamicObject;item->setProperty("value",int(n));item->setProperty("label",f.file.getFileName());item->setProperty("model",f.kStation?"K":"KS");item->setProperty("selected",f.file==processor.osFile);firmwares.add(juce::var(item));}o->setProperty("firmwares",firmwares);
 o->setProperty("uiScale",processor.editorScale.load());const auto drum=processor.drumEditInfo();o->setProperty("drumBank",processor.drumEditSupported()?int(drum.bank):0);o->setProperty("drumKey",int(drum.note));o->setProperty("drumEdit",processor.drumEditEnabled());
 if(library&&callbacks.browserSnapshot){const auto data=callbacks.browserSnapshot();const auto json=juce::JSON::toString(data,true);if(json!=lastLibrary){o->setProperty("browser",data);lastLibrary=json;}}return juce::var(o);
}
void SoulTrainWeb::timerCallback(){if(!ready||!isShowing())return;processor.refreshControls();const bool library=includeLibrary&&((tick++%5)==0);const auto json=juce::JSON::toString(snapshot(library),true);if(json==lastSnapshot)return;lastSnapshot=json;evaluate("window.silverReceive&&window.silverReceive("+json+");");}
}
