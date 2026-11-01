#pragma once
#include <cstdint>
namespace ks {
// Semantic command preparation: select a voice/part window, resolve its DSP
// address, or publish a command to the already-native transport. All mutable
// mirrors are observed at their admitted times; no cached-address shortcut.
class DspRoutingTask final {
public:
 enum class Kind {PrimeRelative,WriteRelative,ReadAlternate,VoiceB,VoiceA,PrimeAbsolute,WriteAbsolute,WriteA,WriteB};
 enum class Stage {ReadPrimary,TestSign,AdmitSign,ReadAlternate,ResolveAddress,DispatchWindow,AwaitWindow,
  ReadVoice,ReadBase,Difference,AdmitDifference,ClampDifference,NormalizeVoice,
  PrimaryFactor,PrimaryProduct,PublishPrimary,AlternateFactor,AlternateProduct,PublishAlternate,DispatchSelect,AwaitSelect,
  PublishAddress,DispatchAddress,AwaitAddress,SelectPort,SelectDelay,ReadCached,CompareCached,DispatchWrite,EmitWrite,Complete};
 enum class Read {PrimaryOffset,AlternateOffset,VoiceB,VoiceA,VoiceBase,CachedAddress};
 enum class Write {PrimaryOffset,AlternateOffset,CachedAddress};
 enum class Request {None,PrimeAddress,WriteAddress,ReadAddress,SelectA,SelectB,Address,AddressThenWord};
 struct Advance {unsigned clocks=0,boundaries=0;};
 explicit DspRoutingTask(Kind kind,uint16_t address=0):kind_(kind),relative_(address),address_(address),stage_(initial(kind)){}
 Kind kind()const{return kind_;}Stage stage()const{return stage_;}
 uint16_t offset()const{return offset_;}uint16_t address()const{return address_;}uint16_t relative()const{return relative_;}uint16_t cached()const{return cached_;}
 uint8_t voice()const{return voice_;}uint8_t baseVoice()const{return base_;}uint8_t difference()const{return difference_;}
 uint32_t product()const{return product_;}
 unsigned primaryFactor()const{return kind_==Kind::VoiceB?101u:53u;}
 unsigned alternateFactor()const{return kind_==Kind::VoiceB?22u:5u;}
 bool negativeAddress()const{return (relative_&0x8000)!=0;}
 bool negativeDifference()const{return (difference_&0x80)!=0;}
 bool complete()const{return stage_==Stage::Complete||stage_==Stage::EmitWrite;}
 Request request()const{
  switch(stage_){case Stage::AwaitWindow:return kind_==Kind::PrimeRelative?Request::PrimeAddress:kind_==Kind::WriteRelative?Request::WriteAddress:Request::ReadAddress;
  case Stage::AwaitSelect:return kind_==Kind::VoiceB?Request::SelectB:Request::SelectA;
  case Stage::AwaitAddress:return Request::Address;case Stage::EmitWrite:return Request::AddressThenWord;default:return Request::None;}
 }
 bool resumeChild(){if(stage_==Stage::AwaitWindow||stage_==Stage::AwaitSelect||stage_==Stage::AwaitAddress){stage_=Stage::Complete;return true;}return false;}
 unsigned nextClocks()const{
  switch(stage_){
  case Stage::ReadPrimary:case Stage::ReadAlternate:case Stage::ReadVoice:case Stage::ReadBase:case Stage::ReadCached:
  case Stage::PublishPrimary:case Stage::PublishAlternate:case Stage::PublishAddress:return 12;
  case Stage::DispatchWindow:case Stage::DispatchSelect:case Stage::DispatchAddress:case Stage::DispatchWrite:return 20;
  case Stage::PrimaryFactor:case Stage::AlternateFactor:return 6;
  case Stage::PrimaryProduct:case Stage::AlternateProduct:return 23;
  case Stage::SelectPort:return 9;
  case Stage::AwaitWindow:case Stage::AwaitSelect:case Stage::AwaitAddress:case Stage::EmitWrite:case Stage::Complete:return 0;
  default:return 3;
  }
 }
 template<class Reader,class Writer>Advance advanceBefore(unsigned available,Reader&& read,Writer&& write){
  Advance p;while(const unsigned cost=nextClocks()){
   if(cost>available)break;
   switch(stage_){
   case Stage::ReadPrimary:offset_=uint16_t(read(Read::PrimaryOffset));stage_=Stage::TestSign;break;
   case Stage::TestSign:stage_=Stage::AdmitSign;break;
   case Stage::AdmitSign:stage_=negativeAddress()?Stage::ReadAlternate:Stage::ResolveAddress;break;
   case Stage::ReadAlternate:offset_=uint16_t(read(Read::AlternateOffset));stage_=Stage::ResolveAddress;break;
   case Stage::ResolveAddress:address_=uint16_t(relative_+offset_);stage_=Stage::DispatchWindow;break;
   case Stage::DispatchWindow:stage_=Stage::AwaitWindow;break;
   case Stage::ReadVoice:voice_=uint8_t(read(kind_==Kind::VoiceB?Read::VoiceB:Read::VoiceA));stage_=kind_==Kind::VoiceB?Stage::NormalizeVoice:Stage::ReadBase;break;
   case Stage::ReadBase:base_=uint8_t(read(Read::VoiceBase));stage_=Stage::Difference;break;
   case Stage::Difference:difference_=uint8_t(voice_-base_);stage_=Stage::AdmitDifference;break;
   case Stage::AdmitDifference:stage_=negativeDifference()?Stage::ClampDifference:Stage::NormalizeVoice;break;
   case Stage::ClampDifference:difference_=0;stage_=Stage::NormalizeVoice;break;
   case Stage::NormalizeVoice:index_=kind_==Kind::VoiceB?voice_:difference_;stage_=Stage::PrimaryFactor;break;
   case Stage::PrimaryFactor:stage_=Stage::PrimaryProduct;break;
   case Stage::PrimaryProduct:product_=primaryFactor()*index_;stage_=Stage::PublishPrimary;break;
   case Stage::PublishPrimary:write(Write::PrimaryOffset,uint16_t(product_));stage_=Stage::AlternateFactor;break;
   case Stage::AlternateFactor:stage_=Stage::AlternateProduct;break;
   case Stage::AlternateProduct:product_=alternateFactor()*index_;stage_=Stage::PublishAlternate;break;
   case Stage::PublishAlternate:write(Write::AlternateOffset,uint16_t(product_));stage_=Stage::DispatchSelect;break;
   case Stage::DispatchSelect:stage_=Stage::AwaitSelect;break;
   case Stage::PublishAddress:write(Write::CachedAddress,address_);stage_=Stage::DispatchAddress;break;
   case Stage::DispatchAddress:stage_=Stage::AwaitAddress;break;
   case Stage::SelectPort:stage_=kind_==Kind::WriteB?Stage::SelectDelay:Stage::ReadCached;break;
   case Stage::SelectDelay:stage_=Stage::ReadCached;break;
   case Stage::ReadCached:cached_=uint16_t(read(Read::CachedAddress));stage_=Stage::CompareCached;break;
   case Stage::CompareCached:stage_=Stage::DispatchWrite;break;
   case Stage::DispatchWrite:stage_=Stage::EmitWrite;break;default:break;
   }
   available-=cost;p.clocks+=cost;++p.boundaries;
  }return p;
 }
private:
 static Stage initial(Kind kind){switch(kind){case Kind::PrimeRelative:case Kind::WriteRelative:return Stage::ReadPrimary;
  case Kind::ReadAlternate:return Stage::ReadAlternate;case Kind::VoiceB:case Kind::VoiceA:return Stage::ReadVoice;
  case Kind::PrimeAbsolute:return Stage::PublishAddress;case Kind::WriteAbsolute:return Stage::ReadCached;default:return Stage::SelectPort;}}
 Kind kind_;uint16_t relative_=0,address_=0,offset_=0,cached_=0;
 uint8_t voice_=0,base_=0,difference_=0;unsigned index_=0;uint32_t product_=0;Stage stage_;
};
}
