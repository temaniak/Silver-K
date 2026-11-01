#pragma once
#include "kstationNativeStartupControl.h"
#include "kstationNativeStartupServices.h"
#include "kstationNativeStartupHardware.h"
#include "kstationNativeStartupState.h"
#include "kstationNativeStartupAdc.h"
#include "kstationNativeStartupTables.h"
#include "kstationNativeStartupButtons.h"
#include "kstationNativeStartupBanner.h"
#include "kstationNativeStartupKeyboard.h"
#include "kstationNativeDspGlobals.h"
#include "kstationNativeProgramState.h"
#include "kstationNativeProgramVoices.h"
#include "kstationNativePanelRefresh.h"
#include "kstationNativePanelHardware.h"
#include "kstationNativeLcdText.h"
#include "kstationNativeNumericDisplay.h"
#include "kstationNativeMetadata.h"
#include "kstationNativeStartupDisplay.h"
#include "kstationNativeParameterInput.h"
#include "kstationNativeParameterUpdate.h"
#include "kstationNativeParameterEdit.h"
#include "kstationNativeUiServices.h"
#include "kstationNativeForegroundClock.h"
#include "kstationNativeVoiceEffects.h"
#include "kstationNativeVoiceSync.h"
#include "kstationNativeVoiceGlobalOwner.h"
#include "kstationNativeArp.h"
#include "kstationNativeMidiSysExOwner.h"
#include "kstationNativeMidiInitOwner.h"
#include "kstationNativePatchPackOwner.h"
#include "kstationNativeFlashCommitOwner.h"
#include "kstationNativeTransposeApplyOwner.h"
#include "kstationNativeDisplayNoticeOwner.h"
#include "kstationNativeMidiSendControl.h"
#include "kstationNativeDspTrigger.h"
#include "kstationNativeUiEncoder.h"
#include "kstationNativeKeyOwner.h"
#include "kstationNativeVoiceFeedback.h"
#include "kstationNativeMainUi.h"
#include "kstationNativeMidiWrappersOwner.h"
#include "kstationNativeDisplayFormatOwner.h"
#include "kstationNativePeriodicDisplay.h"
#include "kstationNativeUiPanel.h"
#include "kstationNativeMidiEvents.h"
#include <optional>
namespace kstation {
// Development-only caller projection composition. Explicit child owners may
// yield to the reference oracle; this is not the CPU-independent controller.
class NativeDevelopmentServices final {
public:
 template<class F> static void ranges(F&&f){
  for(auto r:NativeUiPanel::CodeRanges)f(r.begin,r.end);
  for(auto r:NativePeriodicDisplay::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeDisplayFormat::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeMidiWrappers::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeMainUi::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeVoiceFeedback::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeKeyOwner::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeUiEncoder::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeDspTrigger::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeStartupControl::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeStartupServices::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeStartupHardware::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeStartupState::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeStartupAdc::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeStartupTables::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeStartupButtons::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeStartupBanner::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeStartupKeyboard::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeDspGlobals::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeProgramState::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeProgramVoices::CodeRanges)f(r.begin,r.end);
  for(auto r:NativePanelRefresh::CodeRanges)f(r.begin,r.end);
  for(auto r:NativePanelHardware::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeLcdText::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeNumericDisplay::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeMetadata::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeStartupDisplay::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeParameterInput::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeParameterUpdate::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeParameterEdit::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeUiServices::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeForegroundClock::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeVoiceEffects::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeVoiceSync::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeVoiceGlobal::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeArp::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeMidiSysEx::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeMidiInitOwner::CodeRanges)f(r.begin,r.end);
  for(auto r:NativePatchPack::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeFlashCommitOwner::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeTransposeApplyOwner::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeDisplayNoticeOwner::CodeRanges)f(r.begin,r.end);
  for(auto r:NativeMidiSendControl::CodeRanges)f(r.begin,r.end);
 }
 void reset(){
  floatConvert_.reset();
  uiPanel_.reset();
  periodicDisplay_.reset();
  displayFormat_.reset();
  midiWrappers_.reset();
  mainUi_.reset();
  voiceFeedback_.reset();
  keyEvents_.reset();
  uiEncoder_.reset();
  dspTrigger_.reset();
  startupControl_.reset();
  startupServices_.reset();
  startupHardware_.reset();
  startupState_.reset();
  startupAdc_.reset();
  startupTables_.reset();
  startupButtons_.reset();
  startupBanner_.reset();
  startupKeyboard_.reset();
  dspGlobals_.reset();
  programState_.reset();
  programVoices_.reset();
  panelRefresh_.reset();
  panelHardware_.reset();
  lcdText_.reset();
  numericDisplay_.reset();
  metadata_.reset();
  startupDisplay_.reset();
  parameterInput_.reset();
  parameterUpdate_.reset();
  parameterEdit_.reset();
  uiServices_.reset();
  foregroundClock_.reset();
  voiceEffects_.reset();
  voiceSync_.reset();
  voiceGlobal_.reset();
  arp_.reset();
  sysex_.reset();
  midiInit_.reset();
  patchPack_.reset();
  flashCommit_.reset();
  transposeApply_.reset();
  displayNotice_.reset();
  midiSendControl_.reset();
 }
 bool step(Stage1Cpu&c){
  const auto pc=uint32_t(c.getPC());
  // 610E can request conversion while the MIDI ancestor is suspended.
  // Its independent semantic leaf must not borrow the ancestor's job stack.
  if(pc>=0x5c04&&pc<0x5c3a){
   if(!floatConvert_&&pc==0x5c04)floatConvert_.emplace(uint16_t(c.reg16(0)));
   if(floatConvert_&&floatConvert_->step(c)){if(floatConvert_->task().complete())floatConvert_.reset();return true;}
  }
  // 4444 is both the periodic task suffix and the cold clock initializer.
  // An admitted parent keeps its continuation before a new leaf admission.
  if(periodicDisplay_.active()&&NativePeriodicDisplay::candidate(pc)&&periodicDisplay_.step(c))return true;
  if(uiPanel_.active()&&NativeUiPanel::candidate(pc)&&uiPanel_.step(c))return true;
  // Retire a completed tail child before its caller consumes the return
  // boundary. These owners perform no IO when only retiring a continuation.
  if(voiceGlobal_.active()&&voiceGlobal_.step(c))return true;
  if(flashCommit_.active()&&flashCommit_.step(c))return true;
  if(transposeApply_.active()&&transposeApply_.step(c))return true;
  if(displayFormat_.active()&&displayFormat_.step(c))return true;
  if(sysex_.active()&&sysex_.step(c))return true;
  if(arp_.active()&&arp_.step(c))return true;
  if(keyEvents_.active()&&keyEvents_.step(c))return true;
  if(midiWrappers_.active()&&midiWrappers_.step(c))return true;
  if(parameterEdit_.ownsPackChild()&&NativePatchPack::candidate(pc))return parameterEdit_.step(c);
  if(NativeStartupControl::candidate(pc)&&startupControl_.step(c))return true;
  if(NativeStartupServices::candidate(pc)&&startupServices_.step(c))return true;
  if(NativeStartupHardware::candidate(pc)&&startupHardware_.step(c))return true;
  if(NativeStartupState::candidate(pc)&&startupState_.step(c))return true;
  if(NativeStartupAdc::candidate(pc)&&startupAdc_.step(c))return true;
  if(NativeStartupTables::candidate(pc)&&startupTables_.step(c))return true;
  if(NativeStartupButtons::candidate(pc)&&startupButtons_.step(c))return true;
  if(NativeStartupBanner::candidate(pc)&&startupBanner_.step(c))return true;
  if(NativeStartupKeyboard::candidate(pc)&&startupKeyboard_.step(c))return true;
  if(NativeDspGlobals::candidate(pc)&&dspGlobals_.step(c))return true;
  if(NativeProgramState::candidate(pc)&&programState_.step(c))return true;
  if(NativeProgramVoices::candidate(pc)&&programVoices_.step(c))return true;
  if(NativePanelRefresh::candidate(pc)&&panelRefresh_.step(c))return true;
  if(NativePanelHardware::candidate(pc)&&panelHardware_.step(c))return true;
  if(NativeLcdText::candidate(pc)&&lcdText_.step(c))return true;
  if(NativeNumericDisplay::candidate(pc)&&numericDisplay_.step(c))return true;
  if(NativeMetadata::candidate(pc)&&metadata_.step(c))return true;
  if(NativeStartupDisplay::candidate(pc)&&startupDisplay_.step(c))return true;
  if(NativeParameterInput::candidate(pc)&&parameterInput_.step(c))return true;
  if(NativeParameterUpdate::candidate(pc)&&parameterUpdate_.step(c))return true;
  if(NativeParameterEdit::candidate(pc)&&parameterEdit_.step(c))return true;
  if(NativeUiServices::candidate(pc)&&uiServices_.step(c))return true;
  if(NativeForegroundClock::candidate(pc)&&foregroundClock_.step(c))return true;
  if(NativeVoiceEffects::candidate(pc)&&voiceEffects_.step(c))return true;
  if(NativeVoiceSync::candidate(pc)&&voiceSync_.step(c))return true;
  if(NativeVoiceGlobal::candidate(pc)&&voiceGlobal_.step(c))return true;
  if((arp_.active()||NativeArp::candidate(pc))&&arp_.step(c))return true;
  if((sysex_.active()||NativeMidiSysEx::candidate(pc))&&sysex_.step(c))return true;
  if(NativeMidiInitOwner::candidate(pc)&&midiInit_.step(c))return true;
  if(NativePatchPack::candidate(pc)&&patchPack_.step(c))return true;
  if((flashCommit_.active()||NativeFlashCommitOwner::candidate(pc))&&flashCommit_.step(c))return true;
  if((transposeApply_.active()||NativeTransposeApplyOwner::candidate(pc))&&transposeApply_.step(c))return true;
  if(NativeDisplayNoticeOwner::candidate(pc)&&displayNotice_.step(c))return true;
  if(NativeMidiSendControl::candidate(pc)&&midiSendControl_.step(c))return true;
  if(NativeDspTrigger::candidate(pc)&&dspTrigger_.step(c))return true;
  if(NativeUiEncoder::candidate(pc)&&uiEncoder_.step(c))return true;
  if((keyEvents_.active()||NativeKeyOwner::candidate(pc))&&keyEvents_.step(c))return true;
  if(NativeVoiceFeedback::candidate(pc)&&voiceFeedback_.step(c))return true;
  if(NativeMainUi::candidate(pc)&&mainUi_.step(c))return true;
  if((midiWrappers_.active()||NativeMidiWrappers::candidate(pc))&&midiWrappers_.step(c))return true;
  if(NativeDisplayFormat::candidate(pc)&&displayFormat_.step(c))return true;
  if(NativePeriodicDisplay::candidate(pc)&&periodicDisplay_.step(c))return true;
  if(NativeUiPanel::candidate(pc)&&uiPanel_.step(c))return true;
  return false;
 }
 // Admit direct children from the call site just executed. Return addresses
 // come from verified source graph edges, never from reading a guest frame.
 void afterPhase(Stage1Cpu&c,uint32_t from,NativeMidiEvents&events){
  const auto to=uint32_t(c.getPC());
  if(to==0x581e&&from==0x3b16&&!keyEvents_.active())
   if(!keyEvents_.admitPlayback(c,0x3b1a))throw std::logic_error("K playback admission");
  if(!displayFormat_.active()){
   struct Edge{uint32_t from,to,returned;};
   constexpr Edge edges[]={{0x3f24,0x132c,0x3f28},{0x414a,0x4618,0x414e},{0x45a4,0x4618,0x45a8},
    {0x4410,0x23f8,0x4414},{0x447c,0x2438,0x4480},{0x448e,0x2438,0x4492},{0x4634,0x4644,0x4636},
    {0x463a,0x4454,0x463e},{0x463e,0x2492,0x4642},{0x4646,0x248a,0x464a},{0x46ac,0x248a,0x46b0}};
   for(auto e:edges)if(from==e.from&&to==e.to){
    if(!displayFormat_.admit(c,e.returned))throw std::logic_error("K display format admission");break;}
  }

  if(to==0x79a4&&!voiceGlobal_.active()){
   const auto returned=from==0x79a0?0x79a2u:from==0x8360?0x8364u:0u;
   if(returned&&!voiceGlobal_.admitPrefix(c,returned))throw std::logic_error("K global parameter admission");
  }
  if(to==0x33f4&&from==0x3804&&!events.active())
   if(!events.admitTrigger(c,0x3808))throw std::logic_error("K arpeggiator trigger admission");
  if(to==0x2e34&&!events.active()&&(from==0x2900||from==0x29b4||from==0x2a2c))
   if(!events.admitControl(c,from+2))throw std::logic_error("K MIDI controller admission");
  if(!flashCommit_.active()){
   uint32_t returned=0;
   if((to==0x4d3a&&(from==0x3194||from==0x4d20||from==0x4e6a))
      ||(to==0x4e10&&from==0x31ca)||(to==0x4e3c&&from==0x315c)
      ||(to==0x4dc0&&(from==0x4ed8||from==0x4f3e))||(to==0x4e06&&from==0x4e00))returned=from+4;
   if(returned&&!flashCommit_.admit(c,returned))throw std::logic_error("K Flash commit admission");
  }
  if(!transposeApply_.active()&&to==0x3f54&&from==0x3148)
   if(!transposeApply_.admit(c,0x314c))throw std::logic_error("K transpose apply admission");
 }
private:
 std::optional<NativeMidiFloatConvert> floatConvert_;
 NativeUiPanel uiPanel_;
 NativePeriodicDisplay periodicDisplay_;
 NativeDisplayFormat displayFormat_;
 NativeMidiWrappers midiWrappers_;
 NativeMainUi mainUi_;
 NativeVoiceFeedback voiceFeedback_;
 NativeKeyOwner keyEvents_;
 NativeUiEncoder uiEncoder_;
 NativeDspTrigger dspTrigger_;
 NativeStartupControl startupControl_;
 NativeStartupServices startupServices_;
 NativeStartupHardware startupHardware_;
 NativeStartupState startupState_;
 NativeStartupAdc startupAdc_;
 NativeStartupTables startupTables_;
 NativeStartupButtons startupButtons_;
 NativeStartupBanner startupBanner_;
 NativeStartupKeyboard startupKeyboard_;
 NativeDspGlobals dspGlobals_;
 NativeProgramState programState_;
 NativeProgramVoices programVoices_;
 NativePanelRefresh panelRefresh_;
 NativePanelHardware panelHardware_;
 NativeLcdText lcdText_;
 NativeNumericDisplay numericDisplay_;
 NativeMetadata metadata_;
 NativeStartupDisplay startupDisplay_;
 NativeParameterInput parameterInput_;
 NativeParameterUpdate parameterUpdate_;
 NativeParameterEdit parameterEdit_;
 NativeUiServices uiServices_;
 NativeForegroundClock foregroundClock_;
 NativeVoiceEffects voiceEffects_;
 NativeVoiceSync voiceSync_;
 NativeVoiceGlobal voiceGlobal_;
 NativeArp arp_;
 NativeMidiSysEx sysex_;
 NativeMidiInitOwner midiInit_;
 NativePatchPack patchPack_;
 NativeFlashCommitOwner flashCommit_;
 NativeTransposeApplyOwner transposeApply_;
 NativeDisplayNoticeOwner displayNotice_;
 NativeMidiSendControl midiSendControl_;
};
}
