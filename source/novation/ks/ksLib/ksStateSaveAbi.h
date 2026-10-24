#pragma once
#include <array>
#include <cstdint>
namespace ks {
// Completion-only external ABI value. Pure native jobs never construct or
// retain it while work is active.
struct StateSaveCallerAbi {std::array<uint32_t,7> values{};uint8_t flags=0;};
}
