#include "ksBoard.h"
#include <cstring>
namespace ks {
uint64_t Board::controllerCheckpoint()const{
 if(comparison)return comparison->compatibilityCheckpoint();
 uint64_t hash=14695981039346656037ull;
 const auto byte=[&](uint8_t v){hash^=v;hash*=1099511628211ull;};
 const auto word=[&](uint64_t v){for(unsigned n=0;n<8;++n)byte(uint8_t(v>>(8*n)));};
 const auto section=[&](const char*tag,const void*data,size_t size){word(std::strlen(tag));for(const char*p=tag;*p;++p)byte(uint8_t(*p));word(size);const auto*p=static_cast<const uint8_t*>(data);for(size_t n=0;n<size;++n)byte(p[n]);};
 // Private raw schema2: every backing byte is retained. No CPU scratch mask,
 // compatibility location or fabricated CPU state participates in this hash.
 const char version[]="KS-NATIVE-RESOURCE-V2-RAW-QUEUE-OBJECTS";section("schema",version,sizeof(version)-1);
 section("rom",rom.data(),rom.size());section("flash",flash.data(),flash.size());section("waves",waves.data(),waves.size());
 section("physical-memory-all16MiB",device->memory,sizeof(device->memory));section("mmio",io.data(),io.size());
 word(device->cycles);word(device->pending_irqs);word(device->pending_irqs_high);word(device->interruptMode2);section("priorities",device->interruptPriority,sizeof(device->interruptPriority));
 word(uint32_t(device->lastread));word(uint32_t(device->lastwrite));word(device->nativeLowSize);word(device->nativeLowWritable);word(overlay);word(mainEntered);word(ready);
 word(flashCommand);word(pageRemaining);word(flashRevision);word(adcDue);word(rxDue);word(txDue);word(nativeQueuedMidi);word(serialNext);word(serialSlot);word(serialSlaveStarted);
 // Raw object representations include unused slots, indices and padding.
 // This is a retained diagnostic hash, NOT a semantic cross-Board equality
 // assertion; deterministic live-payload checkpoint admission is still pending.
 section("uart-input-raw-object",&midiIn,sizeof(midiIn));section("uart-output-raw-object",&midiOut,sizeof(midiOut));
 section("audio-raw-object",&audio,sizeof(audio));section("serial-a-to-b-raw-object",&aToB,sizeof(aToB));section("serial-b-to-a-raw-object",&bToA,sizeof(bToA));
 for(unsigned i=0;i<2;++i){word(dsps[i]->cycles());word(dsps[i]->instructions());word(uint64_t(epochs[i]));word(started[i]);for(const auto&slot:dsps[i]->serialTx)for(auto v:slot)word(v);word(serialUnderruns[i]);word(serialOverflows[i]);}
 return hash;
}
}
