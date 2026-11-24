#pragma once

#include "jucePluginLib/patchdb/patch.h"

#include <algorithm>

namespace pluginLib::patchDB
{
    // Adapted from Gearmulator's patchmanager/state.cpp at f9a8e80d.
    // The editor supplies the displayed order, including its filters and sort.
    class PresetSelection
    {
    public:
        void setSelectedPatch(unsigned part, const PatchPtr& patch)
        {
            auto& slot = selected.at(part);
            slot = {};
            if (patch && (slot.key.source = patch->source.lock()))
            {
                slot.key.hash = patch->hash;
                slot.key.program = patch->program;
                slot.bank = patch->bank;
            }
        }

        int indexOf(unsigned part, const std::vector<PatchPtr>& patches) const
        {
            const auto& slot = selected.at(part);
            if (!slot.key.isValid())
                return -1;

            const auto match = std::find_if(patches.begin(), patches.end(), [&](const PatchPtr& patch) {
                return patch->bank == slot.bank && *patch == slot.key;
            });
            return match == patches.end() ? -1 : int(match - patches.begin());
        }

        static int getNeighbourPreset(int current, int count, int direction) noexcept
        {
            if (count == 0)
                return -1;
            if (current < 0)
                return direction < 0 ? count - 1 : 0;

            auto next = current + (direction < 0 ? -1 : 1);
            if (next < 0)
                next += count;
            if (next >= count)
                next -= count;
            return next;
        }

    private:
        struct Slot
        {
            PatchKey key;
            uint32_t bank = g_invalidBank;
        };
        std::array<Slot, 16> selected{};
    };
}
