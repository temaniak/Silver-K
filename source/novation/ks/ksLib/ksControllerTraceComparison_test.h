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
 // Exact observed locations of the complete native graph, H8-verified.
 if(native==0x5db2)return original==0x2417||original==0x2425||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f;
 // Exact observed locations of the complete native graph, H8-verified.
 if(native==0x5edc)return original==0x2417||original==0x2425||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f;
 if(native==0x5efc)return original==0x2417||original==0x2425||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f;
 // Exact observed locations of the complete native graph, H8-verified.
 if(native==0x472c)return original==0x2417||original==0x2425||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f;
 // Exact observed locations of the complete native graph, H8-verified.
 if(native==0x7666)return original==0x2417||original==0x2425||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f||original==0x2521||original==0x252f||original==0x2547||original==0x254f||original==0x2557||original==0x256f||original==0x257d||original==0x2595||original==0x259f||original==0x25a7||original==0x25af||original==0x25b7||original==0x25c5||original==0x25dd||original==0x25e5||original==0x25ed||original==0x25f5;
 // Exact observed locations of the complete native graph, H8-verified.
 if(native==0x267a)return original==0x2417||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f;
 // Exact observed locations of the complete native graph, H8-verified.
 if(native==0x737a)return original==0x2417||original==0x2425||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f||original==0x2521||original==0x252f||original==0x2547||original==0x254f||original==0x2557;
 // Exact observed locations of the complete native graph, H8-verified.
 if(native==0x437a)return original==0x2417||original==0x2425||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f||original==0x2521||original==0x252f||original==0x2547||original==0x254f||original==0x2557;
 // Exact observed locations of the complete native graph, H8-verified.
 if(native==0x4494)return original==0x2417||original==0x2425||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f||original==0x2521||original==0x252f||original==0x2547||original==0x254f||original==0x2557;
 // Exact observed Feedback/Parts diagnostic sites; all real event bytes stay strict.
 if(native==0x9f5a)return original==0x256f||original==0x2595||original==0x259f||original==0x25a7||original==0x25af||original==0x25b7||original==0x25c5||original==0x25dd||original==0x25e5||original==0x25ed||original==0x25f5;
 if(native==0xa12c)return original==0x2417||original==0x2425||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f||original==0x256f||original==0x257d||original==0x2595||original==0x259f||original==0x25a7||original==0x25af||original==0x25b7||original==0x25c5||original==0x25dd||original==0x25e5||original==0x25ed||original==0x25f5;
 // Exact observed NoteProgram/Release diagnostic sites; all real event bytes stay strict.
 if(native==0x3642)return original==0x2417||original==0x2425||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f||original==0x2521||original==0x252f||original==0x2547||original==0x254f||original==0x2557||original==0x256f||original==0x2595||original==0x259f||original==0x25a7||original==0x25af||original==0x25b7||original==0x25c5||original==0x25dd||original==0x25e5||original==0x25ed||original==0x25f5;
 if(native==0x92c0)return original==0x2417||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f;
 // Exact observed MidiControl/PartNote diagnostic sites; all real event bytes stay strict.
 if(native==0x33b8)return original==0x2417||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f;
 if(native==0x3426)return original==0x2417||original==0x2425||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f||original==0x2521||original==0x252f||original==0x2547||original==0x254f||original==0x2557||original==0x256f||original==0x2595||original==0x259f||original==0x25a7||original==0x25af||original==0x25b7||original==0x25c5||original==0x25dd||original==0x25e5||original==0x25ed||original==0x25f5;
 // Exact observed Aux BeginSysEx diagnostic sites; all real event bytes stay strict.
 if(native==0x35f4)return original==0x2417||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f;
 // Exact observed MIDI pump diagnostic sites; all actual event bytes remain strict.
 if(native==0x2a2a)return original==0x2417||original==0x2425||original==0x243d||original==0x2447||original==0x244f||original==0x2457||original==0x245f||original==0x246d||original==0x2485||original==0x24d9||original==0x24ff||original==0x2507||original==0x250f||original==0x2521||original==0x252f||original==0x2547||original==0x254f||original==0x2557||original==0x256f||original==0x2595||original==0x259f||original==0x25a7||original==0x25af||original==0x25b7||original==0x25c5||original==0x25dd||original==0x25e5||original==0x25ed||original==0x25f5;
 switch(native){
 case 0x47d6:case 0x4eb0:case 0x7354:case 0x96d6:return transport;
 case 0xb554:case 0x9662:return transport||nonincrement;
 case 0x7de4:return nonincrement;
 case 0x48be:case 0x4864:case 0x4924:case 0x62e6:case 0x633a:case 0x6382:return transport&&original!=0x2425;
 default:return false;
 }
}
}}
