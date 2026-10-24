#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
namespace ks { namespace tests {
// Test-only diagnostic locations of independently H8-verified native graphs.
// Production recording and replay retain their original strict PC guards.
// Callers must compare every cycle, port, value, flag and slice separately.
inline bool nativeDspTraceLocationMatches(uint32_t original,uint32_t native){
 constexpr std::array<uint32_t,13>addressAndIncrement{{0x2417,0x2425,0x243d,0x2447,0x244f,0x2457,0x245f,0x246d,0x2485,0x24d9,0x24ff,0x2507,0x250f}};
 constexpr std::array<uint32_t,5>word{{0x2521,0x252f,0x2547,0x254f,0x2557}};
 const bool transport=std::find(addressAndIncrement.begin(),addressAndIncrement.end(),original)!=addressAndIncrement.end();
 const bool nonincrement=std::find(word.begin(),word.end(),original)!=word.end();
 switch(native){
 case 0x47d6:case 0x4eb0:case 0x7354:case 0x96d6:return transport;
 case 0xb554:case 0x9662:return transport||nonincrement;
 case 0x7de4:return nonincrement;
 default:return false;
 }
}
}}
