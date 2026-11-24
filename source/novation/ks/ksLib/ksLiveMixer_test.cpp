#include "command/ksLiveMixer.h"

#include <iostream>
#include <stdexcept>

using namespace novation::controller::command;

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
}

int main() {
    try {
        const int owner = 0;
        KSMusicalState state({&owner, 1}, KSLayout::Performance);
        const auto slot = KSStorageSlot::Multi2;
        check(state.loadProgram(slot, {1, 0}, {64, 64}), "Load active part");
        const auto program = state.part(slot)->revisions.program;
        const auto left = uint16_t(KSPartOutputAddress + KSGlobalPartStride + 1);
        const auto right = uint16_t(left + 1);
        KSLiveMixer mixer;

        mixer.route(slot, KSPart::Two, program, 1, 96, 95, {100});
        check(!mixer.due({100}) && mixer.due({123}), "Schedule the first ramp word");
        KSRefreshPacket first;
        mixer.append(first, state, {123});
        const auto from = uint32_t(96) << 16;
        const auto target = uint32_t(95) << 16;
        const auto firstWord = from - 4096;
        check(first.size() == 2 && first.at(0)->address == left && first.at(1)->address == right &&
              first.at(0)->words[0] == firstWord && first.at(1)->words[0] == firstWord,
              "Both Aux channels interpolate below one MIDI step");
        check(mixer.preserve(Endpoint::A, left, target, state) == firstWord,
              "A concurrent refresh must retain the in-flight level");
        check(mixer.preserve(Endpoint::A, uint16_t(left - 1), 2, state) == 2,
              "The ramp does not alter the output selector");

        mixer.route(slot, KSPart::Two, program, 1, 95, 97, {124});
        KSRefreshPacket reversed;
        mixer.append(reversed, state, {146});
        check(reversed.size() == 2 && reversed.at(0)->words[0] > firstWord &&
              reversed.at(0)->words[0] < (uint32_t(97) << 16), "Retarget from the current level");
        KSRefreshPacket settled;
        mixer.append(settled, state, {600});
        const auto finalWord = uint32_t(97) << 16;
        check(settled.size() == 2 && settled.at(0)->words[0] == finalWord &&
              settled.at(1)->words[0] == finalWord && !mixer.active(), "Settle at the exact saved value");

        mixer.route(slot, KSPart::Two, program, 3, 97, 0, {650});
        KSRefreshPacket mono;
        mixer.append(mono, state, {680});
        check(mono.size() == 1 && mono.at(0)->address == right, "Mono routing uses only its assigned channel");
        check(state.loadProgram(slot, {1, 1}, {64, 64}), "Replace the part program");
        KSRefreshPacket obsolete;
        mixer.append(obsolete, state, {703});
        check(obsolete.size() == 0 && !mixer.active(), "A ramp cannot write to a replaced program");

        std::cout << "PASS: KS mixer substeps, retargeting, route isolation and program revision\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
