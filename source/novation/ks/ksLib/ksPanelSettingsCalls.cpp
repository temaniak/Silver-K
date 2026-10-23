#include "ksPanelSettingsCalls.h"
#include "ksRuntimeBoundary.h"
#include <stdexcept>
namespace ks { namespace {
PanelSettingsPartialTest partialTest=nullptr;using T=PanelSettingsTask;using S=T::Stage;
struct Point{unsigned pc,length;};constexpr std::array<Point,122> points{{{0x1f84,4},{0x1f88,2},{0x1f8a,6},{0x1f90,2},{0x1f92,6},{0x1f98,2},{0x1f9a,6},{0x1fa0,2},{0x1fa2,2},{0x1fa4,6},{0x1faa,4},{0x1fae,2},{0x1fb0,2},{0x1fb2,6},{0x1fb8,6},{0x1fbe,2},{0x1fc0,2},{0x1fc2,2},{0x1fc4,2},{0x1fc6,2},{0x1fc8,2},{0x1fca,2},{0x1fcc,4},{0x1fd0,4},{0x1fd4,2},{0x1fd6,2},{0x1fd8,4},{0x1fdc,2},{0x1fde,2},{0x1fe0,2},{0x1fe2,2},{0x1fe4,2},{0x1fe6,4},{0x1fea,2},{0x1fec,2},{0x1fee,4},{0x1ff2,2},{0x1ff4,4},{0x1ff8,2},{0x1ffa,4},{0x1ffe,2},{0x2000,2},{0x2002,2},{0x2004,4},{0x2008,2},{0x200a,2},{0x200c,4},{0x2010,2},{0x2012,4},{0x2016,2},{0x2018,4},{0x201c,2},{0x201e,2},{0x2020,2},{0x2022,2},{0x2024,2},{0x2026,4},{0x202a,2},{0x202c,4},{0x2030,2},{0x2032,4},{0x2036,4},{0x203a,2},{0x203c,2},{0x203e,6},{0x2044,2},{0x2046,2},{0x2048,2},{0x204a,2},{0x204c,2},{0x204e,2},{0x2050,2},{0x2052,4},{0x2056,2},{0x2058,2},{0x205a,6},{0x2060,4},{0x2064,2},{0x2066,4},{0x206a,4},{0x206e,2},{0x2070,6},{0x2076,6},{0x207c,4},{0x2080,2},{0x2082,6},{0x2088,6},{0x208e,4},{0x2092,2},{0x2094,4},{0x2098,6},{0x209e,2},{0x20a0,6},{0x20a6,4},{0x20aa,2},{0x20ac,2},{0x20ae,6},{0x20b4,2},{0x20b6,2},{0x20b8,2},{0x20ba,6},{0x20c0,4},{0x20c4,2},{0x20c6,2},{0x219a,6},{0x21a0,6},{0x21a6,4},{0x21aa,2},{0x21ac,2},{0x21ae,2},{0x21b0,2},{0x21b2,2},{0x21b4,4},{0x21b8,6},{0x21be,6},{0x21c4,6},{0x21ca,4},{0x21ce,2},{0x21d0,6},{0x21d6,6},{0x21dc,4},{0x21e0,2}}};
Point point(const T& task){return points[unsigned(task.awaitingChild()?task.resumeStage():task.stage())];}
unsigned byteClocks(unsigned at,int previous){return at<0x400000&&!(previous&1)&&at==(unsigned(previous)|1u)?0:3;}
}
void setPanelSettingsPartialForTests(PanelSettingsPartialTest value){partialTest=value;}
bool PanelSettingsCalls::step(ControllerCpu& h){
 using namespace controllerDetail;const auto pc=unsigned(h.getPC());std::optional<Call>* slot=nullptr;for(auto& c:calls_)if(c&&c->token==h.getSP()){slot=&c;break;}
 std::optional<T::Kind> kind;switch(pc){case 0x1f84:kind=T::Kind::Reset;break;case 0x1f92:kind=T::Kind::Select;break;case 0x1f9a:kind=T::Kind::Show;break;case 0x1fd8:kind=T::Kind::Poll;break;case 0x2070:kind=T::Kind::Coefficients;break;case 0x2098:kind=T::Kind::First;break;case 0x20ae:kind=T::Kind::Second;break;case 0x219a:kind=T::Kind::Load;break;case 0x21d0:kind=T::Kind::Save;break;default:break;}
 if(kind&&(!slot||point((*slot)->task).pc!=pc)){if(!slot)for(auto& c:calls_)if(!c){slot=&c;break;}if(!slot)throw std::runtime_error("Native PanelSettings capacity exceeded");slot->emplace(*kind,h.getSP(),uint8_t(h.regs[6].rl));++started;}
 if(!slot||point((*slot)->task).pc!=pc){if(slot)slot->reset();if(partialTest&&partialTest(h)){++partialEntries;return true;}return false;}
 auto& call=**slot;auto& task=call.task;auto& r=h.regs;if(task.awaitingChild())task.resumeChild({uint8_t(r[6].rl),uint8_t(r[6].rh),r[6].e!=0});const auto stage=task.stage();
 const auto store=byteClocks(stage==S::WriteCopyByte?0x158ec + task.position():stage==S::WriteSelectedMode?0x158f2:stage==S::WriteDuty?0x158f0:0x159f0,h.lastwrite);const auto cost=task.nextClocks(store);const auto before=h.cycles;RuntimeBoundary f(h,pc,point(task).length);
 auto read=[&](T::Read field,unsigned offset)->uint16_t{return uint8_t(h.read8(field==T::Read::Notice?0x159f0:field==T::Read::Mode?0x158f2:field==T::Read::Duty?0x158f0:0x81fd00 + offset));};
 auto write=[&](T::Write field,uint16_t v){if(field==T::Write::FirstHistory||field==T::Write::SecondHistory)f.store16(field==T::Write::FirstHistory?0x159ec:0x159ee,int16_t(v));else f.store8(field==T::Write::Notice?0x159f0:field==T::Write::Mode?0x158f2:field==T::Write::Duty?0x158f0:0x158ec + task.position(),int8_t(v));};
 if(task.advanceBefore(cost,read,write,store)!=cost)throw std::runtime_error("Native PanelSettings admission differs");
 switch(stage){case S::LoadCalibration:f.call(0x219a);break;
case S::ClearNotice:f.move(r[0].rl,0x0);break;
case S::ReturnReset:f.ret();break;
case S::ShowSelectedMode:f.call(0x1f9a);break;
case S::ReadNotice:f.move(r[0].rl,task.notice());break;
case S::SaveNoticeInput:f.push(r[6].r);break;
case S::BindNoticeText:f.move(r[6].er,0xbab2);break;
case S::PrintNoticeText:f.call(0x9b8);break;
case S::RestoreNoticeInput:f.pop(r[6].r);break;
case S::SetNotice:f.move(r[0].rl,0x1);break;
case S::ReadSelectedMode:f.move(r[0].rl,task.mode());break;
case S::CompareModeFour:h.sub8(r[0].rl,0x4);break;
case S::CompareModeFive:h.sub8(r[0].rl,0x5);break;
case S::ReturnInactive:f.ret();break;
case S::SaveModeInput:f.push(r[6].r);break;
case S::BindModePosition:f.move(r[6].rl,0xffffffcd);break;
case S::SetModePosition:f.call(0x976);break;
case S::PrintModeValue:f.call(0xb00);break;
case S::RestoreModeInput:f.pop(r[6].r);break;
case S::ReturnMode:f.ret();break;
case S::PollInput:f.call(0x21e2);break;
case S::TestPending:f.move(r[6].e,r[6].e);break;
case S::ShowInput:f.call(0x1f9a);break;
case S::CompareFirstSelector:h.sub8(r[6].rl,0x22);break;
case S::UpdateFirstRange:f.call(0x2098);break;
case S::SaveFirstInput:f.push(r[6].r);break;
case S::BindFirstPosition:f.move(r[6].rl,0xffffffc0);break;
case S::SetFirstPosition:f.call(0x976);break;
case S::RestoreFirstInput:f.pop(r[6].r);break;
case S::ScaleFirst:f.call(0x20fc);break;
case S::BindFirstValue:f.move(r[0].rl,r[6].rh);break;
case S::DisplayFirstValue:f.call(0xa28);break;
case S::CompareSecondSelector:h.sub8(r[6].rl,0x23);break;
case S::UpdateSecondRange:f.call(0x20ae);break;
case S::SaveSecondInput:f.push(r[6].r);break;
case S::BindSecondPosition:f.move(r[6].rl,0xffffffc4);break;
case S::SetSecondPosition:f.call(0x976);break;
case S::RestoreSecondInput:f.pop(r[6].r);break;
case S::ScaleSecond:f.call(0x20fc);break;
case S::BindSecondValue:f.move(r[0].rl,r[6].rh);break;
case S::DisplaySecondValue:f.call(0xa28);break;
case S::CompareThirdSelector:h.sub8(r[6].rl,0x20);break;
case S::SaveThirdInput:f.push(r[6].r);break;
case S::BindThirdPosition:f.move(r[6].rl,0xffffffc8);break;
case S::SetThirdPosition:f.call(0x976);break;
case S::RestoreThirdInput:f.pop(r[6].r);break;
case S::ScaleThird:f.call(0x20fc);break;
case S::BindThirdValue:f.move(r[0].rl,r[6].rh);break;
case S::DisplayThirdValue:f.call(0xa28);break;
case S::ReadEncoder:f.call(0x1886);break;
case S::TestEncoder:f.move(r[6].rl,r[6].rl);break;
case S::ReadDuty:f.move(r[0].rl,task.duty());break;
case S::NormalizeDuty:r[0].r&=0xff;value(h,r[0].r);break;
case S::RetainDelta:f.move(r[1].rl,r[6].rl);break;
case S::ExtendDelta:r[1].r=int8_t(r[1].r);value(h,r[1].r);break;
case S::AddDuty:r[0].r=h.add16(r[0].r,r[1].r);break;
case S::ClearDuty:r[0].r=h.sub16(r[0].r,r[0].r);break;
case S::CompareDutyLimit:h.sub16(r[0].r,0xff);break;
case S::ClampDuty:f.move(r[0].rl,0xffffffff);break;
case S::ApplyDuty:f.call(0x151e);break;
case S::BindDutyPosition:f.move(r[6].rl,0xffffffd1);break;
case S::SetDutyPosition:f.call(0x976);break;
case S::DisplayDuty:f.call(0xa28);break;
case S::ReturnPoll:f.ret();break;
case S::BindFirstHistory:f.move(r[1].er,0x159ec);break;
case S::BindFirstCoefficients:f.move(r[2].er,0x158ec);break;
case S::BindFirstBias:f.move(r[3].r,0x9000);break;
case S::CalculateFirstCoefficients:f.call(0x20de);break;
case S::BindSecondHistory:f.move(r[1].er,0x159ee);break;
case S::BindSecondCoefficients:f.move(r[2].er,0x158ee);break;
case S::BindSecondBias:f.move(r[3].r,0x8800);break;
case S::CalculateSecondCoefficients:f.call(0x20de);break;
case S::SaveCoefficients:h.clockI(2);break;
case S::BindFirstRange:f.move(r[1].er,0x159ec);break;
case S::UpdateFirstHistory:f.call(0x20c8);break;
case S::BindFirstOutput:f.move(r[2].er,0x158ec);break;
case S::BindFirstOutputBias:f.move(r[3].r,0x9000);break;
case S::CalculateFirstOutput:f.call(0x20de);break;
case S::ReturnFirst:f.ret();break;
case S::BindSecondRange:f.move(r[1].er,0x159ee);break;
case S::InvertSecondInput:r[6].rh=~r[6].rh;value(h,r[6].rh);break;
case S::UpdateSecondHistory:f.call(0x20c8);break;
case S::RestoreInvertedInput:r[6].rh=~r[6].rh;value(h,r[6].rh);break;
case S::BindSecondOutput:f.move(r[2].er,0x158ee);break;
case S::BindSecondOutputBias:f.move(r[3].r,0x8800);break;
case S::CalculateSecondOutput:f.call(0x20de);break;
case S::ReturnSecond:f.ret();break;
case S::BindLoadSource:f.move(r[0].er,0x81fd00);break;
case S::BindLoadTarget:f.move(r[1].er,0x158ec);break;
case S::BindCopyCount:f.move(r[2].e,0x100);break;
case S::ReadCopyByte:f.move(r[2].rl,task.byte());r[0].er+=1;h.clockI(2);break;
case S::NextCopyTarget:f.inc(r[1].er,1);break;
case S::DecrementCopyCount:f.inc(r[2].e,-1);break;
case S::BindHistoryInitial:f.move(r[0].r,0x60a0);break;
case S::ReadLoadedDuty:f.move(r[0].rl,task.duty());break;
case S::ApplyLoadedDuty:f.call(0x151e);break;
case S::ReturnLoad:f.ret();break;
case S::BindSaveTarget:f.move(r[6].er,0x81fd00);break;
case S::BindSaveSource:f.move(r[5].er,0x158ec);break;
case S::SaveCalibration:f.call(0x11ae);break;
case S::ReturnSave:f.ret();break; default:break;}
 if(h.cycles-before!=cost)throw std::runtime_error("Native PanelSettings clocks differ");
 if(task.complete()){h.pc=h.makepc(f.next);slot->reset();++completed;return true;}
 if(!task.awaitingChild())call.token=h.getSP();h.pc=h.makepc(task.awaitingChild()?f.next:point(task).pc);return true;
}
}
