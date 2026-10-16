#include "ksDspWorkers.h"
#include "dsp56kBase/threadtools.h"
#include "dsp56kBase/audioworkgroup.h"
#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__x86_64__)
#include <immintrin.h>
#endif

namespace ks {
namespace {
inline void relax() noexcept {
#ifdef _WIN32
    YieldProcessor();
#elif defined(__x86_64__)
    _mm_pause();
#elif defined(__aarch64__)
    __asm__ __volatile__("yield");
#else
    std::this_thread::yield();
#endif
}
// Bounded spin followed by blocking: idle DSP threads do not busy-wait while
// the audio queue is full. This changes no system timer or scheduler policy.
struct alignas(64) Signal {
    std::atomic<uint32_t> value{0};
    std::mutex mutex;
    std::condition_variable changed;
#ifdef _WIN32
    static_assert(sizeof(std::atomic<uint32_t>)==4 && std::atomic<uint32_t>::is_always_lock_free);
    std::atomic<bool> parked{false};
    struct AddressApi {
        using Wait=BOOL(WINAPI*)(volatile VOID*,PVOID,SIZE_T,DWORD);
        using Wake=VOID(WINAPI*)(PVOID);
        Wait wait=nullptr;Wake wake=nullptr;
        AddressApi(){const auto module=GetModuleHandleW(L"kernel32.dll");
            wait=reinterpret_cast<Wait>(GetProcAddress(module,"WaitOnAddress"));
            wake=reinterpret_cast<Wake>(GetProcAddress(module,"WakeByAddressSingle"));
        }
    };
    static const AddressApi& api(){static const AddressApi result;return result;}
#endif
    void publish(uint32_t v) noexcept {
#ifdef _WIN32
        if(api().wait && api().wake){
        value.store(v,std::memory_order_release);
        if(parked.exchange(false,std::memory_order_seq_cst))api().wake(&value);
        return;
        }
#endif
        {std::lock_guard lock(mutex);value.store(v,std::memory_order_release);}
        changed.notify_one();
    }
    uint32_t waitChange(uint32_t old) {
        // A KS slice has only a few microseconds of DSP work. On macOS the
        // short 256-probe window parks on almost every slice, making condvar
        // wakeups cost more than both DSPs. Cover the ordinary handoff before
        // parking; the bound still lets idle helpers sleep when audio is full.
#ifdef __APPLE__
        constexpr unsigned spinProbes=8192;
#else
        constexpr unsigned spinProbes=256;
#endif
        for(unsigned n=0;n<spinProbes;++n){auto v=value.load(std::memory_order_acquire);if(v!=old)return v;relax();}
#ifdef _WIN32
        if(api().wait && api().wake)for(;;){
            parked.exchange(true,std::memory_order_seq_cst);
            auto v=value.load(std::memory_order_acquire);
            if(v!=old){parked.exchange(false,std::memory_order_seq_cst);return v;}
            api().wait(&value,&old,sizeof(old),INFINITE);
        }
#endif
        std::unique_lock lock(mutex);
        changed.wait(lock,[&]{return value.load(std::memory_order_acquire)!=old;});
        return value.load(std::memory_order_acquire);
    }
};
}
struct DspWorkers::Worker {
    Signal command,completed;
    // Payload is owned by the coordinator before command.release, then by this
    // worker until completed.release. No mutation overlaps an active slice.
    int64_t target=0;
    int rate=0;
    unsigned block=0;
    bool sampled=false,stop=false;
    std::exception_ptr failure;
    uint32_t generation=0;
    std::thread thread;
#ifdef __APPLE__
    void* inlineContext=nullptr;
    Execute inlineExecute=nullptr;
#endif
    Worker(void* context,Execute execute,unsigned index) {
#ifdef __APPLE__
        // The existing audio worker executes DSP A while the helper executes B.
        // A third runnable realtime thread otherwise only spins at the barrier.
        if(index==0){inlineContext=context;inlineExecute=execute;return;}
#endif
        thread=std::thread([this,context,execute,index]{
            dsp56k::ThreadTools::setCurrentThreadName(index?"KS DSP B experiment":"KS DSP A experiment");
            // Same priority/realtime parameters as the existing live KS worker.
            dsp56k::ThreadTools::setCurrentThreadPriority(dsp56k::ThreadPriority::Highest);
            dsp56k::AudioWorkgroup::Member workgroup;
            int previousRate=0;unsigned previousBlock=0;uint32_t observed=0;
            for(;;){
                observed=command.waitChange(observed);
                if(stop)break;
                try {
                    if(rate!=previousRate || block!=previousBlock){
                        dsp56k::ThreadTools::setCurrentThreadRealtimeParameters(rate,int(block));
                        previousRate=rate;previousBlock=block;
                    }
                    workgroup.update();
                    execute(context,index,target,sampled);
                }catch(...){failure=std::current_exception();}
                completed.publish(observed);
            }
        });
    }
    ~Worker(){if(thread.joinable()){stop=true;command.publish(++generation);thread.join();}}
};
DspWorkers::DspWorkers(void* context,Execute execute,int r,unsigned b):rate(r),block(b) {
    for(unsigned i=0;i<2;++i)workers[i]=std::make_unique<Worker>(context,execute,i);
}
DspWorkers::~DspWorkers()=default;
void DspWorkers::configure(int r,unsigned b) noexcept {rate=r;block=b;}
void DspWorkers::run(const std::array<int64_t,2>& targets,bool sampled) {
    for(unsigned i=0;i<2;++i){auto& w=*workers[i];
        w.target=targets[i];w.sampled=sampled;w.rate=rate;w.block=block;
        w.failure=nullptr;
#ifdef __APPLE__
        if(i==0)continue;
#endif
        w.command.publish(++w.generation);
    }
#ifdef __APPLE__
    auto& inlineWorker=*workers[0];
    try{inlineWorker.inlineExecute(inlineWorker.inlineContext,0,inlineWorker.target,sampled);}
    catch(...){inlineWorker.failure=std::current_exception();}
#endif
    // Always drain both workers, including when one failed. Exception unwinding
    // must never leave a DSP writing into a board being destroyed.
    for(auto& w:workers){
#ifdef __APPLE__
        if(w->inlineExecute)continue;
#endif
        const auto previous=w->generation-1;
        if(w->completed.value.load(std::memory_order_acquire)!=w->generation)w->completed.waitChange(previous);
    }
    for(auto& w:workers)if(w->failure)std::rethrow_exception(w->failure);
}
}
