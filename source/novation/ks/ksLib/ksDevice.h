#pragma once
#include "ksBoard.h"
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
    Device(const std::vector<uint8_t>& os,const std::vector<uint8_t>& bank,const std::vector<uint8_t>& flash,double hostRate,int block,const std::vector<uint8_t>& program={},baseLib::performance::Diagnostics* performance=nullptr,ControllerMode controller=ControllerMode::Environment);
    ~Device();
    bool valid() const{return running.load();}
    const std::string& error()const{return failure;}
    bool midi(const uint8_t*,size_t,uint64_t offset=0);
    bool partMidi(unsigned part,const uint8_t*,size_t);
    void process(float* left,float* right,size_t n,bool offline=false);
    void process(const std::array<float*,OutputCount>&,size_t n,bool offline=false);
    std::vector<uint8_t> dumpProgram(); // Control thread, canonical mirrors; no guest execution.
    std::array<uint64_t,4> diagnostics();
    std::vector<uint8_t> saveFlash(); // Excludes OS and external wave bank.
    std::array<uint8_t,256> snapshot(unsigned part=0)const;
    State fullSnapshot()const;
    uint64_t underruns()const{return underrun.load();}
    uint64_t overflows()const{return overflow.load();}
    unsigned latency()const{return lead.load();}
    const char* workerWaitBackend()const{return "poll-50us";}
    const char* dspExecutionBackend()const{return parallelDSPs?"two-workers-slice-barrier":"sequential";}
    static ControllerMode environmentControllerMode();
    ControllerMode controllerMode()const{return currentController.load();}
    const char* controllerBackend()const{return activeControllerBackend.load();}
    // Control thread only; the worker applies/acknowledges between samples.
    // Failed native validation retains the current controller and state.
    bool setControllerMode(ControllerMode,std::string& error);
    const WorkerPlacement& workerPlacement()const{return placement;}
    void setAudioBuffer(unsigned block,unsigned floor);
    void ensureAudioBlock(unsigned block) noexcept;
    void setBufferFloor(unsigned frames){lead.store(std::max(baseLead,std::min(4096u,frames)));}
private:
    baseLib::performance::Diagnostics* performance;
    const bool parallelDSPs;
    std::atomic<ControllerMode> currentController;
    std::atomic<const char*> activeControllerBackend{"original-h8"};
    std::atomic<int> requestedController{-1};
    std::mutex controllerMutex;
    std::condition_variable controllerCV;
    bool controllerCompleted=false;
    std::string controllerError;
    WorkerPlacement placement;
    struct Event{uint64_t due;uint8_t value;uint8_t part=255,length=0;};
    struct Frame{uint64_t due;std::array<float,OutputCount> value;};
    std::unique_ptr<Board> board;
    kstation::Queue<Event,65536> events;
    kstation::Queue<Frame,8192> output;
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
    std::array<float,OutputCount> nextSample();
    void publish();
    void suspend();
    void resume();
};
}
