#include "ksMidiPacketForeground.h"
#include "ksMidiPacketAbi.h"
#include "ksControllerState.h"
namespace ks {namespace {
int history(uint32_t a){return a<0x400000||(a>=0xfffd10&&a<0xffffa0)?int(a):-1;}
bool endpoint(uint32_t a){return(a>=0x15000&&a<0x30000)||(a>=0xffec00&&a<0xffefff)||a==0xffff8a;}
std::optional<MidiPacketService::Kind>root(uint32_t p){using K=MidiPacketService::Kind;switch(p){case 0x2a58:return K::NoteOff;case 0x2a60:return K::NoteOn;case 0x2a76:return K::Pedal;case 0x2a8e:return K::Expression;case 0x2aa2:return K::PitchBend;case 0x2ac6:return K::Modulation;case 0x2ada:return K::Pressure;case 0x2aea:return K::Byte;case 0x2b60:return K::QueueOff;case 0x2b6a:return K::QueueOn;case 0x2b82:return K::TransmitOff;case 0x2b8c:return K::TransmitOn;case 0x2bc8:return K::Panic;case 0x2be8:return K::Control;case 0x2c06:return K::RawControl;case 0x2c1e:return K::Program;default:return{};}}
}
void MidiPacketForeground::project(CallerAbi&a,const MidiPacketService&s){projectMidiPacket(a,s);}
bool MidiPacketForeground::step(ControllerCpu&h){
 if(!job_){const auto kind=root(h.getPC());if(!kind||!h.nativeFetchClocksOnly||!h.nativeLowBase||h.nativeLowSize<0x30000||!h.interruptMode2||h.indent<=0)return false;const auto sp=h.getSP();if((sp&1)||sp<0xfff400||sp>0xfffbfc)return false;
  const auto word=[&](uint32_t a){return(unsigned(h.nativeLowBase[a])<<8)|h.nativeLowBase[a+1];};if(word(0x15b04)>255||word(0x15b06)>255||h.memory[0xffed6c]||h.memory[0xffed6e])return false;
  job_.emplace(*kind,MidiPacketService::Input{uint16_t(h.regs[6].r),uint16_t(h.regs[5].r),uint8_t(h.regs[2].rl)},sp,h.indent,h.exr);++started;++kinds[unsigned(*kind)];
 }
 auto&j=*job_;if(j.depth!=h.indent)return false;auto&s=j.service;const auto start=h.cycles;const auto previous=h.lastwrite;const auto cost=s.nextClocks(previous);
 const auto used=s.advanceBefore(cost,[&](uint32_t raw,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported MIDI packet read");h.cycles=start+dt;h.lastread=history(a);return h.readResource(a);},[&](uint32_t raw,uint8_t v,unsigned dt){const auto a=raw&0xffffff;if(!endpoint(a))throw std::logic_error("Unsupported MIDI packet write");h.cycles=start+dt;h.lastwrite=history(a);h.writeResource(a,v);},previous);
 if(!cost||used!=cost)throw std::logic_error("MIDI packet admission differs");if(s.effect().read)h.lastread=-1;if(s.effect().write)h.lastwrite=-1;h.exr=s.critical()?7:j.priority;++phases;components|=s.components();
 if(s.complete()){CallerAbi a;for(unsigned n=0;n<7;++n)a.values[n]=uint32_t(h.regs[n].er);a.flags=h.ccr;project(a,s);for(unsigned n=0;n<7;++n)h.regs[n].er=int32_t(a.values[n]);h.ccr=a.flags;h.exr=j.priority;uint32_t target=0;for(unsigned n=0;n<4;++n){h.cycles=start+8+3*n;h.lastread=-1;target=(target<<8)|h.readResource(j.caller+n);}h.setSP(j.caller+4);h.pc=target&0xffffff;--h.indent;job_.reset();++completed;}
 h.cycles=start+cost;return true;
}
}
