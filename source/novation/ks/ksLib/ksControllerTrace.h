#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ks {
// Raw traces can contain private DSP uploads/presets. Keep files local/ignored.
struct ControllerPortEvent {
    uint64_t cycle=0;
    uint32_t pc=0;
    uint8_t dsp=0,offset=0,value=0,flags=0; // bit 0 write, bit 1 accepted
};
struct ControllerSlice {uint64_t begin=0,end=0;uint32_t first=0,count=0;};
struct ControllerTape {
    static constexpr size_t MaxEvents=3000000,MaxSlices=1000000;
    uint64_t checkpoint=0,beginCycle=0,endCycle=0;
    bool complete=false;
    std::vector<ControllerPortEvent> events;
    std::vector<ControllerSlice> slices;
    void validate() const;
    uint64_t digest() const noexcept;
    void save(const std::string& path) const;
    static std::shared_ptr<ControllerTape> load(const std::string& path);
};
class ControllerRecorder final {
public:
    ControllerRecorder(uint64_t checkpoint,uint64_t begin,size_t eventLimit=ControllerTape::MaxEvents,size_t sliceLimit=ControllerTape::MaxSlices);
    void beginSlice(uint64_t cycle) noexcept;
    void port(uint64_t cycle,uint32_t pc,unsigned dsp,unsigned offset,uint8_t value,bool write,bool accepted) noexcept;
    void endSlice(uint64_t cycle) noexcept;
    std::shared_ptr<const ControllerTape> finish();
private:
    std::shared_ptr<ControllerTape> tape;
    size_t eventLimit,sliceLimit;
    bool active=false,overflow=false,sealed=false;
    ControllerSlice pending;
};
}
