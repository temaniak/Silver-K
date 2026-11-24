#pragma once
#include <cstdint>
namespace novation::controller::command {
enum class KWriteProgress:uint8_t { Ready,WaitingForTransfer,Complete,Fault };
}
