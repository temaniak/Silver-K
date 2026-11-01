#pragma once
#include "ksStartupLifecycleService.h"
#include "ksControllerDeviceState.h"
#include "ksControllerImageValidation.h"
#include "ksServiceCycleResolver.h"
#include <cstddef>
namespace ks {
// Physical-resource edge for the semantic startup graph. IRQ ownership stays
// with NativeControllerExecutor and observes priority() between admitted phases.
// This edge has no CPU architecture fields or external ABI projection image.
class NativeStartupController final {
public:
 using Entry=StartupLifecycleService::Entry;using Outcome=StartupLifecycleService::Outcome;
 struct Metrics{uint64_t phases=0,resourceReads=0,resourceWrites=0;};
 explicit NativeStartupController(Entry entry=Entry::Cold,unsigned initialMask=0):service_(entry,initialMask){}
 void bindVerifiedRom(const uint8_t*rom,std::size_t bytes){if(bound_||metrics_.phases)throw std::logic_error("Startup ROM binding ownership");controllerImage::verifyRom(rom,bytes);metadata_.assign(rom);rom_=rom;romBytes_=bytes;bound_=true;}
 bool bound()const{return bound_;}bool mainEntered()const{return service_.mainEntered();}bool ready()const{return service_.ready();}bool firstMainAvailable()const{return bound_&&service_.firstMainAvailable();}
 bool complete()const{return service_.complete();}Outcome outcome()const{return service_.outcome();}unsigned priority()const{return service_.priority();}
 const StartupPlaybackContext&result()const{return service_.result();}const Metrics&metrics()const{return metrics_;}const StartupLifecycleService&service()const{return service_;}bool overlayVerified()const{return overlayVerified_;}
 unsigned nextClocks(const ControllerDeviceState&d)const{return bound_?service_.nextClocks(d.lastwrite):0;}
 unsigned advanceBefore(ControllerDeviceState&d,unsigned budget){const auto cost=nextClocks(d);if(!cost||budget<cost)return 0;
  // The actual Board switches mapping on its MMIO publication. Verify the
  // admitted executable SRAM before any subsequent startup phase can advance.
  if(!overlayVerified_&&d.nativeLowWritable){controllerImage::verifyOverlay(rom_,romBytes_,d.nativeLowBase,d.nativeLowSize);overlayVerified_=true;}
  const auto start=d.cycles;const auto previous=d.lastwrite;ServiceCycleResolver resolver(metadata_);
  const auto used=service_.advanceBefore(cost,[&](uint32_t raw,unsigned offset){const auto at=raw&0xffffff;d.cycles=start+offset;d.lastread=StartupHardwareService::history(at);++metrics_.resourceReads;return d.readResource(at);},[&](uint32_t raw,uint8_t value,unsigned offset){const auto at=raw&0xffffff;d.cycles=start+offset;d.lastwrite=StartupHardwareService::history(at);++metrics_.resourceWrites;d.writeResource(at,value);},resolver,previous);
  if(used!=cost)throw std::logic_error("Native startup phase admission mismatch");if(service_.effect().read)d.lastread=-1;if(service_.effect().write)d.lastwrite=-1;d.cycles=start+cost;++metrics_.phases;return cost;
 }
private:
 StartupLifecycleService service_;ServiceCycleResolver::Metadata metadata_;const uint8_t*rom_=nullptr;std::size_t romBytes_=0;bool bound_=false,overlayVerified_=false;Metrics metrics_{};
};
}
