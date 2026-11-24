#pragma once

#include "dspWordTransport.h"

#include <vector>

namespace novation::controller::command {

struct DspBootImage {
    const char* name;
    Model model;
    Endpoint endpoint;
    size_t programOffset;
    size_t programWords;
    const char* programHash;
    uint16_t tableAddress;
    const char* tableHash;
};

inline constexpr DspBootImage KBootImage{"K", Model::K, Endpoint::A, 0x84f2, 2483,
    "47a758690529a34e5a3d483067399f6b227e380a7f34f61b8c3d1a09a1c89ce3", 0x8200,
    "cc410105c590722f771fe067c124608d1e517c09fd398a5551271b7fa10492b9"};
inline constexpr DspBootImage KsABootImage{"KS_A", Model::KS, Endpoint::A, 0xbad6, 1869,
    "f99e527bc5972029ab6ac86f65dd5764400c8525e3a2b906988f40ec3965b1ed", 0x8600,
    "ae8915e32dcb40fdc31125bafff48fcc98284fd998f4a1cc0cc0ebb1e50a5c8c"};
inline constexpr DspBootImage KsBBootImage{"KS_B", Model::KS, Endpoint::B, 0xd0c3, 2838,
    "b24a446b478bc29c05719e0ace001d725b1af9e2969ffefefe68a7cfd67988bc", 0x8600, KsABootImage.tableHash};

std::vector<uint32_t> readDspBootProgram(const DspBootImage&, const std::vector<uint8_t>& decodedOs);
std::vector<uint32_t> readDspBootTable(const DspBootImage&, const std::vector<uint8_t>& decodedOs);

struct DspBootPort {
    void* context;
    DspEndpoint hdi;
    void (*advance)(void*, BusCycle);
    BusCycle (*bus)(void*);
    NativeFrame (*frame)(void*);
    bool (*verifyProgram)(void*, const uint32_t*, size_t, uint32_t);
};

// Exclusive port owner during cold preparation, before publishing the musical
// session. On failure discard the physical instance before trying again.
class DspBootstrap final {
public:
    DspBootstrap(DspBootPort, const DspBootImage&, uint64_t busSpacing);
    void start(const std::vector<uint32_t>& program, const std::vector<uint32_t>& table);
    void write(uint16_t address, const uint32_t* words, size_t size);
    void waitResponse(uint16_t address, uint32_t mask, uint32_t expected,
                      uint64_t maxBusCycles, uint64_t pollSpacing = 128);

private:
    void check(bool, const char* phase) const;
    void advance(uint64_t cycles);
    void waitFlag(uint8_t mask, bool set, uint64_t budget);
    uint32_t transact(DspAddress, const uint32_t*, size_t);

    DspBootPort port_;
    const DspBootImage& image_;
    DspWordTransport transport_;
    CommandSession session_;
    Session token_{};
    BusCycle bus_{};
    HdiWordTiming timing_;
    uint64_t operation_ = 0;
    bool started_ = false;
};

}
