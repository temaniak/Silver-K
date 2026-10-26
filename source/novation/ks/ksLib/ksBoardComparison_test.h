#pragma once
#include "ksBoard.h"
#include "ksControllerState.h"
#include "ksLiveMidiController.h"
#include "ksRuntimeController.h"
namespace ks::test {
// Explicit legacy test access only. New native scenarios use
// Board::controllerResources() and real typed BoardNativeOwner metrics.
ControllerCpu&comparisonCpu(Board&);
// Retained architecture-based cold oracle, before any actual advance phase.
void startLegacyStartupForTests(Board&);
std::unique_ptr<Board>makeLegacyBoard(const std::vector<uint8_t>&os,const std::vector<uint8_t>&bank,const std::vector<uint8_t>&storage={},bool isolateParts=false,bool legacyStartup=false);
const RuntimeController*legacyRuntime(const Board&);
const LiveMidiController*legacyMidi(const Board&);
std::array<uint64_t,4>legacyBatchMetrics(const Board&);
uint64_t legacyControllerCheckpoint(const Board&);
}
