#pragma once
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ui_request_equivalence_test {
using UI = kstation::ControllerUiServices;
static unsigned long long none = 0, positive = 0;
static std::array<unsigned long long, 256> owners{};
inline void check(const UI& ui) {
    std::array<unsigned char, sizeof(UI)> before{};
    std::memcpy(before.data(), &ui, sizeof(ui));
    const bool actual = ui.hasExternalRequest();
    if (std::memcmp(before.data(), &ui, sizeof(ui))) {
        std::fputs("hasExternalRequest mutated UI state\n", stderr); std::abort();
    }
    const auto request = ui.externalRequest();
    if (std::memcmp(before.data(), &ui, sizeof(ui))) {
        std::fputs("externalRequest mutated UI state\n", stderr); std::abort();
    }
    if (actual != (request.owner != UI::Owner::None)) {
        std::fputs("UI request predicate mismatch\n", stderr); std::abort();
    }
    if (actual) { ++positive; ++owners[static_cast<unsigned>(request.owner)]; } else ++none;
}
struct Report { ~Report() {
    std::fprintf(stderr, "UI predicate: none=%llu positive=%llu owners=", none, positive);
    for (unsigned i=0; i<owners.size(); ++i) if (owners[i]) std::fprintf(stderr, "%u:%llu,", i, owners[i]);
    std::fputc('\n', stderr);
}};
static Report report;
}
