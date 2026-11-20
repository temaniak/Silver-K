#pragma once

#include <array>
#include <cstddef>

namespace novation::controller::command {

// Convert the DAC lanes before they enter the host-rate output queue.
template<size_t Channels> class OutputResampler {
public:
    template<class Render>
    void prime(Render render) {
        phase = 0;
        for (auto& sample : history)
            load(sample, render());
    }

    template<class Render>
    std::array<float, Channels> next(double nativePerHost, Render render) {
        std::array<float, Channels> output{};
        for (size_t channel = 0; channel < Channels; ++channel) {
            const auto a = history[0][channel], b = history[1][channel];
            const auto c = history[2][channel], d = history[3][channel];
            output[channel] = float(b + 0.5 * phase *
                (c - a + phase * (2*a - 5*b + 4*c - d + phase * (3*(b - c) + d - a))));
        }
        phase += nativePerHost;
        while (phase >= 1) {
            phase -= 1;
            history[0] = history[1];
            history[1] = history[2];
            history[2] = history[3];
            load(history[3], render(phase / nativePerHost));
        }
        return output;
    }

private:
    template<class Frame>
    static void load(std::array<double, Channels>& sample, const Frame& input) noexcept {
        for (size_t channel = 0; channel < Channels; ++channel)
            sample[channel] = double(input[channel]) / 8388608;
    }

    double phase = 0;
    std::array<std::array<double, Channels>, 4> history{};
};

}
