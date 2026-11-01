#include "ksDevice.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include "dsp56kBase/threadtools.h"
#include "dsp56kBase/audioworkgroup.h"
namespace ks {
ControllerMode Device::environmentControllerMode(){
 if(!boardComparisonAvailable())return ControllerMode::Native;
 const auto* v=std::getenv("SILVER_KS_CONTROLLER");
 return v&&std::strcmp(v,"native")==0?ControllerMode::Native:v&&std::strcmp(v,"native-midi")==0?ControllerMode::Hybrid:ControllerMode::H8;
}
bool Device::environmentDspWorkersEnabled(){const auto* v=std::getenv("SILVER_KS_DSP_THREADS");return v&&std::strcmp(v,"2")==0;}
Device::Device(const std::vector<uint8_t>& os,const std::vector<uint8_t>& bank,const std::vector<uint8_t>& flash,double hostRate,int block,const std::vector<uint8_t>& program,baseLib::performance::Diagnostics* diagnostics,ControllerMode controller,bool dspWorkersEnabled)
:performance(diagnostics),parallelDSPs(dspWorkersEnabled),currentController(controller==ControllerMode::Environment?environmentControllerMode():controller),baseLead(std::min(4096,std::max(256,block+128))),lead(baseLead),rate(hostRate){
 if(!boardComparisonAvailable())currentController.store(ControllerMode::Native);
 realtimeBlock.store(unsigned(std::max(64,block)));
 inputConverter=std::make_unique<AudioInput>(rate,DSP::SampleRate);
 const auto* cores=std::getenv("SILVER_KS_WORKER_CORES");
 const bool performanceCores=cores&&std::strcmp(cores,"p")==0;
 worker=std::thread([this,os,bank,flash,program,block,performanceCores]{
  dsp56k::ThreadTools::setCurrentThreadName("KS H8S / dual DSP");
  // Boot and JIT compilation have no audio deadline. Enter the realtime band
  // only after warm-up, so a long cold compile cannot demote the live worker.
  dsp56k::ThreadTools::setCurrentThreadPriority(dsp56k::ThreadPriority::High);
  dsp56k::AudioWorkgroup::Member audioWorkgroup;
  try{board=std::make_unique<Board>(os,bank,flash,true,controllerMode()==ControllerMode::Native);if(!board->boot())throw std::runtime_error("KS controller boot timeout");
   if(controllerMode()==ControllerMode::Native)board->enableNativeController();else if(controllerMode()==ControllerMode::Hybrid)board->enableLiveMidiController();
   activeControllerBackend.store(board->controllerBackend());
   hasDemo.store(board->demoAvailable());
   if(program.size()==270 || program.size()==1038){
    auto select=singleSelection(program.data()+13,flash[0x1ff08]);if(program.size()==1038){select[2]=65;select[4]=0;}
    board->enqueueMidi(select.data(),select.size());board->enqueueMidi(program.data(),program.size());
    if(program.size()==270){const auto metadata=singleMetadata(program.data()+13,flash[0x1ff08]);board->enqueueMidi(metadata.data(),metadata.size());}
    for(unsigned n=0;n<20000;++n)board->renderSample();}
   // Compile ordinary voice branches before opening audio. Keep warm-up notes
   // out of the effect buffers, then restore the patch through original MIDI.
   const uint8_t channel=flash[0x1ff08]&15;
   const auto originalState=board->state();const auto original=stateMessage(originalState);
   if(originalState.performance){const uint8_t select[]{uint8_t(0xb0|channel),32,33,uint8_t(0xc0|channel),0};board->enqueueMidi(select,sizeof(select));
    State warm=originalState;warm.performance=false;const auto message=stateMessage(warm);board->enqueueMidi(message.data(),message.size());for(unsigned n=0;n<20000;++n)board->renderSample();}
   const uint8_t warmOn[]{uint8_t(0xb0|channel),7,127,uint8_t(0xb0|channel),91,0,uint8_t(0xb0|channel),92,0,uint8_t(0xb0|channel),93,0,
                         uint8_t(0x90|channel),36,64,uint8_t(0x90|channel),69,64,uint8_t(0x90|channel),96,64};
   board->enqueueMidi(warmOn,sizeof(warmOn));for(unsigned n=0;n<8192;++n)board->renderSample();
   // All Sound Off silences voices but leaves the original controller's held
   // keys intact. Pair every warm-up Note On before a mono patch can reuse them.
   const uint8_t warmOff[]{uint8_t(0x80|channel),36,0,uint8_t(0x80|channel),69,0,uint8_t(0x80|channel),96,0,
                           uint8_t(0xb0|channel),120,0};
   board->enqueueMidi(warmOff,sizeof(warmOff));for(unsigned n=0;n<45340;++n)board->renderSample();
   // Compile the shared 16-voice/multitimbral paths even when starting in
   // Single mode. First simultaneous part notes must not trigger a cold JIT
   // burst after audio opens. These are temporary original MIDI programs.
   State warm=originalState;warm.performance=true;warm.activeMask=15;
   for(unsigned part=0;part<PartCount;++part){if(!originalState.performance)warm.parts[part]=originalState.parts[0];auto& p=warm.parts[part];
    p[0]=uint8_t((p[0]&0x60)|0x10);p[65]=0;p[100]=p[107]=p[110]=0;
    p[179]=16;p[180]=part&1;p[181]=uint8_t(part);p[182]=0;p[183]=127;p[184]=p[185]=64;p[168]=0;
    if(board->zeroBank())for(unsigned osc=0;osc<3;++osc)if(p[128+osc]>=8)p[128+osc]=0;
   }
   warm.parts[0][189]=1;warm.parts[0][190]=15;
   const uint8_t selectMulti[]{uint8_t(0xb0|channel),32,65,uint8_t(0xc0|channel),0};board->enqueueMidi(selectMulti,sizeof(selectMulti));
   const auto warmDump=stateMessage(warm);board->enqueueMidi(warmDump.data(),warmDump.size());for(unsigned n=0;n<20000;++n)board->renderSample();
   for(uint8_t part=0;part<PartCount;++part)for(uint8_t note:{36,60,69,96}){const uint8_t on[]{uint8_t(0x90|part),note,32};board->enqueueMidi(on,3);}
   for(unsigned n=0;n<8192;++n)board->renderSample();
   for(uint8_t part=0;part<PartCount;++part){for(uint8_t note:{36,60,69,96}){const uint8_t off[]{uint8_t(0x80|part),note,0};board->enqueueMidi(off,3);}const uint8_t off[]{uint8_t(0xb0|part),120,0};board->enqueueMidi(off,3);}
   for(unsigned n=0;n<45340;++n)board->renderSample();
   if(!originalState.performance){const auto select=singleSelection(originalState.parts[0].data(),channel);board->enqueueMidi(select.data(),select.size());}
   board->enqueueMidi(original.data(),original.size());
   if(!originalState.performance){const auto metadata=singleMetadata(originalState.parts[0].data(),channel);board->enqueueMidi(metadata.data(),metadata.size());}
   // Clear every warm-up key/sustain owner after restoring the user's patch.
   // All Sound Off alone can leave held keys which become audible when the
   // vocoder/mixer is edited. Finish the restore with empty key/sustain state
   // within the existing cold preroll, before publishing any audio.
   for(uint8_t channel=0;channel<16;++channel)for(uint8_t cc:{64,123,120}){
    const uint8_t off[]{uint8_t(0xb0|channel),cc,0};board->enqueueMidi(off,3);
   }
   for(unsigned n=0;n<20000;++n)board->renderSample();
   for(auto& p:history){auto a=board->renderSample();for(unsigned c=0;c<OutputCount;++c)p[c]=double(a[c])/8388608;}
   publish();
   for(unsigned i=0;i<lead;++i){output.push(Frame{produced,nextSample()});++produced;}
   dsp56k::ThreadTools::setCurrentThreadPriority(dsp56k::ThreadPriority::Highest);
   dsp56k::ThreadTools::setCurrentThreadRealtimeParameters(int(rate),std::max(64,block));
   placement=applyWorkerPlacement(performanceCores);
   if(parallelDSPs.load())board->enableDspWorkers(int(rate),unsigned(std::max(64,block)));
   running.store(true);{std::lock_guard lock(startMutex);started=true;}startCV.notify_one();
   auto configuredBlock=realtimeBlock.load();
   board->setPerformanceDiagnostics(performance);
   baseLib::performance::WorkerSegment workTiming(performance);
   while(!stopping.load()){
    if(pause.load()){workTiming.flush();paused.store(true);while(pause.load() && !stopping.load())std::this_thread::sleep_for(std::chrono::microseconds(50));paused.store(false);continue;}
    const int dspRequest=requestedDspWorkers.load();
    if(dspRequest>=0){
     requestedDspWorkers.store(-1);
     // No slice is active here. Creating/joining helpers never touches the host callback.
     workTiming.flush();std::string error;
     try{if(dspRequest)board->enableDspWorkers(int(rate),realtimeBlock.load());else board->disableDspWorkers();parallelDSPs.store(dspRequest!=0);}
     catch(const std::exception& e){error=e.what();}
     {std::lock_guard lock(controllerMutex);dspWorkersError=std::move(error);dspWorkersCompleted=true;}controllerCV.notify_one();
    }
    const int request=requestedController.load();
    if(request>=0){
     requestedController.store(-1);
     workTiming.flush();std::string error;
     try{const auto mode=ControllerMode(request);
      if(mode==ControllerMode::Native)board->enableNativeController();
      else if(mode==ControllerMode::Hybrid)board->enableLiveMidiController();
      else board->enableNativeController(false);
      activeControllerBackend.store(board->controllerBackend());
      currentController.store(mode);
     }catch(const std::exception& e){error=e.what();}
     {std::lock_guard lock(controllerMutex);controllerError=std::move(error);controllerCompleted=true;}controllerCV.notify_one();
    }
    const auto block=realtimeBlock.load();if(block!=configuredBlock){dsp56k::ThreadTools::setCurrentThreadRealtimeParameters(int(rate),int(block));board->configureDspWorkers(int(rate),block);configuredBlock=block;}
    if(requestedDemo.load(std::memory_order_relaxed)>=0){const int demo=requestedDemo.exchange(-1);if(demo>=0)board->setDemoSong(demo!=0);}
    audioWorkgroup.update();
    // Event parking reduced wake latency but regressed KS throughput in repeated
    // 64- and 128-frame tests. Keep the legacy wait for KS until that is resolved.
    if(produced>=consumed.load()+lead){workTiming.flush();baseLib::performance::Scope sleepTiming(performance,baseLib::performance::Metric::Sleep);std::this_thread::sleep_for(std::chrono::microseconds(50));continue;}
    workTiming.start();
    Event e{};while(!board->drumEditBusy() && events.peek(e) && e.due<=produced){
     if(e.drum){if(!board->selectDrumNote(e.part,e.value))break;events.pop(e);continue;}
     if(e.part<PartCount){
      std::array<uint8_t,9> packet{};const auto part=e.part,length=e.length;
      for(unsigned n=0;n<length;++n){events.peekAt(n,e);packet[n]=e.value;}
      if(!board->enqueuePartMidi(part,packet.data(),length))break;
      for(unsigned n=0;n<length;++n)events.pop(e);
     }else{if(!board->enqueueMidi(&e.value,1))break;events.pop(e);}
    }
    InputFrame incoming{};float sample=0;
    while(input.peek(incoming) && incoming.due<=produced){input.pop(incoming);if(incoming.due==produced)sample=incoming.value;}
    Frame f{produced,nextSample(sample)};if(produced>=consumed.load())output.push(f);++produced;
    if(!(produced&1023))publish();
    workTiming.frame();
   }
  }catch(const std::exception& e){{std::lock_guard lock(controllerMutex);failure=e.what();running.store(false);}controllerCV.notify_one();{std::lock_guard lock(startMutex);started=true;}startCV.notify_one();}
 });
 std::unique_lock lock(startMutex);startCV.wait(lock,[this]{return started;});
}
bool Device::setDspWorkersEnabled(bool enabled,std::string& error){
 std::lock_guard serial(coldMutex);std::unique_lock lock(controllerMutex);error.clear();
 if(!running.load() || stopping.load()){error=failure.empty()?"KS engine is stopped":failure;return false;}
 if(enabled==parallelDSPs.load())return true;
 dspWorkersCompleted=false;dspWorkersError.clear();requestedDspWorkers.store(enabled?1:0);
 controllerCV.wait(lock,[this]{return dspWorkersCompleted || !running.load() || stopping.load();});
 if(!dspWorkersCompleted){error=failure.empty()?"KS engine stopped before switching DSP workers":failure;return false;}
 error=dspWorkersError;return error.empty();
}
bool Device::setControllerMode(ControllerMode mode,std::string& error){
 if(mode!=ControllerMode::Native && !boardComparisonAvailable()){error="KS H8/hybrid is available only in comparison tests";return false;}
 if(mode!=ControllerMode::H8 && mode!=ControllerMode::Native && mode!=ControllerMode::Hybrid){error="Invalid KS controller selection";return false;}
 std::lock_guard serial(coldMutex);std::unique_lock lock(controllerMutex);error.clear();
 if(!running.load() || stopping.load()){error=failure.empty()?"KS engine is stopped":failure;return false;}
 if(mode==controllerMode())return true;
 controllerCompleted=false;controllerError.clear();requestedController.store(int(mode));
 controllerCV.wait(lock,[this]{return controllerCompleted || !running.load() || stopping.load();});
 if(!controllerCompleted){error=failure.empty()?"KS engine stopped before switching controller":failure;return false;}
 error=controllerError;return error.empty();
}
void Device::setAudioBuffer(unsigned block,unsigned floor){baseLead=std::min(4096u,std::max(256u,block+128));realtimeBlock.store(std::max(64u,block));setBufferFloor(floor);}
void Device::ensureAudioBlock(unsigned block) noexcept{const auto needed=std::min(4096u,std::max(256u,block+128));auto current=lead.load(std::memory_order_relaxed);for(unsigned attempt=0;attempt<4 && current<needed;++attempt)if(lead.compare_exchange_weak(current,needed)){realtimeBlock.store(std::max(64u,block));break;}}
Device::~Device(){stopping.store(true);pause.store(false);if(worker.joinable())worker.join();}
std::array<float,OutputCount> Device::nextSample(float input){
 std::array<float,OutputCount> out{};
 for(unsigned c=0;c<OutputCount;++c){auto a=history[0][c],b=history[1][c],d=history[2][c],e=history[3][c];
  out[c]=float(b+0.5*phase*(d-a+phase*(2*a-5*b+4*d-e+phase*(3*(b-d)+e-a))));}
 inputConverter->push(input);
 const double ratio=DSP::SampleRate/rate;phase+=ratio;
 while(phase>=1){phase-=1;
  // Both the effects DSP and voice DSP consume the mono ADC stream.
  const auto word=inputConverter->sample(phase/ratio);
  if(!board->dsp(0).pushAudioInput(word) || !board->dsp(1).pushAudioInput(word))throw std::runtime_error("KS ADC queue overflow");
  history[0]=history[1];history[1]=history[2];history[2]=history[3];auto a=board->renderSample();for(unsigned c=0;c<OutputCount;++c)history[3][c]=double(a[c])/8388608;}
 return out;
}
bool Device::midi(const uint8_t* b,size_t n,uint64_t offset){
 const auto due=consumed.load()+lead+offset;
 if(!events.pushGenerated(n,[&](size_t i){return Event{due,b[i]};})){++overflow;return false;}return true;
}
bool Device::partMidi(unsigned part,const uint8_t* b,size_t n){
 if(part>=PartCount || (n!=3 && n!=9))return false;
 const auto due=consumed.load()+lead;
 if(!events.pushGenerated(n,[&](size_t i){return Event{due,b[i],uint8_t(part),uint8_t(n)};})){++overflow;return false;}return true;
}
bool Device::selectDrumNote(unsigned part,uint8_t note,uint64_t offset){
 if(part>=PartCount || note<36 || note>84)return false;
 if(!events.push(Event{consumed.load()+lead+offset,note,uint8_t(part),0,true})){++overflow;return false;}return true;
}
void Device::process(float* l,float* r,size_t n,bool offline,const float* input,const float* inputRight){
 process({l,r,nullptr,nullptr},n,offline,input,inputRight);
}
void Device::process(const std::array<float*,OutputCount>& channels,size_t n,bool offline,const float* inputLeft,const float* inputRight){
 using namespace baseLib::performance;
 const auto epoch=performance?performance->epoch():0;
 if(epoch)performance->add(Metric::QueueDepth,epoch,output.size());
 uint64_t missing=0,stale=0;
 const auto begin=consumed.load();const auto inputLead=lead.load();Frame f{};
 for(size_t i=0;i<n;++i){auto due=begin+i;
  // Read aliased host inputs before writing outputs. Publish before consumed
  // so the worker cannot reach this deadline before the sample is available.
  const float sample=inputLeft?(inputRight?AudioInput::mono(inputLeft[i],inputRight[i]):AudioInput::finite(inputLeft[i])):0;
  if(!input.push(InputFrame{due+inputLead,sample}))++inputOverflow;
  // This wait is exclusively for a host explicitly selecting offline rendering.
  // The realtime branch has no waits, I/O, locks, heap allocation or guest execution.
  if(offline){while(running.load() && !stopping.load() && output.size()==0)std::this_thread::yield();}
  while(output.peek(f) && f.due<due){output.pop(f);if(epoch)++stale;}
  if(output.peek(f) && f.due==due){output.pop(f);for(unsigned c=0;c<OutputCount;++c)if(channels[c])channels[c][i]=f.value[c];}
  else {for(auto* channel:channels)if(channel)channel[i]=0;++underrun;if(epoch)++missing;}
  consumed.store(due+1,std::memory_order_release);
 }
 // A fast offline caller can empty the queue at the final sample. Refill
 // before returning so an immediate offline -> realtime transition starts
 // with the advertised lead, without waiting in the realtime callback.
 if(offline)while(running.load() && !stopping.load() && output.size()<lead)std::this_thread::yield();
 if(epoch){performance->add(Metric::MissingFrames,epoch,missing);performance->add(Metric::StaleFrames,epoch,stale);}
}
void Device::publish(){
 playingDemo.store(board->demoActive());
 baseLib::performance::Scope snapshotTiming(performance,baseLib::performance::Metric::Snapshot);
 diagnosticProduced.store(produced);diagnosticMidi.store(board->midiReceived);diagnosticWave.store(board->waveReads);
 if(publishedFlashRevision!=board->flashRevision){
  // Host serialization may copy a large bank. The worker never waits for it.
  std::unique_lock lock(flashSnapshotMutex,std::try_to_lock);
  if(lock.owns_lock()){board->copyUserFlash(flashSnapshot);publishedFlashRevision=board->flashRevision;}
 }
 const auto state=board->state();if(!initialPublished){initialSnapshot=state;initialPublished=true;}
 const auto current=publishedSnapshot.load();
 for(unsigned slot=0;slot<snapshots.size();++slot)if(slot!=current && !snapshots[slot].readers.load()){
  snapshots[slot].value=state;publishedSnapshot.store(slot);publishedDrumRevision.store(board->drumEditRevision);return;
 }
 // Readers may pin both retired buffers. Skip this GUI update, never wait.
}
State Device::fullSnapshot()const{
 for(unsigned attempt=0;attempt<4;++attempt){const auto index=publishedSnapshot.load();auto& slot=snapshots[index];slot.readers.fetch_add(1);
  if(index!=publishedSnapshot.load()){slot.readers.fetch_sub(1);continue;}
  const auto state=slot.value;slot.readers.fetch_sub(1);return state;
 }
 return initialSnapshot; // A complete immutable state, never a torn/zero patch.
}
std::array<uint8_t,256> Device::snapshot(unsigned part)const{return fullSnapshot().parts[std::min(part,PartCount-1)];}
void Device::suspend(){pause.store(true);while(running.load() && !paused.load())std::this_thread::sleep_for(std::chrono::microseconds(50));}
void Device::resume(){pause.store(false);while(running.load() && paused.load())std::this_thread::sleep_for(std::chrono::microseconds(50));}
std::array<uint64_t,4> Device::diagnostics(){return {diagnosticProduced.load(),consumed.load(),diagnosticMidi.load(),diagnosticWave.load()};}
std::vector<uint8_t> Device::saveFlash(){std::lock_guard lock(flashSnapshotMutex);return flashSnapshot;}
std::vector<uint8_t> Device::dumpProgram(){
 // Sending a dump request and rendering until its reply advanced guest time
 // without advancing produced/consumed. A host state read could then shift
 // the arp/clock and buffered sound. Pack the same canonical controller
 // mirrors read-only in both Single and Performance, keeping the audio epoch.
 return stateMessage(fullSnapshot());
}
}
