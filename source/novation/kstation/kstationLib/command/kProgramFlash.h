#pragma once

#include "kStationCommandBackend.h"

namespace novation::controller::command {

// This view shares the user image with program lookup. It cannot address the
// firmware and never replaces or reallocates the backing storage.
class KProgramFlash final {
public:
    static constexpr size_t UserOffset=0x1fc00, Globals=0x1ff00, GlobalBytes=256;

    explicit KProgramFlash(std::vector<uint8_t>& flash) noexcept : flash_(flash) {}

    const uint8_t* globalPage() const noexcept { return flash_.data()+Globals; }

    bool storeProgram(ProgramSelection selection,const uint8_t* data,bool pair) noexcept {
        const auto offset=KStationCommandBackend::programOffset(selection);
        const auto size=KStationCommandBackend::ProgramBytes*(pair?2:1);
        if (!writable() || !offset || *offset+size>flash_.size() || (pair && (selection.program&1)))
            return false;
        std::copy_n(data,size,flash_.data()+*offset);
        return true;
    }

    bool storeGlobals(const uint8_t* data) noexcept {
        if (!writable() || !KStationCommandBackend::programOffset({data[0],data[1]}) || data[2]>1 || data[4]>=16)
            return false;
        std::copy_n(data,GlobalBytes,flash_.data()+Globals);
        return true;
    }

private:
    bool writable() const noexcept { return flash_.size()>=Globals+GlobalBytes && globalPage()[2]==1; }

    std::vector<uint8_t>& flash_;
};

}
