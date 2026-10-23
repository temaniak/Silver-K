#include "ksSystemServicesCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
SystemServicesPartialTest partialTest=nullptr;using T=SystemServicesTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,111> points{{{0x1306,4},{0x130a,2},{0x130c,4},{0x1310,4},{0x1314,4},{0x1318,4},{0x131c,4},{0x1320,4},{0x1324,4},{0x1328,4},{0x132c,4},{0x1330,4},{0x1334,4},{0x1338,4},{0x133c,4},{0x1340,4},{0x1344,4},{0x1348,4},{0x134c,4},{0x1350,4},{0x1354,6},{0x135a,2},{0x135c,6},{0x1362,2},{0x1364,2},{0x1366,2},{0x1368,2},{0x136a,2},{0x136c,2},{0x1b74,2},{0x1b76,2},{0x1b78,4},{0x1b7c,2},{0x2228,2},{0x222a,4},{0x222e,4},{0x2232,4},{0x2236,2},{0x2238,2},{0x223a,4},{0x223e,2},{0x2240,2},{0x2242,2},{0x2244,4},{0x2248,2},{0x224a,4},{0x224e,2},{0x2250,2},{0x2252,2},{0x2254,2},{0x2256,2},{0x2258,4},{0x225c,2},{0x225e,2},{0x2260,2},{0x2262,2},{0x2264,2},{0x2266,2},{0x2268,2},{0x226a,2},{0x226c,4},{0x2270,2},{0x2272,2},{0x2274,2},{0x2276,2},{0x2278,2},{0x227a,4},{0x227e,2},{0x2280,2},{0x2282,2},{0x2284,2},{0x2286,2},{0x2288,4},{0x228c,2},{0x228e,2},{0x2290,2},{0x2292,2},{0x2294,4},{0x2298,2},{0x229a,2},{0x229c,2},{0x229e,4},{0x22a2,6},{0x22a8,6},{0x22ae,4},{0x22b2,2},{0x22b4,2},{0x22b6,2},{0x22b8,2},{0x22ba,2},{0x22bc,2},{0x22be,2},{0x22c0,2},{0x22c2,2},{0x22c4,6},{0x22ca,8},{0x22d2,2},{0x22d4,6},{0x22da,6},{0x22e0,4},{0x22e4,2},{0x22e6,2},{0x22e8,2},{0x22ea,2},{0x22ec,2},{0x22ee,2},{0x22f0,2},{0x22f2,6},{0x22f8,6},{0x22fe,4},{0x2302,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){if(at<0x400000)return !(previous&1)&&at==(unsigned(previous)|1u)?0:3;if(at>=0xfffd10&&at<0xffffa0)return !(previous&1)&&at==(unsigned(previous)|1u)?0:1;if(at>=0xffffa0)return 1;return 3;}
}
void setSystemServicesPartialForTests(SystemServicesPartialTest value){partialTest=value;}
bool SystemServicesCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x1306:kind=T::Kind::Initialize;break;case 0x1354:kind=T::Kind::KeyMap;break;case 0x1b74:kind=T::Kind::Keyboard;break;case 0x2228:kind=T::Kind::Glyphs;break;case 0x2262:kind=T::Kind::Bar;break;case 0x22a2:kind=T::Kind::Signature;break;case 0x22d4:kind=T::Kind::Save;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native SystemServices capacity exceeded");slot->emplace(*kind,h.getSP(),uint8_t(h.regs[6].rl));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild();const auto stage=task.stage();
 const auto store=byteClocks(stage==S::WriteKeyOne?0xb93b:stage==S::WriteKeyTwo?0xb93a:stage==S::WriteKeyFour?0xb938:0xffff6d,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field,unsigned offset)->uint16_t{if(field==T::Read::Version)return uint16_t(h.read16(0x81fc28));return uint8_t(h.read8(field==T::Read::Hardware?0xffec14:field==T::Read::Expected?0x190 + offset:0x81fc00 + offset));};auto write=[&](T::Write field,uint16_t v){if(field!=T::Write::KeyboardPort)--r[0].er;f.store8(field==T::Write::KeyOne?0xb93b:field==T::Write::KeyTwo?0xb93a:field==T::Write::KeyFour?0xb938:0xffff6d,int8_t(v));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native SystemServices admission differs");
 switch(stage){case S::ResetHardware:f.call(0x490);break;
case S::PatchKeyMap:f.call(0x1354);break;
case S::InitializeDisplay:f.call(0x926);break;
case S::InitializeButtons:f.call(0x13f8);break;
case S::InitializeUart:f.call(0xfbc);break;
case S::InitializeMidi:f.call(0xfd6);break;
case S::InitializeKeyboard:f.call(0x1b74);break;
case S::InitializeCalibration:f.call(0x1f84);break;
case S::InitializeDspLink:f.call(0x4f0);break;
case S::InitializeDspWait:f.call(0x506);break;
case S::InitializePanelLevel:f.call(0x1510);break;
case S::InitializeControlMaps:f.call(0x13b4);break;
case S::EnableInterrupts:h.exr=uint8_t(0x0);break;
case S::InitializeAdc:f.call(0x152e);break;
case S::InitializeMidiJobs:f.call(0x230c);break;
case S::InitializePlayback:f.call(0x7666);break;
case S::InitializeClock:f.call(0x2604);break;
case S::DisplayStartup:f.call(0xb6c);break;
case S::InitializeGlyphs:f.call(0x2228);break;
case S::EnterPlayback:f.jump(0x41e2,true);break;
case S::ReadHardware:f.move(r[0].rl,task.hardware());break;
case S::BindKeyMap:f.move(r[0].er,0xb93c);break;
case S::BindKeyMask:f.move(r[1].rl,0xffffffff);break;
case S::WriteKeyOne:h.clockI(2);break;
case S::WriteKeyTwo:h.clockI(2);break;
case S::SkipKeyThree:r[0].er=int32_t(uint32_t(r[0].er)-0x1);break;
case S::WriteKeyFour:h.clockI(2);break;
case S::ReturnKeyMap:f.ret();break;
case S::BindKeyboardPort:f.move(r[6].rl,0x1);break;
case S::ResetKeyQueue:f.call(0x1efe);break;
case S::ReturnKeyboard:f.ret();break;
case S::BindGlyphCommand:f.move(r[6].rl,0x0);break;
case S::SetGlyphCommand:f.call(0x960);break;
case S::BindGlyphMask:f.move(r[5].r,0x3e0);break;
case S::BindGlyphCount:f.move(r[5].e,0x6);break;
case S::BindGlyphRow:f.move(r[6].rl,r[5].rl);break;
case S::BindRowCount:f.move(r[6].rh,0x6);break;
case S::WriteGlyphRow:f.call(0x968);break;
case S::DecrementRowCount:f.inc(r[6].rh,-1);break;
case S::BindEmptyRow:f.move(r[6].rl,0x0);break;
case S::WriteEmptyRow:f.call(0x968);break;
case S::BindFullRow:f.move(r[6].rl,0x1f);break;
case S::WriteFullRow:f.call(0x968);break;
case S::ShiftGlyphMask:f.shift(r[5].r,1,false);break;
case S::DecrementGlyphCount:f.inc(r[5].e,-1);break;
case S::BindLastRow:f.move(r[6].rl,0x10);break;
case S::BindLastRowCount:f.move(r[6].rh,0x8);break;
case S::WriteLastRow:f.call(0x968);break;
case S::DecrementLastRowCount:f.inc(r[6].rh,-1);break;
case S::ReturnGlyphs:f.ret();break;
case S::RetainBarValue:f.move(r[5].rl,r[6].rl);break;
case S::CompareBarLimit:h.sub8(r[5].rl,0x19);break;
case S::ClampBarValue:f.move(r[5].rl,0x19);break;
case S::BindBarPosition:f.move(r[6].rl,0xffffff80);break;
case S::SetBarPosition:f.call(0x976);break;
case S::BindFullBar:f.move(r[6].rl,0x5);break;
case S::BindBarCount:f.move(r[6].rh,0x5);break;
case S::BindBarUnit:f.move(r[5].rh,0x5);break;
case S::SubtractBarUnit:r[5].rl=h.sub8(r[5].rl,r[5].rh);break;
case S::WriteSolidBar:f.call(0x968);break;
case S::DecrementSolidCount:f.inc(r[6].rh,-1);break;
case S::RestorePartialBar:r[5].rl=h.add8(r[5].rl,r[5].rh);break;
case S::BindPartialBar:f.move(r[6].rl,r[5].rl);break;
case S::WritePartialBar:f.call(0x968);break;
case S::DecrementPartialCount:f.inc(r[6].rh,-1);break;
case S::BindBlankBar:f.move(r[6].rl,0x0);break;
case S::WriteBlankBar:f.call(0x968);break;
case S::DecrementBlankCount:f.inc(r[6].rh,-1);break;
case S::BindBarEnd:f.move(r[6].rl,0x6);break;
case S::CloseBar:h.clockI(2);f.next=0x976;break;
case S::BindSignatureSource:f.move(r[0].er,0x190);break;
case S::BindSignatureTarget:f.move(r[1].er,0x81fc00);break;
case S::BindSignatureCount:f.move(r[2].e,0x28);break;
case S::ReadSignatureExpected:f.move(r[2].rl,task.expected());r[0].er+=1;h.clockI(2);break;
case S::ReadSignatureActual:f.move(r[2].rh,task.actual());r[1].er+=1;h.clockI(2);break;
case S::CompareSignature:h.sub8(r[2].rl,r[2].rh);break;
case S::DecrementSignatureCount:f.inc(r[2].e,-1);break;
case S::ClearSignatureResult:r[6].r=h.sub16(r[6].r,r[6].r);break;
case S::BindSignatureVersion:f.move(r[6].er,0x81fc00);break;
case S::ReadSignatureVersion:f.move(r[6].r,task.version());break;
case S::ReturnSignature:f.ret();break;
case S::BindSaveSource:f.move(r[0].er,0x190);break;
case S::BindSaveTarget:f.move(r[1].er,0x81fc00);break;
case S::BindSaveCount:f.move(r[2].e,0x30);break;
case S::ReadSaveExpected:f.move(r[2].rl,task.expected());r[0].er+=1;h.clockI(2);break;
case S::ReadSaveActual:f.move(r[2].rh,task.actual());r[1].er+=1;h.clockI(2);break;
case S::CompareSave:h.sub8(r[2].rl,r[2].rh);break;
case S::DecrementSaveCount:f.inc(r[2].e,-1);break;
case S::BindWriteSource:f.move(r[5].er,0x190);break;
case S::BindWriteTarget:f.move(r[6].er,0x81fc00);break;
case S::WriteSystemPage:f.call(0x11ae);break;
case S::ReturnSave:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native SystemServices clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
