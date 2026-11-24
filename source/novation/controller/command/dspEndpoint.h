#pragma once

#include <cstdint>

namespace novation::controller::command
{
    enum class HdiRegister : uint8_t
    {
        Control = 0, Command = 2, Status = 4, Vector = 6, High = 10, Middle = 12, Low = 14
    };

    // This view only accesses the physical host registers. The DSP/audio
    // coordinator controls when the hardware may advance between accesses.
    class DspEndpoint final
    {
    public:
        template<class Dsp>
        static DspEndpoint bind(Dsp& dsp) noexcept
        {
            return {&dsp,
                [](void* context, HdiRegister reg) { return static_cast<Dsp*>(context)->hostRead(uint32_t(reg)); },
                [](void* context, HdiRegister reg, uint8_t value)
                {
                    return static_cast<Dsp*>(context)->hostWrite(uint32_t(reg), value);
                }};
        }

        uint8_t read(HdiRegister reg) const { return read_(context_, reg); }
        bool write(HdiRegister reg, uint8_t value) const { return write_(context_, reg, value); }

    private:
        using Read = uint8_t (*)(void*, HdiRegister);
        using Write = bool (*)(void*, HdiRegister, uint8_t);
        DspEndpoint(void* context, Read read, Write write) noexcept : context_(context), read_(read), write_(write) {}

        void* context_;
        Read read_;
        Write write_;
    };
}
