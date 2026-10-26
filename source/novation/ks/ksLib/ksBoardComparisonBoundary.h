#pragma once
#include "ksControllerDeviceState.h"
#include "ksControllerTrace.h"
#include <memory>
#include <vector>
namespace ks {
class Board;
enum class BoardComparisonMode {Original,Hybrid,LegacyCompatibility};
// Interface only. Implementations, architectural state and reference selection
// live in the explicitly linked comparison target. Production registers none.
class BoardComparisonBackend {
public:
 virtual ~BoardComparisonBackend()=default;
 virtual ControllerDeviceState& resources()noexcept=0; // borrowed, never delete through this view
 virtual void initialize()=0;
 virtual void beginSlice(uint64_t begin,unsigned budget)=0;
 virtual uint64_t cycles()const=0;
 virtual uint64_t compatibilityCheckpoint()const=0;
 virtual void advanceNextPhase()=0;
 virtual void tickAfterPhase(uint64_t previous)=0;
 virtual bool mainEntered()const=0;
 virtual bool ready()const=0;
 virtual bool nativeMapping()const=0;
 virtual bool protectedCode()const=0;
 virtual bool allowsLiveInput()const=0;
 virtual uint32_t diagnosticLocation()const=0;
 virtual const char*name()const=0;
 virtual void select(BoardComparisonMode)=0;
 virtual void resetMetrics()=0;
 virtual void enableDelegationProfile()=0;
 virtual uint64_t interpretedSteps()const=0;
 virtual uint64_t stepsSinceHandoff()const=0;
 virtual uint64_t checkedPorts()const=0;
 virtual void startRecorded(std::shared_ptr<const ControllerTape>,uint64_t checkpoint)=0;
};
using BoardComparisonFactory=std::unique_ptr<BoardComparisonBackend>(*)(Board*,const std::vector<uint8_t>&);
// Register once before creating any Boards. Only comparison targets call this.
void setBoardComparisonFactoryForTests(BoardComparisonFactory);
bool boardComparisonAvailable()noexcept;
std::unique_ptr<BoardComparisonBackend> createBoardComparison(Board*,const std::vector<uint8_t>&);
}
