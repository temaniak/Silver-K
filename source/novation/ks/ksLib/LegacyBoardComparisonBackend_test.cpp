// Explicit comparison backend. Never part of ksLib or a product target.
#include "ksBoardComparison_test.h"
#include "ksRecordedController_test.h"
#include "ksDspTransport.h"
#include "ksControllerJobs.h"
#include <cstdlib>
#include <cstring>
#include <stdexcept>
namespace ks {
#include "LegacyBoardComparisonAccess.inc"
class LegacyBoardComparisonBackend final:public BoardComparisonBackend {
 struct Proxy final:ControllerBus {
  explicit Proxy(Board&board):board_(board){}
  uint8_t read(uint32_t a)override{return board_.read(a);}
  void write(uint32_t a,uint8_t v)override{board_.write(a,v);}
  Board&board_;
 };
public:
 LegacyBoardComparisonBackend(Board&board,const std::vector<uint8_t>&rom):board_(board),proxy_(board),cpu_(std::make_unique<ControllerCpu>()),rom_(rom){
  const auto*transport=std::getenv("SILVER_KS_DSP_TRANSPORT");eventTransport_=transport&&std::strcmp(transport,"events")==0;
  const auto*jobs=std::getenv("SILVER_KS_CONTROL_JOBS");eventControl_=jobs&&std::strcmp(jobs,"1")==0;
 }
 ControllerDeviceState&resources()noexcept override{return *cpu_;}
 void initialize()override{
  if(initialized_||&board_.controllerResources()!=static_cast<ControllerDeviceState*>(cpu_.get()))throw std::logic_error("Legacy comparison resource attachment differs");
  // Board has finished loading/attaching its real physical resource state.
  // Install both legacy protected CPU pointer and base resource bus now.
  cpu_->memmap(&proxy_,0,1<<24);cpu_->boot();initialized_=true;
 }
 void beginSlice(uint64_t begin,unsigned budget)override{if(!initialized_)throw std::logic_error("Legacy comparison not initialized");sliceLimit_=begin+budget;sliceBudget_=budget;BoardComparisonAccess::snapshotUART(board_);}
 uint64_t cycles()const override{return recorded_?recorded_->cycles():cpu_->cycles;}
 uint64_t compatibilityCheckpoint()const override{return BoardComparisonAccess::checkpoint(board_);}
 void advanceNextPhase()override{
  checkMain_=startupPhase_=finishTail_=false;nativeTick_=false;
  if(recorded_){recorded_->advance(sliceBudget_,[&](unsigned i,unsigned off){return BoardComparisonAccess::hostRead(board_,i,off);},[&](unsigned i,unsigned off,uint8_t v){return BoardComparisonAccess::hostWrite(board_,i,off,v);});return;}
  auto&h=*cpu_;
  if(runtime_&&referenceTail_){
   if(runtime_->activeNativeWork()){runtime_->step(h);nativeTick_=true;finishTail_=!runtime_->activeNativeWork();}
   else if(referenceTailHybrid_){const auto pc=unsigned(h.getPC());if(!LiveMidiController::candidate(pc)||!live_->step(h)){live_->delegated(pc);h.step();}}
   else h.step();
  }else if(runtime_){
   nativeTick_=true;
   if(!board_.ready){
    if(!nativeCodeSealed_&&board_.overlay){RuntimeController::verifyImage(h);nativeCodeSealed_=true;}
    runtime_->stepStartup(h);startupPhase_=checkMain_=true;
   }else if(!eventTransport_&&!eventControl_)runtime_->step(h);
   else{
    unsigned batched=0;const unsigned pc=h.getPC();
    if(eventTransport_&&pc>=0x2416&&pc<=0x25e4&&DspTransport::candidate(pc))batched=DspTransport::service(h,BoardComparisonAccess::nextEvent(board_,sliceLimit_));
    if(batched){++transportJobs_;transportBoundaries_+=batched;runtime_->steps+=batched;live_->voiceActions.steps+=batched;live_->voiceActions.transportSteps+=batched;}
    else if(eventControl_&&ControllerJobs::candidate(pc)){
     const auto job=ControllerJobs::service(h,BoardComparisonAccess::nextEvent(board_,sliceLimit_));
     if(job.steps){++controlJobs_;controlBoundaries_+=job.steps;runtime_->steps+=job.steps;live_->noteActions.steps+=job.noteSteps;}
     else runtime_->step(h);
    }else runtime_->step(h);
   }
  }else if(live_){const unsigned pc=h.getPC();if(!LiveMidiController::candidate(pc)||!live_->step(h)){live_->delegated(pc);h.step();}checkMain_=true;}
  else{h.step();checkMain_=true;}
 }
 void tickAfterPhase(uint64_t previous)override{
  if(recorded_)return;
  if(nativeTick_)BoardComparisonAccess::nativeTick(board_,previous);else BoardComparisonAccess::referenceTick(board_,previous);
  if(finishTail_){cpu_->interpreterAllowed=true;cpu_->nativeFetchClocksOnly=false;cpu_->nativeLowBase=nullptr;}
  if(checkMain_&&(!startupPhase_||!runtime_->activeNativeWork())){if(cpu_->getPC()==0x41e2)board_.mainEntered=true;if(board_.mainEntered&&cpu_->getPC()==0x4334)board_.ready=true;}
 }
 bool mainEntered()const override{return board_.mainEntered;}
 bool ready()const override{return board_.ready;}
 bool nativeMapping()const override{return runtime_&&!referenceTail_;}
 bool protectedCode()const override{return nativeMapping()&&nativeCodeSealed_;}
 bool allowsLiveInput()const override{return !recorded_;}
 uint32_t diagnosticLocation()const override{return uint32_t(cpu_->getPC());}
 const char*name()const override{
  if(recorded_)return "recorded-cpp-prototype";
  if(nativeMapping())return eventControl_?(eventTransport_?"legacy-native-events-control-comparison":"legacy-native-control-jobs-comparison"):(eventTransport_?"legacy-native-events-transport-comparison":"legacy-native-runtime-comparison");
  return midiForTests()?"live-cpp-midi-hybrid":"original-h8";
 }
 void select(BoardComparisonMode mode)override{if(mode==BoardComparisonMode::LegacyCompatibility)selectLegacy();else selectReference(mode==BoardComparisonMode::Hybrid);}
 void resetMetrics()override{if(live_)live_->resetMetrics();if(runtime_)runtime_->resetMetrics();transportJobs_=transportBoundaries_=controlJobs_=controlBoundaries_=0;}
 void enableDelegationProfile()override{if(live_)live_->enableDelegationProfile();}
 uint64_t interpretedSteps()const override{return cpu_->interpretedSteps;}
 uint64_t stepsSinceHandoff()const override{return nativeMapping()?cpu_->interpretedSteps-nativeH8Steps_:0;}
 uint64_t checkedPorts()const override{return recorded_?recorded_->checkedPorts():0;}
 void startRecorded(std::shared_ptr<const ControllerTape>tape,uint64_t checkpoint)override{
  if(BoardComparisonAccess::recording(board_)||recorded_||midiForTests()||(runtime_&&runtime_->activeNativeWork())||!board_.ready||!BoardComparisonAccess::started(board_))throw std::logic_error("KS controller replay requires completed original boot");
  recorded_=std::make_unique<RecordedController>(std::move(tape),checkpoint,cpu_->cycles);
 }
 void startLegacyStartupForTests(){
  if(!initialized_||cpu_->cycles||board_.ready||board_.mainEntered||live_||runtime_||recorded_)throw std::logic_error("Legacy cold comparison requires a fresh unadvanced Board");
  auto state=std::make_unique<LiveMidiController>(*cpu_);
  auto runtime=std::make_unique<RuntimeController>(*cpu_,*state,RuntimeController::Entry::Reset);
  // Exact former Board nativeStartup setup. CPU architecture is test-only.
  const uint32_t reset=(uint32_t(rom_[0])<<24)|(uint32_t(rom_[1])<<16)|(uint32_t(rom_[2])<<8)|rom_[3];
  cpu_->pc=cpu_->makepc(reset&0xffffff);cpu_->cycles=0;cpu_->lastread=3;
  cpu_->pending_irqs=cpu_->pending_irqs_high=0;cpu_->ccr=128;cpu_->exr=0;
  live_=std::move(state);runtime_=std::move(runtime);nativeCodeSealed_=false;
  cpu_->interpreterAllowed=false;cpu_->nativeFetchClocksOnly=true;
  cpu_->nativeLowBase=rom_.data();cpu_->nativeLowSize=unsigned(rom_.size());
 }
 ControllerCpu&cpuForTests(){return *cpu_;}
 const RuntimeController*runtimeForTests()const{return referenceTail_?nullptr:runtime_.get();}
 const LiveMidiController*midiForTests()const{return referenceTail_&&!referenceTailHybrid_?nullptr:live_.get();}
 std::array<uint64_t,4>batchesForTests()const{return{transportJobs_,transportBoundaries_,controlJobs_,controlBoundaries_};}
private:
 void selectReference(bool hybrid){
  if(!controllerReferenceAvailable())throw std::logic_error("KS hybrid controller is available only in comparison tests");
  if(recorded_||!board_.ready)throw std::logic_error("Live KS MIDI requires completed original boot, without replay");
  if(runtime_&&runtime_->activeNativeWork()){referenceTail_=true;referenceTailHybrid_=hybrid;return;}
  referenceTail_=false;runtime_.reset();cpu_->interpreterAllowed=true;cpu_->nativeFetchClocksOnly=false;cpu_->nativeLowBase=nullptr;
  if(hybrid){if(!live_)live_=std::make_unique<LiveMidiController>(*cpu_);}else live_.reset();
 }
 void selectLegacy(){
  if(recorded_||!board_.ready||!BoardComparisonAccess::started(board_))throw std::logic_error("Native KS runtime requires completed original boot without replay");
  if(runtime_&&!referenceTail_)return;
  if(runtime_&&runtime_->activeNativeWork()){referenceTail_=false;return;}
  auto state=std::make_unique<LiveMidiController>(*cpu_);auto runtime=std::make_unique<RuntimeController>(*cpu_,*state);
  live_=std::move(state);runtime_=std::move(runtime);referenceTail_=false;
  nativeH8Steps_=cpu_->interpretedSteps;cpu_->interpreterAllowed=false;cpu_->nativeFetchClocksOnly=true;
  cpu_->nativeLowBase=board_.overlay?cpu_->memory+0xa00000:rom_.data();cpu_->nativeLowSize=board_.overlay?0x200000:unsigned(rom_.size());cpu_->nativeLowWritable=board_.overlay;
 }
 Board&board_;Proxy proxy_;std::unique_ptr<ControllerCpu>cpu_;
 const std::vector<uint8_t>&rom_;
 std::unique_ptr<LiveMidiController>live_;std::unique_ptr<RuntimeController>runtime_;std::unique_ptr<RecordedController>recorded_;
 bool initialized_=false,referenceTail_=false,referenceTailHybrid_=false,nativeCodeSealed_=true,eventTransport_=false,eventControl_=false;
 bool nativeTick_=false,checkMain_=false,startupPhase_=false,finishTail_=false;
 uint64_t nativeH8Steps_=0,sliceLimit_=0;unsigned sliceBudget_=0;
 uint64_t transportJobs_=0,transportBoundaries_=0,controlJobs_=0,controlBoundaries_=0;
};
namespace {
std::unique_ptr<BoardComparisonBackend>createLegacy(Board*board,const std::vector<uint8_t>&rom){if(!board)throw std::invalid_argument("Legacy comparison requires real Board");return std::make_unique<LegacyBoardComparisonBackend>(*board,rom);}
struct Registration{Registration(){setBoardComparisonFactoryForTests(createLegacy);}}registration;
}
namespace test {
void startLegacyStartupForTests(Board&b){auto*p=dynamic_cast<LegacyBoardComparisonBackend*>(BoardComparisonAccess::backend(b));if(!p)throw std::logic_error("Legacy cold comparison backend unavailable");p->startLegacyStartupForTests();}
std::unique_ptr<Board>makeLegacyBoard(const std::vector<uint8_t>&os,const std::vector<uint8_t>&bank,const std::vector<uint8_t>&storage,bool isolateParts,bool legacyStartup){
 if(!boardComparisonAvailable())throw std::logic_error("Explicit legacy comparison factory was not linked");
 auto b=std::make_unique<Board>(os,bank,storage,isolateParts,false);
 if(legacyStartup)startLegacyStartupForTests(*b);
 return b;
}
ControllerCpu&comparisonCpu(Board&b){auto*p=dynamic_cast<LegacyBoardComparisonBackend*>(BoardComparisonAccess::backend(b));if(!p)throw std::logic_error("Board has no legacy comparison CPU");return p->cpuForTests();}
const RuntimeController*legacyRuntime(const Board&b){const auto*p=dynamic_cast<const LegacyBoardComparisonBackend*>(BoardComparisonAccess::backend(b));return p?p->runtimeForTests():nullptr;}
const LiveMidiController*legacyMidi(const Board&b){const auto*p=dynamic_cast<const LegacyBoardComparisonBackend*>(BoardComparisonAccess::backend(b));return p?p->midiForTests():nullptr;}
std::array<uint64_t,4>legacyBatchMetrics(const Board&b){const auto*p=dynamic_cast<const LegacyBoardComparisonBackend*>(BoardComparisonAccess::backend(b));if(!p)throw std::logic_error("Board has no legacy comparison metrics");return p->batchesForTests();}
uint64_t legacyControllerCheckpoint(const Board&b){if(!BoardComparisonAccess::backend(b))throw std::logic_error("Board has no legacy checkpoint");return BoardComparisonAccess::checkpoint(b);}
}
}
