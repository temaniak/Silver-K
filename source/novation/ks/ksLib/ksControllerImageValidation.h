#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
namespace ks::controllerImage {
inline constexpr std::size_t ImageSize=87360;
inline constexpr uint64_t ImageIdentity=0xbd1afefba93cb7b9ull;
struct Range {uint32_t first,last;};
inline constexpr std::array<Range,3> ExecutableRanges{{{0x170,0x172},{0x490,0x2a0c},{0x2a18,0xb8e8}}};
// The Board may append its transport checksum. Identity covers the original
// admitted image only, exactly as the existing native constructor does.
inline void verifyRom(const uint8_t* image,std::size_t bytes){
    if(!image||bytes<ImageSize)throw std::runtime_error("Native KS runtime requires verified OS 2.1.02");
    uint64_t hash=14695981039346656037ull;
    for(std::size_t n=0;n<ImageSize;++n){hash^=image[n];hash*=1099511628211ull;}
    if(hash!=ImageIdentity)throw std::runtime_error("Native KS runtime requires verified OS 2.1.02");
}
// Caller has admitted the immutable ROM. Check the actual executable SRAM
// after relocation; writable tables and initialization data are not code.
inline void verifyOverlay(const uint8_t* rom,std::size_t romBytes,const uint8_t* overlay,std::size_t overlayBytes){
    if(!rom||!overlay||romBytes<ExecutableRanges.back().last||overlayBytes<ExecutableRanges.back().last)
        throw std::runtime_error("Native KS handoff code image span is invalid");
    for(const auto range:ExecutableRanges)
        for(uint32_t offset=range.first;offset<range.last;++offset)
            if(overlay[offset]!=rom[offset])throw std::runtime_error("Native KS handoff code image differs from verified ROM");
}
// Preserve the existing Board's direct and SRAM-alias write guard. Addresses
// are physical caller inputs; signed/tagged normalization belongs to the bus.
inline constexpr bool immutableCodeAddress(uint32_t address){
    const auto offset=address>=0xa00000&&address<0xc00000?address-0xa00000:address;
    for(const auto range:ExecutableRanges)if(offset>=range.first&&offset<range.last)return true;
    return false;
}
}
