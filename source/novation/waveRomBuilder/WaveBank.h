#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace waveRom
{
constexpr size_t bankSize = 129024;
struct Slot { int index, samples, bits; size_t offset; const char* name; bool tonal() const { return index < 17; } size_t bytes() const { return size_t(samples * bits / 8); } };
const std::array<Slot, 24>& slots();
struct Selection { double begin = 0, end = 0, confidence = 0; bool valid(size_t n) const; };
struct Processing { bool normalize = true; double peakDb = -1, fadeInMs = 0, fadeOutMs = 8; };
Selection findCycle(const std::vector<float>&, double rate);
Selection findTransient(const std::vector<float>&, double rate);
std::vector<float> prepare(const std::vector<float>&, double rate, Selection, const Slot&, Processing);
std::vector<float> resample(const std::vector<float>&, size_t count, bool periodic);
double seamError(const std::vector<float>&);
double noteHz(int midiNote);
std::string noteName(int midiNote);

// PCM in the UI has the polarity heard through the KS output path. The ROM
// stores its inverse, as in tools/build_ks_wave_rom.py. No firmware is needed.
class Bank
{
public:
    Bank();
    void load(const std::vector<uint8_t>&);
    const std::vector<uint8_t>& bytes() const { return raw; }
    std::vector<float> decode(size_t slot) const;
    void put(size_t slot, const std::vector<float>&);
    bool placeholder() const { return zeroPlaceholder; }
private:
    std::vector<uint8_t> raw;
    bool zeroPlaceholder = false;
};

struct PreviewBuffer { std::vector<float> pcm; double step = 1; bool loop = false; };
std::unique_ptr<PreviewBuffer> tonalPreview(const std::vector<float>&, int note, double deviceRate);
std::unique_ptr<PreviewBuffer> samplePreview(const std::vector<float>&, double sourceRate, int note, double deviceRate, bool loop);

// Single GUI producer / single audio consumer. Retired buffers are reclaimed
// by the GUI, never by the callback. All allocation/filtering precedes publish.
class PreviewVoice
{
public:
    ~PreviewVoice(); // Audio must have been stopped by the owner first.
    bool publish(std::unique_ptr<PreviewBuffer>);
    void stop() { stopRequested.store(true, std::memory_order_release); }
    bool playing() const { return active.load(std::memory_order_relaxed); }
    void render(float* const* outputs, int channels, int count, float gain) noexcept;
    void collect();
private:
    template<size_t N> struct Queue {
        std::array<PreviewBuffer*, N> data{};
        std::atomic<size_t> read{0}, write{0};
        bool push(PreviewBuffer* p) noexcept { auto w=write.load(std::memory_order_relaxed); auto next=(w+1)%N; if(next==read.load(std::memory_order_acquire))return false; data[w]=p; write.store(next,std::memory_order_release);return true; }
        PreviewBuffer* pop() noexcept { auto r=read.load(std::memory_order_relaxed);if(r==write.load(std::memory_order_acquire))return nullptr;auto* p=data[r];read.store((r+1)%N,std::memory_order_release);return p; }
    };
    Queue<8> incoming;
    Queue<32> retired;
    PreviewBuffer* current = nullptr;
    double position = 0;
    unsigned ramp = 0;
    unsigned release = 0;
    float last = 0, carry = 0;
    std::atomic<bool> stopRequested{false}, active{false};
};
}
