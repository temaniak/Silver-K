#pragma once
#include "ksBoard.h"
#include "ksAudioInput.h"
#include "ksWorkerPlacement.h"
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include "baseLib/performanceDiagnostics.h"
namespace ks {
enum class ControllerMode {Environment=-1,H8=0,Native=1,Hybrid=2};
// DSP/H8 ownership belongs to the worker. Host audio does bounded queue access.
class Device {
public:
    static bool environmentDspWorkersEnabled();
    Device(const std::vector<uint8_t>& os,const std::vector<uint8_t>& bank,const std::vector<uint8_t>& flash,double hostRate,int block,const std::vector<uint8_t>& program={},baseLib::performance::Diagnostics* performance=nullptr,ControllerMode controller=ControllerMode::Environment,bool dspWorkersEnabled=environmentDspWorkersEnabled());
    ~Device();
    bool valid() const{return running.load();}
    const std::string& error()const{return failure;}
    bool midi(const uint8_t*,size_t,uint64_t offset=0);
    bool partMidi(unsigned part,const uint8_t*,size_t);
    bool selectDrumNote(unsigned part,uint8_t note,uint64_t offset=0);
    uint64_t drumEditRevision()const{return publishedDrumRevision.load();}
    void process(float* left,float* right,size_t n,bool offline=false,const float* input=nullptr,const float* inputRight=nullptr);
    void process(const std::array<float*,OutputCount>&,size_t n,bool offline=false,const float* input=nullptr,const float* inputRight=nullptr);
    std::vector<uint8_t> dumpProgram(); // Control thread, canonical mirrors; no guest execution.
    std::array<uint64_t,4> diagnostics();
    std::vector<uint8_t> saveFlash(); // Excludes OS and external wave bank.
    std::array<uint8_t,256> snapshot(unsigned part=0)const;
    State fullSnapshot()const;
    uint64_t underruns()const{return underrun.load();}
    uint64_t overflows()const{return overflow.load();}
    uint64_t inputOverflows()const{return inputOverflow.load();}
    unsigned latency()const{return lead.load();}
    const char* workerWaitBackend()const{return "poll-50us";}
    const char* dspExecutionBackend()const{return parallelDSPs.load()?"two-workers-slice-barrier":"sequential";}
    bool setDspWorkersEnabled(bool,std::string& error); // Control thread; applied by the worker between samples.
    static ControllerMode environmentControllerMode();
    ControllerMode controllerMode()const{return currentController.load();}
    const char* controllerBackend()const{return activeControllerBackend.load();}
    // Control thread only; the worker applies/acknowledges between samples.
    // Failed native validation retains the current controller and state.
    bool setControllerMode(ControllerMode,std::string& error);
    bool demoAvailable()const{return hasDemo.load();}
    bool demoActive()const{return playingDemo.load();}
    bool setDemoSong(bool enabled){if(!valid() || (enabled && !demoAvailable()))return false;requestedDemo.store(enabled?1:0);return true;}
    const WorkerPlacement& workerPlacement()const{return placement;}
    void setAudioBuffer(unsigned block,unsigned floor);
    void ensureAudioBlock(unsigned block) noexcept;
    void setBufferFloor(unsigned frames){lead.store(std::max(baseLead,std::min(4096u,frames)));}
private:
    baseLib::performance::Diagnostics* performance;
    std::atomic<bool> parallelDSPs;
    std::atomic<int> requestedDspWorkers{-1};
    std::atomic<ControllerMode> currentController;
    std::atomic<const char*> activeControllerBackend{"original-h8"};
    std::atomic<int> requestedController{-1};
    std::atomic<int> requestedDemo{-1};
    std::atomic<bool> hasDemo{false},playingDemo{false};
    std::mutex controllerMutex;
    std::condition_variable controllerCV;
    bool controllerCompleted=false;
    std::string controllerError;
    bool dspWorkersCompleted=false;
    std::string dspWorkersError;
    WorkerPlacement placement;
    struct Event{uint64_t due;uint8_t value;uint8_t part=255,length=0;bool drum=false;};
    std::atomic<uint64_t> publishedDrumRevision{0};
    struct Frame{uint64_t due;std::array<float,OutputCount> value;};
    std::unique_ptr<Board> board;
    kstation::Queue<Event,65536> events;
    kstation::Queue<Frame,8192> output;
    struct InputFrame{uint64_t due;float value;};
    kstation::Queue<InputFrame,8192> input;
    std::atomic<uint64_t> inputOverflow{0};
    std::unique_ptr<AudioInput> inputConverter;
    struct SnapshotSlot{State value;mutable std::atomic<unsigned> readers{0};};
    std::array<SnapshotSlot,3> snapshots;
    std::atomic<unsigned> publishedSnapshot{0};
    State initialSnapshot;bool initialPublished=false;
    std::mutex flashSnapshotMutex;
    std::vector<uint8_t> flashSnapshot;
    uint64_t publishedFlashRevision=~uint64_t(0);
    std::atomic<uint64_t> diagnosticProduced{0},diagnosticMidi{0},diagnosticWave{0};
    std::atomic<uint64_t> consumed{0},underrun{0},overflow{0};
    std::atomic<bool> stopping{false},running{false},pause{false},paused{false};
    std::thread worker;
    std::mutex startMutex,coldMutex;
    std::condition_variable startCV;
    bool started=false;
    unsigned baseLead;
    std::atomic<unsigned> lead;
    std::atomic<unsigned> realtimeBlock{256};
    std::string failure;
    double rate,phase=0;
    uint64_t produced=0;
    std::array<std::array<double,OutputCount>,4> history{};
    std::array<float,OutputCount> nextSample(float input=0);
    void publish();
    void suspend();
    void resume();
};
}
