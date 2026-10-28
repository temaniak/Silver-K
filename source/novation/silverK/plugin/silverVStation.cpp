#include "silverVStation.h"
#include "silverPatch.h"
#include <cstring>
namespace silverK {
namespace {
constexpr unsigned Programs=400,SoundSize=128,NameSize=21;
constexpr uint8_t Record[]{'S','K','V','S'};
uint32_t big32(const uint8_t* p){return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3];}
}
bool parseVStationBank(pluginLib::patchDB::DataList& output,const pluginLib::patchDB::Data& input){
 using namespace pluginLib::patchDB;
 if(input.size()<156 || std::memcmp(input.data(),"CcnK",4) || std::memcmp(input.data()+16,"NvS0",4) || big32(input.data()+12)!=1)return false;
 const auto declared=big32(input.data()+4),count=big32(input.data()+24);
 if(count==0 || count>Programs)return false;
 DataList parsed;
 if(std::memcmp(input.data()+8,"FxBk",4)==0){
  // Standard VST2 program-list bank, each entry is a complete FPCh preset.
  if(declared && size_t(declared)+8!=input.size())return false;
  size_t offset=156;
  for(unsigned n=0;n<count;++n){
   if(offset+8>input.size())return false;
   const size_t size=size_t(big32(input.data()+offset+4))+8;
   if(size<60 || size>input.size()-offset)return false;
   DataList program;if(!parseVStationProgram(program,Data(input.begin()+offset,input.begin()+offset+size)))return false;
   auto& record=program.front();record[4+7]=1;record[4+11]=uint8_t(1+n/100);record[4+12]=uint8_t(n%100);
   parsed.push_back(std::move(record));offset+=size;
  }
  if(offset!=input.size())return false;
 }else{
  if(input.size()<161 || std::memcmp(input.data()+8,"FBCh",4))return false;
  const auto* chunk=input.data()+160;
  // Both chunk versions store sounds first, followed by a separate name table.
  const size_t prefix=chunk[0]==1?1:chunk[0]==2?3:0,tail=chunk[0]==1?7:6;
  const size_t chunkSize=prefix+size_t(count)*(SoundSize+NameSize)+tail;
  if(!prefix || big32(input.data()+156)!=chunkSize || chunkSize>input.size()-160)return false;
  // Some hosts leave byteSize stale or append a footer outside the chunk.
  // Only the completely validated inner chunk defines the sound/name bounds.
  const auto* sounds=chunk+prefix;
  if(!std::all_of(sounds,sounds+count*SoundSize,[](uint8_t b){return b<128;}))return false;
  const auto* names=sounds+count*SoundSize;
  for(unsigned n=0;n<count;++n){
   Data record(std::begin(Record),std::end(Record));
   const uint8_t header[]{0xf0,0,0x20,0x29,1,0x41,0x7f,1,0,0x11,1,uint8_t(1+n/100),uint8_t(n%100)};
   record.insert(record.end(),std::begin(header),std::end(header));
   record.insert(record.end(),sounds+n*SoundSize,sounds+(n+1)*SoundSize);record.push_back(0xf7);
   const auto* name=names+n*NameSize;const auto* end=std::find(name,name+NameSize,uint8_t(0));
   if(!std::all_of(name,end,[](uint8_t b){return b>=32 && b!=127;}))return false;
   record.insert(record.end(),name,name+NameSize);parsed.push_back(std::move(record));
  }
 }
 output.insert(output.end(),std::make_move_iterator(parsed.begin()),std::make_move_iterator(parsed.end()));return true;
}
bool parseVStationProgram(pluginLib::patchDB::DataList& output,const pluginLib::patchDB::Data& input){
 // NvS0/FPCh stores one 128-byte sound followed by its 21-byte name.
 // Bytes after a name's NUL terminator are padding, not characters.
 constexpr size_t size=60+SoundSize+NameSize;
 if(input.size()!=size || std::memcmp(input.data(),"CcnK",4) || std::memcmp(input.data()+8,"FPCh",4)
    || std::memcmp(input.data()+16,"NvS0",4) || big32(input.data()+12)!=1 || big32(input.data()+24)!=1
    || big32(input.data()+56)!=SoundSize+NameSize)return false;
 const auto declared=big32(input.data()+4);if(declared && size_t(declared)+8!=input.size())return false;
 const auto* sound=input.data()+60;const auto* name=sound+SoundSize;
 const auto* end=std::find(name,name+NameSize,uint8_t(0));
 if(!std::all_of(sound,name,[](uint8_t b){return b<128;}) || !std::all_of(name,end,[](uint8_t b){return b>=32 && b!=127;}))return false;
 pluginLib::patchDB::Data record(std::begin(Record),std::end(Record));
 const uint8_t header[]{0xf0,0,0x20,0x29,1,0x41,0x7f,0,0,0x11,1,0,0};
 record.insert(record.end(),std::begin(header),std::end(header));record.insert(record.end(),sound,name);record.push_back(0xf7);
 record.insert(record.end(),name,name+NameSize);output.push_back(std::move(record));return true;
}
bool parseVStationSysex(pluginLib::patchDB::DataList& output,const pluginLib::patchDB::Data& input){
 // V-Station MIDI exports extend the K program dump with a 21-byte name.
 if(input.size()!=13+SoundSize+NameSize+1 || modelOf(input)!=Model::KStation || input.back()!=0xf7
    || (input[7]!=0 && input[7]!=1) || !std::all_of(input.begin()+1,input.end()-1,[](uint8_t b){return b<128;}))return false;
 const auto* sound=input.data()+13;const auto* name=sound+SoundSize;
 const auto* end=std::find(name,name+NameSize,uint8_t(0));
 if(!std::all_of(name,end,[](uint8_t b){return b>=32 && b!=127;}))return false;
 // Use the same individual-program record as FPCh, including source identity.
 pluginLib::patchDB::Data record(std::begin(Record),std::end(Record));
 const uint8_t header[]{0xf0,0,0x20,0x29,1,0x41,0x7f,0,0,0x11,1,0,0};
 record.insert(record.end(),std::begin(header),std::end(header));record.insert(record.end(),sound,name);record.push_back(0xf7);
 record.insert(record.end(),name,name+NameSize);output.push_back(std::move(record));return true;
}
bool unpackVStationPatch(pluginLib::patchDB::Data& record,std::string& name){
 if(record.size()!=sizeof(Record)+142+NameSize || !std::equal(std::begin(Record),std::end(Record),record.begin()) || !validPatch(record.data()+sizeof(Record),142))return false;
 const auto* text=record.data()+sizeof(Record)+142;const auto end=std::find(text,text+NameSize,uint8_t(0));
 const auto* chars=reinterpret_cast<const char*>(text);juce::String decoded;
 if(juce::CharPointer_UTF8::isValidString(chars,int(end-text)))decoded=juce::String::fromUTF8(chars,int(end-text));
 else for(auto* c=text;c!=end;++c)decoded+=juce::String::charToString(*c);
 name=decoded.trim().toStdString();
 pluginLib::patchDB::Data sysex(record.begin()+sizeof(Record),record.begin()+sizeof(Record)+142);record=std::move(sysex);return true;
}
}
