#pragma once
#include <cstdint>
namespace ks {
// Startup banner selection and original nested debounce/startup waits with deferred key observation and mux publications.
class StartupBannerTask final {
public:
 enum class Kind { Banner };enum class Stage { BindDefaultBanner, ReadBannerHardware, SelectBannerHardware, BindAlternateBanner, ShowBanner, BindVersionBanner, ShowVersion, ClearBannerPorts, WriteBannerDirection, BindBannerMux, WriteBannerMux, SaveDebounceWord, BindDebounceCount, SaveDebounceCount, BindDebounceInner, DecrementDebounceInner, RepeatDebounceInner, RestoreDebounceCount, DecrementDebounceCount, RepeatDebounceCount, RestoreDebounceWord, ReadBannerKeys, BindIdleMux, WriteIdleMux, BindIdleDirection, WriteIdleDirection, TestBannerKey, SelectBannerKey, InitializeBannerNotice, SaveStartupWord, BindStartupCount, SaveStartupCount, BindStartupInner, DecrementStartupInner, RepeatStartupInner, RestoreStartupCount, DecrementStartupCount, RepeatStartupCount, RestoreStartupWord, ReturnBanner, AwaitChild, Complete };
 enum class Read { Hardware, Keys };enum class Write { Direction, Mux };uint8_t hardware()const{return hardware_;}uint8_t keys()const{return keys_;}
 explicit StartupBannerTask(Kind kind){switch(kind){case Kind::Banner:stage_=Stage::BindDefaultBanner;break;}}
 Stage stage()const{return stage_;}Stage resumeStage()const{return resume_;}bool awaitingChild()const{return stage_==Stage::AwaitChild;}bool complete()const{return stage_==Stage::Complete;}
 bool resumeChild(){if(!awaitingChild())return false;stage_=resume_;return true;}
 unsigned nextClocks(unsigned store=3)const{switch(stage_){case Stage::BindDefaultBanner:case Stage::BindAlternateBanner:case Stage::BindVersionBanner:return 9; case Stage::ReadBannerHardware:return 12; case Stage::ShowBanner:return 20; case Stage::ShowVersion:case Stage::InitializeBannerNotice:return 15; case Stage::WriteBannerDirection:case Stage::WriteIdleDirection:return 6+store; case Stage::WriteBannerMux:case Stage::WriteIdleMux:return 3+store; case Stage::SaveDebounceWord:case Stage::SaveDebounceCount:case Stage::RestoreDebounceCount:case Stage::RestoreDebounceWord:case Stage::SaveStartupWord:case Stage::SaveStartupCount:case Stage::RestoreStartupCount:case Stage::RestoreStartupWord:return 11; case Stage::BindDebounceCount:case Stage::BindDebounceInner:case Stage::BindStartupCount:case Stage::BindStartupInner:return 6; case Stage::ReadBannerKeys:return 4; case Stage::ReturnBanner:return 17; case Stage::AwaitChild:case Stage::Complete:return 0;default:return 3;}}
 template<class Reader,class Writer>unsigned advanceBefore(unsigned available,Reader&& read,Writer&& write,unsigned store=3){const auto cost=nextClocks(store);if(!cost||cost>available)return 0;switch(stage_){case Stage::ReadBannerHardware:hardware_=uint8_t(read(Read::Hardware));next();break;
case Stage::SelectBannerHardware:stage_=hardware_?Stage::BindAlternateBanner:Stage::ShowBanner;break;
case Stage::WriteBannerDirection:write(Write::Direction,0);next();break;
case Stage::WriteBannerMux:write(Write::Mux,0x25);next();break;
case Stage::BindDebounceCount:outer_=1;next();break;
case Stage::BindDebounceInner:inner_=0xa67;next();break;
case Stage::DecrementDebounceInner:--inner_;next();break;
case Stage::RepeatDebounceInner:stage_=inner_?Stage::DecrementDebounceInner:Stage::RestoreDebounceCount;break;
case Stage::DecrementDebounceCount:--outer_;next();break;
case Stage::RepeatDebounceCount:stage_=outer_?Stage::SaveDebounceCount:Stage::RestoreDebounceWord;break;
case Stage::ReadBannerKeys:keys_=uint8_t(read(Read::Keys));next();break;
case Stage::WriteIdleMux:write(Write::Mux,0x30);next();break;
case Stage::WriteIdleDirection:write(Write::Direction,255);next();break;
case Stage::SelectBannerKey:stage_=keys_&64?Stage::SaveStartupWord:Stage::InitializeBannerNotice;break;
case Stage::BindStartupCount:outer_=500;next();break;
case Stage::BindStartupInner:inner_=0xa67;next();break;
case Stage::DecrementStartupInner:--inner_;next();break;
case Stage::RepeatStartupInner:stage_=inner_?Stage::DecrementStartupInner:Stage::RestoreStartupCount;break;
case Stage::DecrementStartupCount:--outer_;next();break;
case Stage::RepeatStartupCount:stage_=outer_?Stage::SaveStartupCount:Stage::RestoreStartupWord;break;
case Stage::ShowBanner:await(Stage::BindVersionBanner);break;
case Stage::ShowVersion:await(Stage::ClearBannerPorts);break;
case Stage::InitializeBannerNotice:await(Stage::SaveStartupWord);break;
case Stage::ReturnBanner:stage_=Stage::Complete;break; default:next();break;}return cost;}
private:
 void next(){stage_=Stage(unsigned(stage_)+1);}void await(Stage resume){resume_=resume;stage_=Stage::AwaitChild;}
 Stage stage_=Stage::Complete,resume_=Stage::Complete;uint16_t outer_=0,inner_=0;uint8_t hardware_=0,keys_=0;
};
}
