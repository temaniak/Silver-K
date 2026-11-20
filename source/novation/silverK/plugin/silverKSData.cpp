#include "silverKSData.h"
#include "baseLib/sha256.h"

namespace silverK {
namespace {
constexpr const char* bundledInitHash="9a960a260afdef411d421c703cbd1950ce9082b675e7ee9a3a92be8c087a0e07";
std::vector<uint8_t> readBytes(const juce::File& file) {
    juce::MemoryBlock data;
    if (file.getSize() != 0x80000 || !file.loadFileAsData(data))
        return {};
    const auto* first = static_cast<const uint8_t*>(data.getData());
    return {first, first + data.getSize()};
}
}
bool readKSDataFiles(const std::vector<juce::File>& files,std::vector<uint8_t>& storage,juce::String& error){
 error.clear();storage.clear();
 if(files.empty()){error="Select KS All Data or its MIDI files in Setup > KS factory data. KS also requires firmware and Wave ROM in ROMs.";return false;}
 if(files.size()==1 && files.front().hasFileExtension("bin")){
  const auto& file=files.front();storage=readBytes(file);
  if(storage.size()!=0x80000){error="KS user Flash must contain exactly 524288 bytes: "+file.getFileName();return false;}
  if(baseLib::SHA256(storage).toString()==bundledInitHash){error="Silver Init is test data. Select your KS All Data or a compatible full user Flash in Setup > KS factory data.";return false;}
 }else{
  if(files.size()>128){error="Select at most 128 KS Data MIDI files";return false;}
  storage.assign(0x80000,0xff);std::array<bool,600> programs{};std::array<bool,100> performances{};unsigned programCount=0;
  for(const auto& file:files){
   if(!file.hasFileExtension("mid;midi") || !file.existsAsFile() || file.getSize()>8*1024*1024){error="Select one full KS Flash BIN, or KS Data MIDI files: "+file.getFileName();return false;}
   juce::MidiFile midi;auto input=file.createInputStream();if(!input || !midi.readFrom(*input)){error="Cannot read KS Data MIDI: "+file.getFileName();return false;}
   bool found=false;
   for(int t=0;t<midi.getNumTracks();++t)for(int n=0;n<midi.getTrack(t)->getNumEvents();++n){
    const auto& message=midi.getTrack(t)->getEventPointer(n)->message;const auto* bytes=message.getRawData();const auto size=message.getRawDataSize();
    constexpr uint8_t prefix[]{0xf0,0,0x20,0x29,1,0x42};
    if(size<14 || !std::equal(std::begin(prefix),std::end(prefix),bytes) || bytes[size-1]!=0xf7)continue;
    if(!std::all_of(bytes+1,bytes+size-1,[](uint8_t value){return value<128;})){error="Invalid KS Data MIDI: "+file.getFileName();return false;}
    size_t offset=0,length=0;bool* present=nullptr;
    if(bytes[7]==1 && size==270){
     const unsigned bank=bytes[11],program=bytes[12];if(bank<1 || bank>8 || program>=(bank<=4?100u:50u)){error="Invalid KS program address in "+file.getFileName();return false;}
     const unsigned index=bank<=4?(bank-1)*100+program:400+(bank-5)*50+program;
     offset=0x20000+index*256;length=256;present=&programs[index];
     if(!*present)++programCount;
    }else if(bytes[7]==3 && size==1038 && bytes[12]<100){offset=0x45800+bytes[12]*1024;length=1024;present=&performances[bytes[12]];}
    if(!present)continue;found=true;
    if(*present && !std::equal(bytes+13,bytes+13+length,storage.begin()+offset)){error="Conflicting KS Data at the same program/performance address: "+file.getFileName();return false;}
    std::copy_n(bytes+13,length,storage.begin()+offset);*present=true;
   }
   if(!found){error="No KS Program/Performance data in "+file.getFileName();return false;}
  }
  if(!programCount){error="KS Data must include programs, not only performances";return false;}
  constexpr uint8_t globals[]{1,0,3,0,0,1,15,127,0,0,64,0,0,64,64,0,3,0,0,0,0};
  std::fill(storage.begin()+0x1ff00,storage.begin()+0x20000,0);std::copy(std::begin(globals),std::end(globals),storage.begin()+0x1ff00);
  if(!programs[200])for(unsigned n=0;n<600;++n)if(programs[n]){
   storage[0x1ff02]=uint8_t(n<400?n/100+1:(n-400)/50+5);storage[0x1ff03]=uint8_t(n<400?n%100:(n-400)%50);break;
  }
 }
 if(storage[0x1ff02]<1 || storage[0x1ff02]>8 || storage[0x1ff03]>=(storage[0x1ff02]<=4?100:50) || storage[0x1ff04]>1){error="Invalid startup globals in KS Data";return false;}
 return true;
}
}
